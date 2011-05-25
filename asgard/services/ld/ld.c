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


extern unsigned int _LDGOTEND;
extern unsigned int _LDGOT;
extern unsigned int _edynamic;
extern unsigned int _DYNAMIC;

dl_object *gs_first;     // the global dependencies scope (DT_NEEDED)
dl_object *mem_first;    // memory list of loaded libraries (we will use a first fit algorithm)
dl_object objects[MAX_OBJECTS];
int objcount;   // ammount of objects
int resojb;     // resolved objects
char path[256];

/* We are running on the process being loaded */
void __ldmain(struct init_data_dl *initd)
{   
    struct pm_msg_generic pmmsg;
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
    Elf32_Dyn *dyn = (Elf32_Dyn *)((long)_DYNAMIC + initd->ld_start);

    for(i = 0; i < 24; i++)
        dync.data[i] = 0;

    i = 0;
    while(dyn[i].d_tag != DT_NULL)
    {
        switch(dyn->d_tag)
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
                dyn[i].d_un.d_ptr += initd->ld_start;
            default:
                dync.data[dyn->d_tag] = dyn[i].d_un.d_val;
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
		    sym = (Elf32_Sym*)(dync.data[DT_SYMTAB] + ELF32_R_SYM(rel->r_info));

		    if (ELF32_R_SYM(rel->r_info) && sym->st_value == 0)    // undefined symbol!
			    __ldexit(-4);

		    RELOC_REL(rel, sym, (unsigned int*)(rel->r_offset + initd->ld_start), initd->ld_start);
		    rel++;
	    }
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
		    sym = (Elf32_Sym*)(dync.data[DT_SYMTAB] + ELF32_R_SYM(rel->r_info)); // get the symbol from the table by adding it's offset

		    if (ELF32_R_SYM(rel->r_info) && sym->st_value == 0)    // undefined symbol!
			    __ldexit(-4);

		    RELOC_RELA(rela, sym, (unsigned int*)(rel->r_offset + initd->ld_start), initd->ld_start);
		    rela++;
	    }
        n++;
        rela = (Elf32_Rela*)dync.data[DT_JMPREL];
	    rsize = dync.data[DT_PLTRELSZ];
    }while(n < 2 && dync.data[DT_PLTREL] == DT_RELA);

    /******************************************************/
    /*                 LD Relocation END                  */
    /******************************************************/

    // now we can execute as normal (we have linked ourselves!)
    objcount = 2;
    resojb = 2;
    gs_first = NULL;
    
    path[0] = '/';
    path[1] = 'u';
    path[2] = 's';
    path[3] = 'r';
    path[4] = '/';
    path[5] = 'l';
    path[6] = 'i';
    path[7] = 'b';
    path[8] = '/';

    // object 0 will be out executable file
    objects[0].base = initd->prg_start;
    objects[1].maxaddr = 0;
    
    objects[1].base = initd->ld_start;
    objects[1].maxaddr = initd->ld_size;
    objects[1].mem_next = objects[1].mem_prev = NULL;
    objects[1].ls_first = NULL;

    mem_first = &objects[1];

    // we will need the IOLIB to read the elf headers from 
    // dependencies
    open_port(LD_IOPORT, 2, PRIV_LEVEL_ONLY);
    open_port(LD_PMAN_PORT, 1, PRIV_LEVEL_ONLY);

    /* Load dependencies */
    int ret = dl_load(&objects[0], initd);
    if(!ret)
    {
        __ldexit(-6);
    }

    i = 2;
    while(i < objcount && (ret = dl_load(&objects[i], NULL))){ i++; }
    
    if(!ret)
    {
        __ldexit(-6);
    }

    dl_build_global_scope();

    /* Now resolve the symbols */
    dl_resolve(0, 1);
    i = 2;
    while(objcount != resojb)
    {
        dl_resolve(i++, 1);
    }
    
    // close the IOPORT
    close_port(LD_IOPORT);
    close_port(LD_PMAN_PORT);
        
    /* Invoke init on each loaded library */
    dl_init_libs();

    // let PMAN know the dynamic loader has finished
    pmmsg.pm_type = PM_LOADER_READY;
    send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &pmmsg);

    // process entry point will be executed from _ldstart upon ret
}

typedef void (*ldfunc)(void);

void __ldexit(int ret)
{ 
    struct pm_msg_finished finished;
    int i;
    dl_object *o;

    // finalize all shared libraries
    for(i = 2; i < objcount; i++)
    {
        o = objects[i].ls_first;
        while(o)
        {
            if(o->fini)
            {
                ((ldfunc)o->fini)();
            }
            o = o->next;
        }
        if(objects[i].fini)
        {
            ((ldfunc)o->fini)();
        }
    }
        
    finished.pm_type = PM_TASK_FINISHED;
	finished.req_id = 0;
	finished.ret_value = ret;

	send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &finished);

	for(;;) { reschedule(); }
}

void dl_init_libs()
{
    int i;
    dl_object *o;
    
    // invoke init section on each library
    for(i = objcount-1; i > 1; i--)
    {
        o = objects[i].ls_first;
        while(o)
        {
            if(o->init)
            {
                ((ldfunc)o->init)();
            }
            o = o->next;
        }
        if(objects[i].init)
        {
            ((ldfunc)objects[i].init)();
        }
    }
}

unsigned int elf_hash(const unsigned char *name)
{
    unsigned int h = 0, g;

    while (*name) 
    { 
        h = (h << 4) + *name++; 
        if (g = h & 0xf0000000) 
            h ^= g >> 24; 
        h &= g; 
    }

    return h; 
} 

/*
This function will load an object and all it's dependencies.
*/
int dl_load(dl_object *obj, struct init_data_dl *initd)
{    
    struct Elf32_Phdr phdr;
    struct Elf32_Ehdr h;

    /*
    if it's not the executable file we must find free address
    space for the library and tell PMAN to load the library.
    */
    if(initd == NULL)
    {
        // was this library already loaded?
        dl_object *o = mem_first;

        while(o)
        {
            if(streq(o->name, obj->name))
                return 0;
            o = o->mem_next;
        }

        // is the name absolute or relative?
        int index = last_index_of(obj->name, '/');

        if(index == -1)
        {
            // idealy we should sheck environment variables and stuf like that
            // but we will do that in a future.
            // for now we will use /lib
            istrcopy(obj->name, path, 9);
        }

        // calculate dependency size
        FILE fp;
                
        if(!fopen(obj->name, &fp)) return 0;

        // read the elf header        
        if(fread((char*)&h, sizeof(struct Elf32_Ehdr), &fp) != sizeof(struct Elf32_Ehdr))
        {
            fclose(&fp);
            return 0;
        }

        // read program headers
        // and calculate full size of loadable segments
        unsigned int count = h.e_phnum;
        obj->maxaddr = 0;

        fseek(&fp, h.e_phoff);

        while(count > 0)
        {
            if(fread((char*)&phdr, sizeof(struct Elf32_Phdr), &fp) != sizeof(struct Elf32_Phdr))
            {
                fclose(&fp);
                return 0;
            }
            if( (phdr.p_type == PT_LOAD || phdr.p_type == PT_DYNAMIC) && (unsigned int)phdr.p_vaddr + phdr.p_memsz > obj->maxaddr)
            {
                obj->maxaddr = (unsigned int)phdr.p_vaddr + phdr.p_memsz;
            }
            if(phdr.p_type == PT_DYNAMIC)
            {
                obj->dynamic = (unsigned int)phdr.p_vaddr;
            }
            count--;
        }
        
        if(obj->maxaddr & 0x1000 != 0)
            obj->maxaddr += 0x1000 - (obj->maxaddr & 0x1000);

        fclose(&fp);

        // find a free area for this library
        o = mem_first;
        dl_object *ol = NULL;
        unsigned int last_addr = 0xC0000000;    // start at 3GB boundary

        while(o)
        {   
            if(o->maxaddr - last_addr >= obj->maxaddr)
                break;  // it fits here
            last_addr = o->maxaddr;
            ol = o;
            o = o->mem_next;
        }

        if(o == NULL)
        {
            if((unsigned int)0xFFFFFFFF - last_addr < obj->maxaddr)
                return 0;
            if(ol == NULL)
            {
                mem_first = obj;
                obj->mem_next = NULL;
                obj->mem_prev = NULL;
            }
            else
            {
                obj->mem_next = NULL;
                obj->mem_prev = ol;
            }
            obj->base = last_addr;
            obj->maxaddr += obj->base;
        }
        else
        {
            obj->mem_next = o;
            obj->mem_prev = ol;
            obj->base = last_addr;
            obj->maxaddr += obj->base;
        }

        // tell pman to load the library
        struct pm_msg_loadlib pmload;

        pmload.pm_type = PM_LOAD_LIBRARY;
        pmload.req_id = 0;
        pmload.response_port = LD_PMAN_PORT;
        pmload.vlow = obj->base;
        pmload.vhigh = obj->maxaddr;
        pmload.path_smo_id = share_mem(PMAN_TASK, path, len(path) + 1, READ_PERM);

        send_msg(PMAN_TASK, PMAN_COMMAND_PORT, &pmload);
        
        struct pm_msg_loadlib_res res;
        int id;
        
        do
        {   
            while(get_msg_count(LD_PMAN_PORT) == 0){ reschedule(); }

            get_msg(LD_PMAN_PORT, &res, &id);

        }while(id != PMAN_TASK);
        
        /*
        Get pointers to the dynamic section
        */
        obj->dynamic += obj->base;
        obj->next = NULL;
        obj->ls_first = NULL;
    }
    else
    {
        // use SMO to read the orignal file program headers
        int offset = 0, i = 0;
        do
        {
            read_mem(initd->phsmo, offset, sizeof(struct Elf32_Phdr), &phdr);
            offset += initd->phsize;
            i++;
        }while(phdr.p_type != PT_DYNAMIC && i < initd->phcount);

        if(phdr.p_type != PT_DYNAMIC)
            return 0;

        obj->dynamic = (unsigned int)phdr.p_vaddr + obj->base;
    }

    // to get the pointers to hash, symtbl and dynstr I'll read it's dynamic section
    Elf32_Dyn *dyn = (Elf32_Dyn*)obj->dynamic;

    obj->pltgot = NULL;
    obj->hash = NULL;
    obj->dynstr = NULL;
    obj->symtbl = NULL;
    obj->rela = NULL;
    obj->rel = NULL;
    obj->jmprel = NULL;
    obj->init = NULL;
    obj->fini = NULL;
    obj->relasz = NULL;
    obj->relsz = NULL;
    obj->pltrelsz = NULL;
    obj->pltrel = 0;
        
    int i = 0;
    while(dyn[i].d_tag != DT_NULL)
    {
        switch(dyn->d_tag)
        {
            case DT_PLTGOT:
                obj->pltgot = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_HASH:
                obj->hash = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_STRTAB:
                obj->dynstr = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_SYMTAB:
                obj->symtbl = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_RELA:
                obj->rela = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_REL:
                obj->rel = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_JMPREL:
                obj->jmprel = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_INIT:
                obj->init = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_FINI:
                obj->fini = (unsigned int)dyn[i].d_un.d_ptr + obj->base;
                break;
            case DT_RELASZ:
                obj->relasz = dyn[i].d_un.d_val;
                break;
            case DT_RELSZ:
                obj->relsz = dyn[i].d_un.d_val;
                break;
            case DT_PLTRELSZ:
                obj->pltrelsz = dyn[i].d_un.d_val;
                break;
            case DT_PLTREL:
                obj->pltrel = dyn[i].d_un.d_val;
            default:
                break;
        }
        i++;
    }

    /*
    Look for dependencies on the DT_NEEDED area and dl_resolve them.
    REMEMBER: Add the dependencies to the global lookup scope in a
    breadth-first order.
    */
    i = 0;
    while(dyn[i].d_tag != DT_NULL)
    {
        switch(dyn->d_tag)
        {
            case DT_NEEDED:
                // allocate a new object
                obj = &objects[objcount];
                objcount++;
                // look on the dynstr using d_val
                obj->name = (char*)(obj->dynstr + dyn[i].d_un.d_val);
                break;
            default:
                break;
        }
        i++;
    }

    return 1;
}

void dl_build_global_scope()
{
    int i = 2;  // we will ignore the linker

    gs_first = objects; // first the executable

    // now all of it's dependencies and so on
    // NOTE: Because of the way we created the objects
    // on loading, they appear in a breadth-first order
    while(i < objcount)
    {
        objects[i-1].next = &objects[i];        
        i++;
    }
    objects[i-1].next = NULL;
}

int dl_resolve(int obj, int lazy)
{
    dl_object *o = &objects[obj];
    /*
    Resolve symbols at load time.
    */
    dl_relocate(o, DT_REL, DT_RELSZ);
    dl_relocate(o, DT_RELA, DT_RELASZ);
    
    if(lazy)
        dl_relocate(o, DT_JMPREL, DT_RELASZ);
    else
        dl_relocate_gotplt_lazy(o);
        
    resojb++;
}

int dl_relocate(dl_object *obj, int rel, int relsz)
{
    int i, relsc;
	Elf32_Rel *rels;  // careful, if section is not DT_REL this will be an Elf32_Rela *array.
    Elf32_Addr *addr, value, ooff;
    Elf32_Word type;
    Elf32_Sym *sym, *sm;
    char *sname;
    int numrel;
    int ret = 0;

    if((rel == DT_REL && !obj->rel) 
        || (rel == DT_RELA && !obj->rela)
        || (rel == DT_JMPREL && !obj->jmprel))
        return 1;

    if(rel == DT_REL) 
        numrel = relsz / sizeof(Elf32_Rel);
    else
        numrel = relsz / sizeof(Elf32_Rela);

    for (i = 0; i < numrel; i++, rels++) 
    {
        if(rel != DT_REL) rels = (Elf32_Rel*)((unsigned int)rels+4);

        type = ELF32_R_TYPE(rels->r_info);

        if (type == R_386_NONE 
            || (type == R_386_JMP_SLOT && rel != DT_JMPREL))
            continue;

        addr = (Elf32_Addr *)(rels->r_offset + obj->base);

        sym = (Elf32_Sym*)(obj->symtbl + ELF32_R_SYM(rels->r_info));
        sname = (char*)(obj->dynstr + sym->st_name);

        if (type == R_386_COPY) 
        {
            /*
              The link editor creates this relocation type for dynamic linking.
              Its offset member refers to a location in a writable segment. The
              symbol table index specifies a symbol that should exist both in the
              current object file and in a shared object. During execution, the
              dynamic linker copies data associated with shared object's symbol to
              the location specified by the offset.
            */
            dl_object *fobj;
            char *dst, *src;
                     
            if(!dl_find_symbol(sname, obj, &sm, &fobj, 1, type == R_386_JMP_SLOT))
            {
                ret++;
                continue;
            }
            
            // copy the value
            src = (char*)(fobj->base + sm->st_value); // the symbol is not yet resolved, add the base!
            dst = (char*)addr;

            int j;
            for(j = 0; j < sym->st_size; j++)
            {
                dst[j] = src[j];
            }
            continue;
        }

        // if it uses the addend set the value to the address
        switch(type)
        {
            case R_386_32:
            case R_386_PC32:
            case R_386_PLT32:
            case R_386_GLOB_DAT:
            case R_386_RELATIVE:
                if(rel == DT_REL)
                    value = *addr;
                else
                    value = (Elf32_Addr)((Elf32_Rela*)rels)->r_addend;
                break;
            default:
                value = 0;
        }

        sym = NULL;
        sname = NULL;
        // if it uses a symbol
        if (type == R_386_32 || type == R_386_PC32 
            || type == R_386_GLOB_DAT || type == R_386_JMP_SLOT 
            || type == R_386_GOTOFF) 
        {
            // resolve the symbol and it's name
            if (sym->st_shndx != SHN_UNDEF 
                && ELF32_ST_BIND(sym->st_info) == STB_LOCAL) 
            {
                // it's a local symbol, just add the base
                value += obj->base;
            }
            else
            {
                // find the symbol with the index specified on this symbol
                // r_info
                Elf32_Sym *symref = (Elf32_Sym*)(obj->symtbl + ELF32_R_SYM(rels->r_info));
                dl_object *fobj;
                sname = (char*)(obj->dynstr + symref->st_name);

                if(!dl_find_symbol(sname, obj, &sm, &fobj, 1, type == R_386_JMP_SLOT))
                {
                    ret++;
                    continue;
                }

                value = (Elf32_Addr)((unsigned int)value + fobj->base + (unsigned int)symref->st_value);
            }

            if (type == R_386_PC32)
            {
                value = (Elf32_Addr)((unsigned int)value - (unsigned int)addr);
            }
            else if (type == R_386_JMP_SLOT) 
            {
                *addr = value;
                continue;
            }
        }
        else if (type == R_386_RELATIVE)
        {
            value += obj->base;
        }

        *((unsigned int*)addr) |= (unsigned int)value;
    }
}

int dl_relocate_gotplt_lazy(dl_object *obj)
{
    Elf32_Rel *rel;
    Elf32_Addr *addr;
    int i, num;

    if(!obj->pltgot
        || obj->pltrel != DT_REL) return 1; // I'll only support JMPREL with DT_REL format

	rel = (Elf32_Rel*)obj->jmprel;
	num = obj->pltrelsz / sizeof(Elf32_Rel);
	
    // TODO: should we unprotect the GOT for the PLT here?
    for (i = 0; i < num; i++, rel++) 
    {
		addr = (Elf32_Addr *)(rel->r_offset + obj->base);
		*addr += obj->base;
	}
    // TODO: we should protect the GOT for the PLT here

    // set the GOT second and third entries
    unsigned int *got = (unsigned int*)obj->pltgot;

    got[1] = (unsigned int)&obj;  // second entry contains the object address
    got[2] = (unsigned int)&dl_runtime_bind;  

    return 1;
}

char *dl_symbol_name(dl_object *obj, Elf32_Sym *sym)
{
    return (char*)(obj->dynstr + sym->st_name);
}

/*
Find a symbol on an object (and only on this object).
sym will be set to the symbol if found or NULL otherwise.
*/
int dl_find_symbol_obj(char *name, dl_object *obj, Elf32_Sym **sym, int plt)
{
    unsigned int nbuckets = *((unsigned int*)obj->hash),
                 *hash = (unsigned int*)(obj->hash+8),
                 *chain = (unsigned int*)(obj->hash + (nbuckets << 2)),
	             hv = elf_hash(name) % *(hash), // first entry of the hash has nbuckets
                 index;
    Elf32_Sym *sm = NULL;
    *sym = NULL;

    // using the hash we will find the symbol on the specified object
    for(index = hash[hv]; index; index = chain[index])
    {
        sm = (Elf32_Sym*)obj->symtbl + index;

        if (ELF32_ST_TYPE(sm->st_info) != STT_NOTYPE &&
		    ELF32_ST_TYPE(sm->st_info) != STT_OBJECT &&
		    ELF32_ST_TYPE(sm->st_info) != STT_FUNC)
			continue;

        // consider only global and weak symbols
        if(ELF32_ST_BIND(sm->st_info) != STB_GLOBAL
            && ELF32_ST_BIND(sm->st_info) != STB_WEAK)
            continue;

        // if the address is undefined, we won't consider
        // the symbol, unless it's a function and we don't 
        // need the actual symbol
        if (sm->st_shndx == SHN_UNDEF && (plt
            || sm->st_value == 0 
            || ELF32_ST_TYPE(sm->st_info) != STT_FUNC))
				continue;
		
        if(streq(name, (char*)(obj->dynstr + sm->st_name)))
        {
            *sym = sm;
            return 1;
        }
    }

	return 0;
}

/*
This function will find a symbol starting at a given object.
- First it'll find on the local scope.
- If the symbol is not found on the local scope, it'll search the global scope.
*/
int dl_find_symbol(char *name, dl_object *obj, Elf32_Sym **sym, dl_object **found_obj, int weak, int plt)
{
    Elf32_Sym *weaksym = NULL, *sm = NULL, *fsm;
    dl_object *wobj, *fobj;
    dl_object *o;

    o = obj->ls_first;
    
    // first try the local scope
    while(o)
    {   
        if(dl_find_symbol_obj(name, o, &fsm, plt))
        {
            // is it a weak symbol?
            if(ELF32_ST_BIND(fsm->st_info) == STB_GLOBAL)
            {
                sm = fsm;
                fobj = o;
                break;  // found a symmbol
                
            }
            else if(ELF32_ST_BIND(fsm->st_info) == STB_WEAK && !weaksym) 
            {
                wobj = o;
                weaksym = fsm;
            }
        }
        o = o-> next;
    }

    // if we didn't find a symbol on the local scope, try the 
    // global scope (I won't care about weak symbols found on 
    // the local scope, unless nothing is found on the global
    // scope.
    if(!sm)
    {
        o = obj->next;    // start at the next object
        while(o)
        {   
            if(dl_find_symbol_obj(name, o, &fsm, plt))
            {
                // is it a weak symbol?
                if(ELF32_ST_BIND(fsm->st_info) == STB_GLOBAL)
                {
                    sm = fsm;
                    fobj = o;
                    break;  // found a symmbol                
                }
                else if(ELF32_ST_BIND(fsm->st_info) == STB_WEAK && !weaksym) 
                {
                    wobj = o;
                    weaksym = fsm;
                }
            }
            o = o-> next;
        }
    }

    // if we found a weak symbol and the weak flag is set, return it.
    if(sm)
    {
        *sym = sm;
        *found_obj = fobj;
        return 1;
    }
    else if(sm == NULL && weaksym != NULL && weak)
    {
        *sym = weaksym;
        *found_obj = wobj;
        return 1;
    }
    *sym = NULL;
    *found_obj = NULL;
    return 0;
}

void dl_runtime_bind(dl_object *obj, int rel_index)
{
    // a function tried to access a shared library function
    // with late binding. Fix the GOT.
    Elf32_Rel *rel = (Elf32_Rel *)(obj->jmprel + rel_index);
    Elf32_Sym *sym = (Elf32_Sym*)(obj->symtbl + ELF32_R_SYM(rel->r_info)), *sm;
    char *symn = (char*)(obj->dynstr + sym->st_name);
    dl_object *fobj;

    /*
    the program pushes a relocation offset (offset) on
    the stack. The relocation offset is a 32-bit, non-negative byte
    offset into the relocation table. The designated relocation entry
    will have type R_386_JMP_SLOT, and its offset will specify the
    global offset table entry used in the previous jmp instruction. The
    relocation entry also contains a symbol table index, thus telling
    the dynamic linker what symbol is being referenced, name1 in this
    case.
    */
    Elf32_Word *addr = (Elf32_Word *)(obj->pltgot + rel->r_offset);

    // do a symbol lookup
    if(!dl_find_symbol(symn, obj, &sm, &fobj, 1, 1))
    {
        __ldexit(-6);
    }

    *addr = fobj->base + (Elf32_Word)sm->st_value; // set the correct value on the GOT
}


int streq(char* str1, char* str2)
{
	int i = 0;

	if(str1 == NULL && str2 == NULL) return -1;
	if(str1 == NULL || str2 == NULL) return 0;

	while(str1[i] != '\0' && str2[i] != '\0' && str1[i] == str2[i])
	{
		i++;
	}

	return str1[i] == str2[i];
}

int last_index_of(char *str, char c)
{
	int i = 0, k = -1;

	if(str == NULL) return -1;

	while(str[i] != '\0'){
		if(str[i] == c) k = i;
		i++;	
	}
	return k;
}

void istrcopy(char* source, char* dest, int start)
{

	int i = 0;

	while(source[i] != '\0')
	{
		dest[start + i] = source[i];
		i++;
	}
	dest[start + i] = '\0';
}

int len(const char* str)
{
	int i = 0;

	if(str == NULL) return 0;

	while(str[i] != '\0')
	{
		i++;
	}

	return i;
}
