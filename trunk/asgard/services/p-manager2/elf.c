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

#include "types.h"
#include "task_thread.h"
#include "io.h"
#include "loader.h"
#include "elf_loader.h"
#include "kmalloc.h"

/* This is the simplest implementation of an elf interpreter */

/* Begin elf parsing. This function takes a task id and a function pointer, to an io function.
The io function will return 1 if data is already present, or 0 y we have to wait for a response.
This function will return -1 if it's not a valid elf file. */
INT32 elf_begin(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset))
{
	/* read header */
	task->loader_inf.stage = LOADER_STAGE_ELF_HDR;

	task->io_finished.callback = elf_read_finished_callback;

	if(ioread(&task->io_event_src, sizeof(struct Elf32_Ehdr),(char*)&task->loader_inf.elf_header))
	{
		if(!elf_check_header(task)) return -1;

		// data is already present, read program headers
		return elf_seekphdrs(task, ioread, ioseek);
	}
	// if data is not present, leave, for elf_request will continue
	return 0;
}

INT32 elf_seekphdrs(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset))
{
	// seek to program headers possition
	task->loader_inf.stage = LOADER_STAGE_ELF_PHDR;

	task->io_finished.callback = elf_seek_finished_callback;

	if(ioseek(&task->io_event_src, task->loader_inf.elf_header.e_phoff))
	{
		// data is already present, read it
		return elf_readphdrs(task, ioread, ioseek);
	}
	// if data is not present, leave, for elf_request will continue
	return 0;
}

INT32 elf_readphdrs(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset))
{
	UINT32 size;

	// seek to program headers possition
	size = task->loader_inf.elf_header.e_phentsize * task->loader_inf.elf_header.e_phnum;

	task->loader_inf.elf_pheaders = (BYTE*)kmalloc(size);
    if(task->loader_inf.elf_pheaders == NULL)
    {
        return -1;
    }

    if(task->loader_inf.elf_pheaders == NULL)
    {
        return -1;
    }
    
	task->io_finished.callback = elf_readh_finished_callback;

	if(ioread(&task->io_event_src, size, task->loader_inf.elf_pheaders))
	{
        if(!elf_check(task)) return -1;

		task->loader_inf.stage = LOADER_STAGE_LOADED;
        return 1;
	}

	return 0;
}

/* Check task has a valid elf file */
INT32 elf_check_header(struct pm_task *task)
{
	struct Elf32_Ehdr *header = &task->loader_inf.elf_header;

	// check elf identifier
	if(header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') return 0;
	if(header->e_ident[4] != ELFCLASS32 || header->e_ident[5] != ELFDATA2LSB) return 0;

	// check it´s an executable file
	if(header->e_type != ET_EXEC && header->e_type != ET_DYN) return 0;	// must be an executable file

	return 1;
}

/* Check task has a valid elf file */
INT32 elf_check(struct pm_task *task)
{
    // if we hace section headers, decode them

	return 1;
}



