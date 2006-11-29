/*
*	Process and Memory Manager Service
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

#include "pmanager_internals.h"

extern struct pm_thread thread_info[MAX_THR];
extern struct pm_task   task_info[MAX_TSK];

/* This is the simplest implementation of an elf interpreter */

/* Begin elf parsing. This function takes a task id and a function pointer, to an io function.
The io function will return 1 if data is already present, or 0 y we have to wait for a response, -1 if it's not a valid elf file. */
int elf_begin(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*))
{
	/* read header */
	task_info[task].flags |= TSK_FLAG_ELF_HDR;
	if(ioread(task, sizeof(struct Elf32_Ehdr),(char*)&task_info[task].elf_header))
	{
		task_info[task].flags &= ~TSK_FLAG_ELF_HDR;

		if(!elf_check_header(task)) return -1;

		// data is already present, read program headers
		return elf_seekphdrs(task, ioseek, ioread);
	}
	// if data is not present, leave, for elf_request will continue
	return 0;
}

int elf_seekphdrs(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*))
{
	// seek to program headers possition
	task_info[task].flags |= TSK_FLAG_ELF_PHDR;
	if(ioseek(task, task_info[task].elf_header.e_phoff))
	{
		// data is already present, read it
		return elf_readphdrs(task, ioseek, ioread);
	}
	// if data is not present, leave, for elf_request will continue
	return 0;
}

int elf_readphdrs(int task, int (*ioseek)(int, unsigned int), int (*ioread)(int, int, char*))
{
	// seek to program headers possition
	task_info[task].flags |= TSK_FLAG_ELF_PHDR;
	int size = task_info[task].elf_header.e_phentsize * task_info[task].elf_header.e_phnum;

	task_info[task].elf_pheaders = (char *)malloc(size);

	if(ioread(task, size, task_info[task].elf_pheaders))
	{
		task_info[task].flags &= ~TSK_FLAG_ELF_PHDR;
		
		if(!elf_check(task)) return -1;
		
		return 1;
	}

	return 0;
}

/* Check task has a valid elf file */
int elf_check_header(int task)
{
	struct Elf32_Ehdr *header = (struct Elf32_Ehdr*)&task_info[task].elf_header;

	// check elf identifier
	if(header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') return 0;
	if(header->e_ident[4] != ELFCLASS32 || header->e_ident[5] != ELFDATA2LSB) return 0;

	// check it´s an executable file
	if(header->e_type != ET_EXEC) return 0;	// must be an executable file

	return 1;
}

/* Check task has a valid elf file */
int elf_check(int task)
{
	
	return 1;
}

/* This function will return file possition to read based on the elf program headers.
if address is not on the file, it'll return 0, else 1, and out_pos and our_size will be valid; 
NOTE: The way this has been made, we will require sections to be page aligned.
*/
int elf_filepos(int task, void *linear, unsigned int *outpos, unsigned int *outsize, int *perms, int *page_displacement)
{
	char *headers = task_info[task].elf_pheaders;
	int i = 0;
	linear -= SARTORIS_PROCBASE_LINEAR;
	*page_displacement = 0;

	while(i < task_info[task].elf_header.e_phnum)
	{
		struct Elf32_Phdr *prog_header = (struct Elf32_Phdr*) &headers[i*task_info[task].elf_header.e_phentsize];

		// consider only loadable segments
		if(prog_header->p_type != PT_LOAD)
		{
			i++;
			continue;
		}

		// align (to the next page boundary) the end of the segment, in order to load a page if it has something
		// on file
		unsigned int aligned_seg_end = (unsigned int)prog_header->p_vaddr + (unsigned int)prog_header->p_memsz;
		aligned_seg_end += (0x1000 - aligned_seg_end % 0x1000);

		/* See if address is inside the segment */
		if((unsigned int)prog_header->p_vaddr <= (unsigned int)linear && (unsigned int)linear < aligned_seg_end)
		{
			/*	ok linear falls on this segment, now
				check wether we must or not read from 
				the file
			*/
			// Section offset is our address substracting the begining (in memory) of the section 
			// and page aligned (down)
			unsigned int section_offset = (linear - prog_header->p_vaddr);
			section_offset -= section_offset % 0x1000;

			if( ((unsigned int)prog_header->p_vaddr % 0x1000 != 0) && (section_offset == 0) )
			{
				*page_displacement = prog_header->p_offset % 0x1000;
			}

			if(section_offset < prog_header->p_filesz)
			{
				/* Calculate outpos and outsize */
				*outpos = prog_header->p_offset + section_offset;

				unsigned int size = prog_header->p_filesz - section_offset;

				if(size - *page_displacement > 0x1000)
				{
					*outsize = 0x1000 - *page_displacement;
				}
				else
				{
					*outsize = size - *page_displacement;	
				}

				if(prog_header->p_flags & PF_WRITE)
				{
					*perms = PGATT_WRITE_ENA;
				}
				else
				{
					*perms = 0;	// I think this allows only reading in sartoris :)
				}

				return 1;
			}
			else
			{
				return 0;
			}
		}
		i++;
	}
	return 0;
}
