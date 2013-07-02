#include <stdio.h>

#include "main.h"

template<typename HDR, typename SecHDR, typename Dyn>
class ElfImpl : public Elf {
private:
	HDR *hdr;
	SecHDR *strhdr;
	SecHDR *dynhdr;
	size_t  dyncount;

public:
	const char*
	get_string(size_t off) const {
		return data + strhdr->sh_offset + off;
	}

	ElfImpl(const char *data, size_t size)
	: Elf(data, size)
	{
		strhdr   = 0;
		dynhdr   = 0;
		dyncount = 0;

		hdr = (decltype(hdr))(data);
		db_ident = (ei_class << 16) | (ei_osabi << 8) | 32;

		SecHDR *shdr = (SecHDR*)(data + hdr->e_shoff);
		for (size_t i = 0; i != hdr->e_shnum; ++i) {
			if (shdr[i].sh_type == SHT_DYNAMIC)
				dynhdr = shdr+i;
			else if (shdr[i].sh_type == SHT_STRTAB)
				strhdr = shdr+i;
		}

		if (!dynhdr) {
			log(Debug, "no dynamic section");
			error = true;
			return;
		}
		if (!strhdr) {
			log(Debug, "no string table section");
			error = true;
			return;
		}

		if (dynhdr->sh_entsize != sizeof(Dyn)) {
			log(Error, "invalid entsize for dynamic section\n");
			error = true;
			return;
		}

		dyncount = dynhdr->sh_size / sizeof(Dyn);

		Dyn *dynstart = (Dyn*)(data + dynhdr->sh_offset);
		for (size_t i = 0; i != dyncount; ++i) {
			Dyn *dyn = dynstart + i;
			switch (dyn->d_tag) {
				case DT_NEEDED:
					needed.push_back(get_string(dyn->d_un.d_ptr));
					break;
				case DT_RPATH:   rpath   = get_string(dyn->d_un.d_ptr); break;
				case DT_RUNPATH: runpath = get_string(dyn->d_un.d_ptr); break;
				default:
					break;
			}
		}
	}
};

using Elf32 = ElfImpl<Elf32_Ehdr, Elf32_Shdr, Elf32_Dyn>;
using Elf64 = ElfImpl<Elf64_Ehdr, Elf64_Shdr, Elf64_Dyn>;

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
		return new Elf32(data, size);
	else if (ei_class == ELFCLASS64)
		return new Elf64(data, size);
	else {
		log(Error, "unrecognized ELF class: %u\n", (unsigned)ei_class);
		return 0;
	}
}

Elf::Elf(const char *data_, size_t size_)
: error(false), data(data_), size(size_)
{
	elf_ident = (unsigned char*)data;
	if (elf_ident[EI_MAG0] != ELFMAG0 ||
	    elf_ident[EI_MAG1] != ELFMAG1 ||
	    elf_ident[EI_MAG2] != ELFMAG2 ||
	    elf_ident[EI_MAG3] != ELFMAG3)
	{
		error = true;
		return;
	}

	ei_class      = elf_ident[EI_CLASS];
	ei_data       = elf_ident[EI_DATA];
	ei_version    = elf_ident[EI_VERSION];
	ei_osabi      = elf_ident[EI_OSABI];
	ei_abiversion = elf_ident[EI_ABIVERSION];
}
