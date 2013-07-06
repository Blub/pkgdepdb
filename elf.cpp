#include <stdio.h>
#include <stdint.h>

#include <elf.h>

#include <sys/endian.h>

#include "main.h"
#include "endian.h"

Elf::Elf()
: refcount(0)
{
	ei_class =
	ei_osabi = 0;

	rpath_set   =
	runpath_set = false;
}

Elf::Elf(const Elf& cp)
: refcount(0),
  dirname(cp.dirname),
  basename(cp.basename),
  ei_class(cp.ei_class),
  ei_osabi(cp.ei_osabi),
  rpath_set(cp.rpath_set),
  runpath_set(cp.runpath_set),
  rpath(cp.rpath),
  runpath(cp.runpath),
  needed(cp.needed)
{
}

template<bool BE, typename HDR, typename SecHDR, typename Dyn>
Elf*
LoadElf(const char *data, size_t size, bool *waserror, const char *name)
{
	std::unique_ptr<Elf> object(new Elf);

	auto checksize = [size,name](size_t off, size_t sz, const char *msg) -> bool {
		if (off + sz > size) {
			log(Error, "%s: unexpected end of file in ELF file, offset %lu (file size %lu): %s\n",
			    name, (unsigned long)(off + sz), (unsigned long)size, msg);
			return false;
		}
		return true;
	};

	HDR  *hdr   = (HDR*)(data);
	auto  shnum = Eswap<BE>(hdr->e_shnum);
	auto  e_shoff = Eswap<BE>(hdr->e_shoff);

	if (!checksize(e_shoff, sizeof(SecHDR), "looking for section headers"))
		return 0;

	SecHDR *sec_start = (SecHDR*)(data + e_shoff);
	if (!checksize((char*)(sec_start + shnum-1) - data, sizeof(*sec_start), "section header array"))
		return 0;
	auto findsec = [=](std::function<bool(SecHDR*)> cond) -> SecHDR* {
		for (size_t i = 0; i != shnum; ++i) {
			SecHDR *s = sec_start + i;
			if (cond(s))
				return s;
		}
		return 0;
	};

	SecHDR *dynhdr = findsec([](SecHDR *hdr) { return Eswap<BE>(hdr->sh_type) == SHT_DYNAMIC; });
	if (!dynhdr) {
		log(Debug, "%s: not a dynamic executable, no .dynamic section found\n", name);
		*waserror = false;
		return 0;
	}

	if (Eswap<BE>(dynhdr->sh_entsize) != sizeof(Dyn)) {
		log(Error, "%s: invalid entsize for dynamic section\n", name);
		return 0;
	}

	Dyn   *dyn_start = (Dyn*)(data + Eswap<BE>(dynhdr->sh_offset));
	size_t dyncount = Eswap<BE>(dynhdr->sh_size) / sizeof(Dyn);

	if (!checksize((char*)(dyn_start + dyncount-1)-data, sizeof(Dyn), ".dynamic entries"))
		return 0;

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
		log(Error, "%s: No DT_STRTAB\n", name);
		return 0;
	}
	if (!strsz) {
		log(Error, "%s: No DT_STRSZ\n", name);
		return 0;
	}

	// find string section with the offset from DT_STRTAB
	SecHDR *dynstrsec = findsec([dynstr](SecHDR *hdr) {
		return Eswap<BE>(hdr->sh_type) == SHT_STRTAB &&
		       Eswap<BE>(hdr->sh_addr) == Eswap<BE>(dynstr->d_un.d_ptr);
	});
	if (!dynstrsec) {
		log(Error, "%s: Found no .dynstr section\n", name);
		return 0;
	}

	size_t dynstr_at = Eswap<BE>(dynstrsec->sh_offset);
	if (!checksize(dynstr_at, strsz, "looking for .dynstr section"))
		return 0;

	auto get_string = [=](size_t off) -> const char* {
		// range check
		if (off >= strsz) {
			log(Error, "%s: out of bounds string entry\n", name);
			return 0;
		}
		const char *str = data + dynstr_at + off;
		const char *end = data + dynstr_at + strsz;
		const char *at  = str;
		while (*str && str != end) ++str;
		if (*str && str == end) {
			// missing terminating nul byte
			log(Error, "%s: unterminated string in string table\n", name);
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
				object->needed.push_back(str);
				break;
			case DT_RPATH:
				object->rpath_set = true;
				if (! (str = get_string(d_ptr)) )
					return 0;
				object->rpath = str;
				break;
			case DT_RUNPATH:
				object->runpath_set = true;
				if (! (str = get_string(d_ptr)) )
					return 0;
				object->runpath = str;
				break;
			default:
				break;
		}
	}

	*waserror = false;
	return object.release();
}

auto LoadElf32LE = &LoadElf<false, Elf32_Ehdr, Elf32_Shdr, Elf32_Dyn>;
auto LoadElf32BE = &LoadElf<true,  Elf32_Ehdr, Elf32_Shdr, Elf32_Dyn>;
auto LoadElf64LE = &LoadElf<false, Elf64_Ehdr, Elf64_Shdr, Elf64_Dyn>;
auto LoadElf64BE = &LoadElf<true,  Elf64_Ehdr, Elf64_Shdr, Elf64_Dyn>;

Elf* Elf::open(const char *data, size_t size, bool *waserror, const char *name)
{
	*waserror = false;
	unsigned char *elf_ident = (unsigned char*)data;
	if (elf_ident[EI_MAG0] != ELFMAG0 ||
	    elf_ident[EI_MAG1] != ELFMAG1 ||
	    elf_ident[EI_MAG2] != ELFMAG2 ||
	    elf_ident[EI_MAG3] != ELFMAG3)
	{
		log(Debug, "%s: not an ELF file\n", name);
		return 0;
	}

	*waserror = true;
	unsigned char ei_class   = elf_ident[EI_CLASS];
	unsigned char ei_data    = elf_ident[EI_DATA];
	unsigned char ei_version = elf_ident[EI_VERSION];
	unsigned char ei_osabi   = elf_ident[EI_OSABI];

	if (ei_version != EV_CURRENT) {
		log(Error, "%s: invalid ELF version: %u\n", name, (unsigned)ei_version);
		return 0;
	}

	if (ei_data != ELFDATA2LSB &&
	    ei_data != ELFDATA2MSB)
	{
		log(Error, "%s: unrecognized ELF data type: %u (neither ELFDATA2LSB nor ELFDATA2MSB)\n",
		    name, (unsigned)ei_data);
		return 0;
	}

	if (ei_osabi != ELFOSABI_FREEBSD &&
	    ei_osabi != ELFOSABI_LINUX   &&
	    ei_osabi != ELFOSABI_NONE)
	{
		log(Warn, "%s: osabi not recognized: %u\n", name, (unsigned)ei_osabi);
	}

	Elf *e = 0;
	if (ei_class == ELFCLASS32) {
		if (ei_data == ELFDATA2LSB)
			e = LoadElf32LE(data, size, waserror, name);
		else
			e = LoadElf32BE(data, size, waserror, name);
	} else if (ei_class == ELFCLASS64) {
		if (ei_data == ELFDATA2LSB)
			e = LoadElf64LE(data, size, waserror, name);
		else
			e = LoadElf64BE(data, size, waserror, name);
	}
	if (!e)
		return 0;
	e->ei_class = ei_class;
	e->ei_data  = ei_data;
	e->ei_osabi = ei_osabi;
	return e;
}

static void
fixpath(std::string& path)
{
	size_t at = 0;
	do {
		at = path.find("//", at);
		if (at == std::string::npos)
			break;
		path.erase(path.begin()+at);
	} while(true);
}

static void
replace_origin(std::string& path, const std::string& origin)
{
	size_t at = 0;
	do {
		at = path.find("$ORIGIN", at);
		if (at == std::string::npos)
			break;
		path.replace(at, 7, origin);
		// skip this portion
		at += origin.length();
	} while (at < path.length());
	fixpath(path);
}

void
Elf::solve_paths(const std::string& origin)
{
	if (rpath_set)
		replace_origin(rpath, origin);
	if (runpath_set)
		replace_origin(runpath, origin);
}
