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

#include <os/pman_task.h>
#include <sartoris/syscall.h>
#include "ld.h"
#include "ldio.h"
#include "services/pmanager/services.h"

/* We are running on the process being loaded */
unsigned int __ldep(struct init_data_dl *initd)
{   
    unsigned int rsize, n;
	Elf32_Rel *rel;
    Elf32_Rela *rela;
	Elf32_Sym *sym;
	dyncache dync;
    int i;

    /******************************************************/
    /*                     LD Relocation                  */
    /******************************************************/
    /*
    We need to relocate ourselves.
    First add the offset to the dynamic section pointers.
    */
    Elf32_Dyn *dyn = (Elf32_Dyn *)(initd->ld_dynamic);
    
    for(i = 0; i < 24; i++)
        dync.data[i] = 0;
    
    i = 0;
    while(dyn[i].d_tag != DT_NULL)
    {
        if(dyn[i].d_tag < 24 || dyn[i].d_tag == 30)
        {
            switch(dyn[i].d_tag)
            {
                case DT_PLTGOT:
                case DT_HASH:
                case DT_STRTAB:
                case DT_SYMTAB:
                case DT_RELA:
                case DT_INIT:
                case DT_FINI:
                case DT_REL:
                case DT_DEBUG:
                case DT_JMPREL:
                    dync.data[dyn[i].d_tag] = (unsigned int)dyn[i].d_un.d_ptr + (unsigned int)initd->ld_start;
                    break;
                case DT_SYMBOLIC:
                    dync.data[DT_FLAGS] |= DF_SYMBOLIC;
                    break;
                case DT_FLAGS:
                    dync.data[25] |= dyn[i].d_un.d_val;
                    break;
                default:
                    dync.data[dyn[i].d_tag] = dyn[i].d_un.d_val;
            }
        }
        i++;
    }
    
    // relocate rel relocation section
	rel = (Elf32_Rel*)dync.data[DT_REL];
    rsize = dync.data[DT_RELSZ];
    
    n = 0;
    do
    {
        for (i = 0; i < rsize; i += sizeof (Elf32_Rel)) 
        {
		    sym = (Elf32_Sym*)(dync.data[DT_SYMTAB] + sizeof(Elf32_Sym) * ELF32_R_SYM(rel->r_info));
                        
		    RELOC_REL(rel, sym, (unsigned int*)(rel->r_offset + initd->ld_start), initd->ld_start);
		    rel++;
	    }
        n++;
        rel = (Elf32_Rel*)dync.data[DT_JMPREL];
	    rsize = dync.data[DT_PLTRELSZ];
    }while(n < 2 && dync.data[DT_PLTREL] == DT_REL);
    
    // relocate DT_RELA (if JMPREL is of type DT_RELA we will initialize it here)
    rela = (Elf32_Rela*)dync.data[DT_RELA];
	rsize = dync.data[DT_RELASZ];
    
    n = 0;
    do
    {
        for (i = 0; i < rsize; i += sizeof(Elf32_Rela)) 
        {
		    sym = (Elf32_Sym*)(dync.data[DT_SYMTAB] + sizeof(Elf32_Sym) * ELF32_R_SYM(rel->r_info)); // get the symbol from the table by adding it's offset
            
		    RELOC_RELA(rela, sym, (unsigned int*)(rel->r_offset + initd->ld_start), initd->ld_start);
		    rela++;
	    }
        n++;
        rela = (Elf32_Rela*)dync.data[DT_JMPREL];
	    rsize = dync.data[DT_PLTRELSZ];
    }while(n < 2 && dync.data[DT_PLTREL] == DT_RELA);
    
    __ldmain(initd, &dync);
    
    return initd->prg_start;
}
