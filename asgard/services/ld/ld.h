/*
*
*	Copyright (C) 2002, 2003, 2004, 2005
*       
*	Santiago Bazerque 	sbazerque@gmail.com			
*	Nicolas de Galarreta	nicodega@gmail.com
*
*	
*	Redistribution and use in source and binary forms, with or without 
* 	modification, are permitted provided that conditions specified on 
*	the License file, located at the root project directory are met.
*
*	You should have received a copy of the License along with the code,
*	if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*	or you can contact us directly at the email addresses provided above.
*
*
*/

#include <sartoris/kernel.h>
#include <sartoris/syscall.h>
#include <lib/const.h>
#include "elf.h"
#include "proc/init_data_dl.h"

#ifndef LDH
#define LDH

#define MAX_OBJECTS   40
#define LD_INITPORT  2
#define LD_IOPORT     2
#define LD_PMAN_PORT  3

typedef struct
{
    Elf32_Sword	d_tag;
    union 
    {
        Elf32_Word	d_val;
        Elf32_Addr	d_ptr;
    } d_un;
} Elf32_Dyn;

typedef struct 
{
   	Elf32_Addr	r_offset;
   	Elf32_Word	r_info;
} Elf32_Rel;

typedef struct 
{ 
    Elf32_Addr r_offset; 
    Elf32_Word r_info; 
    Elf32_Sword r_addend; 
} Elf32_Rela;

typedef struct 
{
    Elf32_Word    st_name;
    Elf32_Addr    st_value;
    Elf32_Word    st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half    st_shndx;
} Elf32_Sym;

#define ELF32_R_SYM(i)	((i)>>8)
#define ELF32_R_TYPE(i)	((unsigned char)(i))
#define ELF32_R_INFO(s, t)	((s)<<8+(unsigned char)(t))

#define ELF32_ST_BIND(i)	((i)>>4)
#define ELF32_ST_TYPE(i)	((i)&0xf)
#define ELF32_ST_INFO(b, t)	(((b)<<4)+((t)&0xf))

#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2
#define STB_LOPROC     13
#define STB_HIPROC     15

#define STT_NOTYPE       0
#define STT_OBJECT       1
#define STT_FUNC         2
#define STT_SECTION      3
#define STT_FILE         4

#define DT_NULL         0
#define DT_NEEDED		1
#define DT_PLTRELSZ		2
#define DT_PLTGOT		3
#define DT_HASH		    4
#define DT_STRTAB		5
#define DT_SYMTAB		6
#define DT_RELA		    7
#define DT_RELASZ		8
#define DT_RELAENT		9
#define DT_STRSZ		10
#define DT_SYMENT		11
#define DT_INIT		    12
#define DT_FINI		    13
#define DT_SONAME		14
#define DT_RPATH		15
#define DT_SYMBOLIC		16
#define DT_REL		    17
#define DT_RELSZ		18
#define DT_RELENT		19
#define DT_PLTREL		20
#define DT_DEBUG		21
#define DT_TEXTREL		22
#define DT_JMPREL		23
#define DT_FLAGS        30
#define DT_LOPROC       0x70000000
#define DT_HIPROC       0x7fffffff

#define DF_ORIGIN       0x1
#define DF_SYMBOLIC     0x2
#define DF_TEXTREL      0x4
#define DF_BIND_NOW     0x8 

#define R_386_NONE      0
#define R_386_32	    1    
#define R_386_PC32	    2
#define R_386_GOT32	    3
#define R_386_PLT32	    4
#define R_386_COPY	    5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF	9
#define R_386_GOTPC	    10

unsigned int elf_hash(const unsigned char *name);

// We should use malloc, but for now we will keep library
// references on a fixed size array.
typedef struct __dl_object
{
    unsigned int base;    // base address of the object
    unsigned int maxaddr; // max address of the object
    unsigned int hash;    // address of the hash 
    unsigned int symtbl;  // address of the symbols table  
    unsigned int dynstr;  // address of the symbols strings
    unsigned int rel;     
    unsigned int rela;    
    unsigned int jmprel;  
    unsigned int relsz;   
    unsigned int relasz;  
    unsigned int pltrelsz;
    unsigned int pltgot;     // GOT
    unsigned int init;
    unsigned int fini;
    unsigned int pltrel;
    unsigned int dynamic; // address of the dynamic section
    unsigned int *buckets;
    unsigned int *chain;
    unsigned int nbuckets;
    unsigned int flags;
    char *name;           // the decoded name of the dependency
    struct __dl_object *ls_first; // local scope of dependencies (when using dlopen)
    struct __dl_object *next;     // next on scope list
    struct __dl_object *mem_next; // used on memory list
    struct __dl_object *mem_prev;
} dl_object;

typedef struct
{
    unsigned int data[25];
} dyncache;

void __ldexit(int ret);

/*
This functions are used on the initial relocation procedure for LD.
*/
static inline void RELOC_REL(Elf32_Rel *rel, Elf32_Sym *sym, unsigned int *addr, unsigned int base)
{
	if (ELF32_R_TYPE(rel->r_info) == R_386_RELATIVE) 
    	*addr = base;
	else if (ELF32_R_TYPE(rel->r_info) == R_386_GLOB_DAT 
        || ELF32_R_TYPE(rel->r_info) == R_386_JMP_SLOT) 
    	*addr = (unsigned int)sym->st_value + base;
	else
    	__ldexit(-6); // not supported
}

static inline void RELOC_RELA(Elf32_Rela *rel, Elf32_Sym *sym,unsigned int *addr, unsigned int base)
{
	if (ELF32_R_TYPE(rel->r_info) == R_386_RELATIVE) 
    	*addr = base + rel->r_addend;
	else if (ELF32_R_TYPE(rel->r_info) == R_386_GLOB_DAT
        || ELF32_R_TYPE(rel->r_info) == R_386_JMP_SLOT) 
    	*addr = (unsigned int)sym->st_value + rel->r_addend + base;
	else 
    	__ldexit(-6); // not supported	
}

/* Functions */
unsigned int elf_hash(const unsigned char *name);
int dl_load(dl_object *obj, struct init_data_dl *initd);
void dl_build_global_scope();
int dl_resolve(int obj, int lazy);
int dl_relocate(dl_object *obj, int rel, unsigned int relsz);
int dl_relocate_gotplt_lazy(dl_object *obj);
char *dl_symbol_name(dl_object *obj, Elf32_Sym *sym);
int dl_find_symbol_obj(char *name, unsigned int hv, dl_object *obj, Elf32_Sym **sym, int plt);
int dl_find_symbol(char *name, dl_object *obj, Elf32_Sym **sym, dl_object **found_obj, int weak, int plt, int exclude_obj);
unsigned int dl_runtime_bind(dl_object *obj, int rel_index);
void dl_init_libs();
void __ldmain(struct init_data_dl *initd, dyncache *dync);
void dl_buildobject(dl_object *obj, dyncache *dyn);
void dl_runtime_bind_start();
int dl_get_envs();
int dl_build_path(dl_object *obj);

/* String helpers */
int streq(char* str1, char* str2);
int last_index_of(char *str, char c);
void istrcopy(char* source, char* dest, int start);
int len(const char* str);

#endif

