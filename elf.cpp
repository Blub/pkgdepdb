#include <stdio.h>

#include "main.h"

Elf::Elf()
{
}

template<typename HDR, typename SecHDR, typename Dyn>
Elf*
LoadElf(const char *data, size_t size)
{
	std::unique_ptr<Elf> object(new Elf);

	HDR    *hdr      = 0;
	SecHDR *dynhdr   = 0;
	size_t  dyncount = 0;
	Dyn    *dynstr   = 0;
	size_t  strsz    = 0;

	hdr = (HDR*)(data);

	//unsigned char *elf_ident = (unsigned char*)data;
	//unsigned char ei_class = elf_ident[EI_CLASS];
	//unsigned char ei_osabi = elf_ident[EI_OSABI];
	//db_ident = (ei_class << 16) | (ei_osabi << 8) | 32;

	auto checksize = [size](size_t off, size_t sz) -> bool {
		if (off + sz >= size) {
			log(Error, "unexpected end of file in ELF file\n");
			return false;
		}
		return true;
	};

	if (!checksize(hdr->e_shoff, sizeof(SecHDR)))
		return 0;

	SecHDR *shdr = (SecHDR*)(data + hdr->e_shoff);
	for (size_t i = 0; i != hdr->e_shnum; ++i) {
		if (!checksize((char*)(shdr+i) - data, sizeof(*shdr)))
			return 0;
		if (shdr[i].sh_type == SHT_DYNAMIC) {
			dynhdr = shdr+i;
			break;
		}
	}

	if (!dynhdr) {
		log(Debug, "no dynamic section");
		return 0;
	}

	if (dynhdr->sh_entsize != sizeof(Dyn)) {
		log(Error, "invalid entsize for dynamic section\n");
		return 0;
	}

	dyncount = dynhdr->sh_size / sizeof(Dyn);

	Dyn *dynstart = (Dyn*)(data + dynhdr->sh_offset);
	for (size_t i = 0; i != dyncount; ++i) {
		Dyn *dyn = dynstart + i;
		if (!checksize((char*)dyn - data, sizeof(*dyn)))
			return 0;
		if (dyn->d_tag == DT_STRTAB)
			dynstr = dyn;
		if (dyn->d_tag == DT_STRSZ)
			strsz  = dyn->d_un.d_val;
	}
	if (!dynstr) {
		log(Error, "No DT_STRTAB\n");
		return 0;
	}

	size_t dynstr_at = dynstr->d_un.d_ptr;
	auto get_string = [=](size_t off) -> const char* {
		// range check
		if (off >= strsz) {
			log(Error, "out of bounds string entry\n");
			return 0;
		}
		const char *str = data + dynstr_at + off;
		const char *end = data + dynstr_at + strsz;
		const char *at  = str;
		while (*str && str != end) ++str;
		if (*str && str == end) {
			// missing terminating nul byte
			log(Error, "unterminated string in string table\n");
			return 0;
		}
		return at;
	};

	for (size_t i = 0; i != dyncount; ++i) {
		Dyn        *dyn = dynstart + i;
		const char *str;
		switch (dyn->d_tag) {
			case DT_NEEDED:
				if (! (str = get_string(dyn->d_un.d_ptr)) )
					return 0;
				object->needed.push_back(str);
				break;
			case DT_RPATH:
				if (! (str = get_string(dyn->d_un.d_ptr)) )
					return 0;
				object->rpath = str;
				break;
			case DT_RUNPATH:
				if (! (str = get_string(dyn->d_un.d_ptr)) )
					return 0;
				object->runpath = str;
				break;
			default:
				break;
		}
	}
	return object.release();
}

auto LoadElf32 = &LoadElf<Elf32_Ehdr, Elf32_Shdr, Elf32_Dyn>;
auto LoadElf64 = &LoadElf<Elf64_Ehdr, Elf64_Shdr, Elf64_Dyn>;

Elf* Elf::open(const char *data, size_t size)
{
	unsigned char *elf_ident = (unsigned char*)data;
	if (elf_ident[EI_MAG0] != ELFMAG0 ||
	    elf_ident[EI_MAG1] != ELFMAG1 ||
	    elf_ident[EI_MAG2] != ELFMAG2 ||
	    elf_ident[EI_MAG3] != ELFMAG3)
	{
		log(Debug, "not an ELF file\n");
		return 0;
	}

	unsigned char ei_class   = elf_ident[EI_CLASS];
	unsigned char ei_version = elf_ident[EI_VERSION];
	unsigned char ei_osabi   = elf_ident[EI_OSABI];

	if (ei_version != EV_CURRENT) {
		log(Error, "invalid ELF version: %u\n", (unsigned)ei_version);
		return 0;
	}

	if (ei_osabi != ELFOSABI_FREEBSD &&
	    ei_osabi != ELFOSABI_LINUX   &&
	    ei_osabi != ELFOSABI_NONE)
	{
		log(Warn, "osabi not recognized: %u\n", (unsigned)ei_osabi);
	}

	if (ei_class == ELFCLASS32)
		return LoadElf32(data, size);
	else if (ei_class == ELFCLASS64)
		return LoadElf64(data, size);
	else {
		log(Error, "unrecognized ELF class: %u\n", (unsigned)ei_class);
		return 0;
	}
}
