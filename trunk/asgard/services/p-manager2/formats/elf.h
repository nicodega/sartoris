/*
	ELF File Format
*/

#ifndef _ELF_H
#define _ELF_H

/* data types */

typedef void*           Elf32_Addr;
typedef unsigned short  Elf32_Half;
typedef unsigned int    Elf32_Off;
typedef int             Elf32_Sword;
typedef unsigned int    Elf32_Word;

/* elf header */

#define EI_NIDENT 16

#define ELFCLASS32 0x1
#define ELFDATA2LSB 0x1

struct Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Half    e_type;
  Elf32_Half    e_machine;
  Elf32_Word    e_version;
  Elf32_Addr    e_entry;     /* entry point */
  Elf32_Off     e_phoff;     /* program header table file offset in bytes (zero otherwise) */
  Elf32_Off     e_shoff;     /* section header table file offset in bytes (zero otherwise) */
  Elf32_Word    e_flags;
  Elf32_Half    e_ehsize;    /* size in bytes of the elf header table */
  Elf32_Half    e_phentsize; /* size in bytes of one entry in the program header table */ 
  Elf32_Half    e_phnum;     /* number of entries in the program header table */
  Elf32_Half    e_shentsize; /* size in bytes of one entry in the section header table */
  Elf32_Half    e_shnum;     /* number of entries in the section header table */
  Elf32_Half    e_shstrndx;  /* section header table index associated with the section */
                             /* name string table, or SHN_UNDEF otherwise */
  
};

/* possible values for e_type */

#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

/* program header table entry */

struct Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off  p_offset; /* offset from the beginning of the file at which first byte resides */
  Elf32_Addr p_vaddr;  /* virtual address at which the first byte of the segment resides in memory */
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz; /* number of bytes in the file image of the segment, may be zero */
  Elf32_Word p_memsz;  /* number of bytes in the memory image of the segment, may be zero */
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* possible values for p_type */

#define PT_NULL    0           /* unused */
#define PT_LOAD    1           /* load into memory, pad with zeros if necessary */
#define PT_DYNAMIC 2           /* dynamic linking info */
#define PT_INTERP  3           /* location and size of string (path to interpreter) */
#define PT_NOTE    4           /* location and size of extra information */
#define PT_SHLIB   5           /* reserved */
#define PT_PHDR    6           /* location and size of program header table itself (if present) */
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff


/* possible values for p_flags (deduced from 'objdump -p' output on Linux) */

#define PF_EXEC  1
#define PF_WRITE 2
#define PF_READ  4

#endif /* _ELF_H */
