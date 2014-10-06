#include <stdio.h>
#include <stdint.h>

#include <elf.h>

#include "main.h"
#include "endian.h"
#include "elf.h"

namespace pkgdepdb {

Elf::Elf()
: ei_class_   (0),
  ei_osabi_   (0),
  rpath_set_  (false),
  runpath_set_(false),
  owner_      (nullptr)
{}

Elf::Elf(const Elf& cp)
: dirname_    (cp.dirname_),
  basename_   (cp.basename_),
  ei_class_   (cp.ei_class_),
  ei_data_    (cp.ei_data_),
  ei_osabi_   (cp.ei_osabi_),
  rpath_set_  (cp.rpath_set_),
  runpath_set_(cp.runpath_set_),
  rpath_      (cp.rpath_),
  runpath_    (cp.runpath_),
  needed_     (cp.needed_),
  req_found_  (cp.req_found_),
  req_missing_(cp.req_missing_),
  owner_      (cp.owner_)
{}

template<bool BE,
         typename HDR, typename SecHDR, typename ProgHDR, typename Dyn>
Elf* LoadElf(const char *data, size_t size, bool *waserror, const char *name,
             const Config &optconfig)
{
  uniq<Elf> object(new Elf);

  auto checksize =
    [size,name,&optconfig](ssize_t ioff, size_t sz, const char *msg)
  -> bool {
    size_t off = size_t(ioff);
    if (off + sz > size) {
      optconfig.Log(Error, "%s: unexpected end of file in ELF file,"
                           " offset %lu (file size %lu): %s\n",
                    name, (unsigned long)(off + sz), (unsigned long)size, msg);
      return false;
    }
    return true;
  };

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
  HDR  *hdr     = (HDR*)(data);
  auto  shnum   = Eswap<BE>(hdr->e_shnum);
  auto  phnum   = Eswap<BE>(hdr->e_phnum);
  auto  e_shoff = Eswap<BE>(hdr->e_shoff);
  auto  e_phoff = Eswap<BE>(hdr->e_phoff);

  if (!checksize((ssize_t)e_shoff, sizeof(SecHDR),
                 "looking for section headers") ||
      !checksize((ssize_t)e_phoff, sizeof(ProgHDR),
                 "looking for program headers"))
  {
    return 0;
  }

  auto prog_start = reinterpret_cast<const ProgHDR*>(data + e_phoff);
  // TODO: check auxiliary data when phnum == PX_XNUM
  if (phnum == PN_XNUM)
    optconfig.Log(Error, "%s: TODO: program header count too large, skipping",
                  name);
  else {
    if (!checksize((char*)(prog_start + phnum-1) - data, sizeof(*prog_start),
                   "program header array"))
    {
      return 0;
    }
    auto ph = prog_start;
    for (; ph != prog_start + phnum; ++ph) {
      if (Eswap<BE>(ph->p_type) == PT_INTERP)
        break;
    }
    if (ph != prog_start + phnum) {
      // this one has an interpreter request
      object->interpreter_set_ = true;
      object->interpreter_ = data + Eswap<BE>(ph->p_offset);
    }
  }

  auto sec_start = reinterpret_cast<const SecHDR*>(data + e_shoff);
  if (!checksize((char*)(sec_start + shnum-1) - data, sizeof(*sec_start),
                 "section header array"))
  {
    return 0;
  }
  auto findsec = [=](function<bool(const SecHDR*)> cond) -> const SecHDR* {
    for (size_t i = 0; i != shnum; ++i) {
      auto s = sec_start + i;
      if (cond(s))
        return s;
    }
    return 0;
  };

  auto dynhdr = findsec(
    [](const SecHDR *hdr) {
      return Eswap<BE>(hdr->sh_type) == SHT_DYNAMIC;
    });
  if (!dynhdr) {
    optconfig.Log(Debug,
                  "%s: not a dynamic executable, no .dynamic section found\n",
                  name);
    *waserror = false;
    return 0;
  }

  if (Eswap<BE>(dynhdr->sh_entsize) != sizeof(Dyn)) {
    optconfig.Log(Error, "%s: invalid entsize for dynamic section\n", name);
    return 0;
  }

  Dyn   *dyn_start = (Dyn*)(data + Eswap<BE>(dynhdr->sh_offset));
#pragma clang diagnostic pop
  size_t dyncount = Eswap<BE>(dynhdr->sh_size) / sizeof(Dyn);

  if (!checksize((char*)(dyn_start + dyncount-1)-data, sizeof(Dyn),
                 ".dynamic entries"))
  {
    return 0;
  }

  Dyn    *dynstr   = 0;
  size_t  strsz    = 0;

  for (size_t i = 0; i != dyncount; ++i) {
    Dyn *dyn = dyn_start + i;
    auto d_tag = Eswap<BE>(dyn->d_tag);
    if (d_tag == DT_STRTAB)
      dynstr = dyn;
    if (d_tag == DT_STRSZ)
      strsz  = Eswap<BE>(dyn->d_un.d_val);
  }
  if (!dynstr) {
    optconfig.Log(Error, "%s: No DT_STRTAB\n", name);
    return 0;
  }
  if (!strsz) {
    optconfig.Log(Error, "%s: No DT_STRSZ\n", name);
    return 0;
  }

  // find string section with the offset from DT_STRTAB
  auto dynstrsec = findsec([dynstr](const SecHDR *hdr) {
    return Eswap<BE>(hdr->sh_type) == SHT_STRTAB &&
           Eswap<BE>(hdr->sh_addr) == Eswap<BE>(dynstr->d_un.d_ptr);
  });
  if (!dynstrsec) {
    optconfig.Log(Error, "%s: Found no .dynstr section\n", name);
    return 0;
  }

  auto dynstr_at = ssize_t(Eswap<BE>(dynstrsec->sh_offset));
  if (!checksize(dynstr_at, strsz, "looking for .dynstr section"))
    return 0;

  auto get_string = [=,&optconfig](size_t off) -> const char* {
    // range check
    if (off >= strsz) {
      optconfig.Log(Error, "%s: out of bounds string entry\n", name);
      return 0;
    }
    const char *str = data + dynstr_at + off;
    const char *end = data + dynstr_at + strsz;
    const char *at  = str;
    while (*str && str != end) ++str;
    if (*str && str == end) {
      // missing terminating nul byte
      optconfig.Log(Error, "%s: unterminated string in string table\n", name);
      return 0;
    }
    return at;
  };

  for (size_t i = 0; i != dyncount; ++i) {
    Dyn        *dyn = dyn_start + i;
    const char *str;
    auto d_tag = Eswap<BE>(dyn->d_tag);
    auto d_ptr = Eswap<BE>(dyn->d_un.d_ptr);
    switch (d_tag) {
      case DT_NEEDED:
        if (! (str = get_string(d_ptr)) )
          return 0;
        object->needed_.push_back(str);
        break;
      case DT_RPATH:
        object->rpath_set_ = true;
        if (! (str = get_string(d_ptr)) )
          return 0;
        object->rpath_ = str;
        break;
      case DT_RUNPATH:
        object->runpath_set_ = true;
        if (! (str = get_string(d_ptr)) )
          return 0;
        object->runpath_ = str;
        break;
      default:
        break;
    }
  }

  *waserror = false;
  return object.release();
}

static const auto
LoadElf32LE = &LoadElf<false, Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr, Elf32_Dyn>;
static const auto
LoadElf32BE = &LoadElf<true,  Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr, Elf32_Dyn>;
static const auto
LoadElf64LE = &LoadElf<false, Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr, Elf64_Dyn>;
static const auto
LoadElf64BE = &LoadElf<true,  Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr, Elf64_Dyn>;

Elf* Elf::Open(const char *data, size_t size, bool *waserror, const char *name,
               const Config &optconfig)
{
  *waserror = false;
  unsigned char *elf_ident = (unsigned char*)data;
  if (elf_ident[EI_MAG0] != ELFMAG0 ||
      elf_ident[EI_MAG1] != ELFMAG1 ||
      elf_ident[EI_MAG2] != ELFMAG2 ||
      elf_ident[EI_MAG3] != ELFMAG3)
  {
    optconfig.Log(Debug, "%s: not an ELF file\n", name);
    return 0;
  }

  *waserror = true;
  unsigned char ei_class   = elf_ident[EI_CLASS];
  unsigned char ei_data    = elf_ident[EI_DATA];
  unsigned char ei_version = elf_ident[EI_VERSION];
  unsigned char ei_osabi   = elf_ident[EI_OSABI];

  if (ei_version != EV_CURRENT) {
    optconfig.Log(Error, "%s: invalid ELF version: %u\n",
                  name, (unsigned)ei_version);
    return 0;
  }

  if (ei_data != ELFDATA2LSB &&
      ei_data != ELFDATA2MSB)
  {
    optconfig.Log(Error,
                  "%s: unrecognized ELF data type: %u"
                  " (neither ELFDATA2LSB nor ELFDATA2MSB)\n",
                  name, (unsigned)ei_data);
    return 0;
  }

  if (ei_osabi != ELFOSABI_FREEBSD &&
      ei_osabi != ELFOSABI_LINUX   &&
      ei_osabi != ELFOSABI_NONE)
  {
    optconfig.Log(Warn, "%s: osabi not recognized: %u\n",
                  name, (unsigned)ei_osabi);
  }

  Elf *e = 0;
  if (ei_class == ELFCLASS32) {
    if (ei_data == ELFDATA2LSB)
      e = LoadElf32LE(data, size, waserror, name, optconfig);
    else
      e = LoadElf32BE(data, size, waserror, name, optconfig);
  } else if (ei_class == ELFCLASS64) {
    if (ei_data == ELFDATA2LSB)
      e = LoadElf64LE(data, size, waserror, name, optconfig);
    else
      e = LoadElf64BE(data, size, waserror, name, optconfig);
  }
  if (!e)
    return 0;
  e->ei_class_ = ei_class;
  e->ei_data_  = ei_data;
  e->ei_osabi_ = ei_osabi;
  return e;
}

void fixpath(string& path) {
  size_t at = 0;
  do {
    at = path.find("//", at);
    if (at == string::npos)
      break;
    path.erase(at, 1);
  } while(true);

  at = 0;
  do {
    at = path.find("/./", at);
    if (at == string::npos)
      break;
    path.erase(at, 2);
  } while(true);

  while (path.length() >= 3 &&
         path[path.length()-1] == '.' &&
         path[path.length()-2] == '.' &&
         path[path.length()-3] == '/')
  {
    at = path.find_last_of('/', path.length()-4);
    path.erase(at);
  }

  at = 0;
  do {
    at = path.find("/../", at);
    if (at == string::npos)
      break;
    if (!at) { // beginning
      path = move(path.substr(3));
      continue;
    }
    if (at + 4 == path.length()) { //end
      path.erase(at, string::npos);
      continue;
    }
    //otherwise cut out the previous path:
    size_t prev = path.rfind('/', at-1);
    if (prev == string::npos) {
      // no previous dir
      // eg with ../../ so just skip this path
      ++at;
      continue;
    }
    path.erase(prev, at+3-prev);
  } while(true);

  while (path.length() > 1 && path[path.length()-1] == '/') {
    at = path.find_last_not_of('/');
    if (at > 0 && at != string::npos)
      path.erase(at+1);
  }
}

void fixpathlist(string& list) {
  string old(move(list));
  list = "";
  size_t at = 0;
  size_t to = old.find_first_of(':', 0);
  while (to != string::npos) {
    string sub(old.substr(at, to-at));
    fixpath(sub);
    list.append(move(sub));
    list.append(1, ':');
    at = to+1;
    to = old.find_first_of(':', at);
  }
  string sub(old.substr(at, to-at));
  fixpath(sub);
  list.append(move(sub));
}

static void replace_origin(string& path, const string& origin) {
  size_t at = 0;
  do {
    at = path.find("$ORIGIN", at);
    if (at == string::npos)
      break;
    path.replace(at, 7, origin);
    // skip this portion
    at += origin.length();
  } while (at < path.length());
  fixpathlist(path);
}

void Elf::SolvePaths(const string& origin) {
  if (rpath_set_)
    replace_origin(rpath_, origin);
  if (runpath_set_)
    replace_origin(runpath_, origin);
}

const char* Elf::classString() const {
  switch (ei_class_) {
    case ELFCLASSNONE: return "NONE";
    case ELFCLASS32:   return "ELF32";
    case ELFCLASS64:   return "ELF64";
    default: return "UNKNOWN";
  }
}

const char* Elf::dataString() const {
  switch (ei_data_) {
    case ELFDATANONE: return "NONE";
    case ELFDATA2LSB: return "2's complement, little-endian";
    case ELFDATA2MSB: return "2's complement, big-endian";
    default: return "UNKNOWN";
  }
}

const char* Elf::osabiString() const {
  switch (ei_osabi_) {
    case 0:    return "None";
    case 1:    return "HP-UX";
    case 2:    return "NetBSD";
    case 3:    return "Linux";
    case 4:    return "GNU/Hurd";
    case 5:    return "86Open common IA32 ABI";
    case 6:    return "Solaris";
    case 7:    return "AIX";
    case 8:    return "IRIX";
    case 9:    return "UNIX - FreeBSD";
    case 10:   return "UNIX - TRU64";
    case 11:   return "Novell Modesto";
    case 12:   return "OpenBSD";
    case 13:   return "Open VMS";
    case 14:   return "HP Non-Stop Kernel";
    case 15:   return "Amiga Research OS";
    case 97:   return "ARM";
    case 255:  return "Standalone (embedded) application";
    default: return "UNKNOWN";
  }
}

bool Elf::CanUse(const Elf &other, bool strict) const {
  if (ei_data_  != other.ei_data_ ||
      ei_class_ != other.ei_class_)
  {
    return false;
  }
  return (ei_osabi_ == other.ei_osabi_) ||
         (!strict && (!ei_osabi_ || !other.ei_osabi_));
}

} // ::pkgdepdb
