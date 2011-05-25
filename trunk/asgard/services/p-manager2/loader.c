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

#include "formats/elf.h"
#include "elf_loader.h"
#include "loader.h"
#include "helpers.h"
#include "task_thread.h"
#include "types.h"
#include <sartoris/syscall.h>
#include <services/stds/stdfss.h>
#include <services/pmanager/services.h>

static INT32 tsk_priv[] = { /* Proc */ 2, /* Serv */ 0 };
int ld_task;
int ld_size;

UINT32 loader_create_task(struct pm_task *task, char *path, UINT32 plength, int param, UINT32 type, BOOL flags)
{
	struct task mk_task;
	ADDR pg_address;

	/* Get a Page for the Directory */
	task->vmm_info.page_directory = (struct vmm_page_directory*)vmm_get_dirpage(task->id);
    
	if(task->vmm_info.page_directory == NULL) 
		return PM_NOT_ENOUGH_MEM;
	
    /* if we got here, task id is valid, the task slot is free,
	the path got through fine (smo is ok) and we have enough
	memory at least for a new page directory */
    
	/* save info */
	task->state = TSK_LOADING;
	task->first_thread = NULL;
	task->num_threads = 0;

	task->io_event_src.fs_service = io_get_stdfs_process();
	task->io_event_src.file_id = -1;
	task->io_event_src.port = STDFSS_PORT;
	task->io_event_src.smo = -1;
	task->io_finished.callback = NULL;
	task->killed_threads = 0;

	task->loader_inf.full_path = path;
	task->loader_inf.path_len = plength;
	task->loader_inf.stage = LOADER_STAGE_ELF_HDR;	// First Stage is ELF Header retrieval
	task->loader_inf.elf_pheaders = NULL;
    task->loader_inf.param = param;

	/* Reset VMM information on task */
	// do not invoke vmm_init_task_info, it would NULL our 
    // directory and it was invoked on tsk_create.
    task->vmm_info.page_count = 2;
	task->vmm_info.max_addr = PMAN_TSK_MAX_ADDR;
    
    /* Create microkernel task */
	mk_task.mem_adr = (ADDR)SARTORIS_PROCBASE_LINEAR;  /* paging mapped the memory here */
	mk_task.size = PMAN_TASK_SIZE;						/* task size is 4GB - kernel space size (0x800000) */
	mk_task.priv_level = tsk_priv[type];       

	if(create_task(task->id, &mk_task))
    {
		pman_print_and_stop("Could not create task %i ", task->id);
        return PM_ERROR;
    }
    /* Pagein the Page directory on task address space */
    pm_page_in(task->id, 0, (ADDR)LINEAR2PHYSICAL(task->vmm_info.page_directory), 0, 0);
    
    /* Get a Page Table for the task */
	pg_address = vmm_get_tblpage(task->id, SARTORIS_PROCBASE_LINEAR);
    
	if(pg_address == NULL) 
	{
		vmm_put_page(task->vmm_info.page_directory);
		task->vmm_info.page_directory = NULL;
		return PM_NOT_ENOUGH_MEM;
	}

	pm_page_in(task->id, (ADDR)SARTORIS_PROCBASE_LINEAR, (ADDR)LINEAR2PHYSICAL(pg_address), 1, PGATT_WRITE_ENA);

    // we will only open programs
	if(flags != LOADER_CTASK_TYPE_SYS)
	{
		/* start file open */
		task->io_finished.callback = loader_fileopen_finished;
		task->io_finished.params[0] = 0;
		task->io_finished.params[1] = 0;

		io_begin_open(&task->io_event_src, task->loader_inf.full_path, task->loader_inf.path_len, IO_OPEN_MODE_READ);
	}

	return PM_OK;
}

/*
This function will be invoked when an interpreter finished loading
a task.
*/
void loader_ready(struct pm_task *task)
{
    claim_mem(task->loader_inf.phdrs_smo);
}

/* This function will return file possition to read based on the elf program headers.
if address is not on the file, it'll return FALSE, else TRUE, and out_pos and our_size will be valid.
NOTE: The way this has been made, we will require sections to be page aligned.
*/
BOOL loader_filepos(struct pm_task *task, ADDR linear, UINT32 *outpos, UINT32 *outsize, INT32 *perms, INT32 *page_displacement, BOOL *exec)
{
	BYTE *headers = task->loader_inf.elf_pheaders;
	UINT32 i = 0, aligned_seg_end, section_offset, size;
	struct Elf32_Phdr *phdr = NULL;

	linear = (ADDR)((UINT32)linear - SARTORIS_PROCBASE_LINEAR);
	
	*page_displacement = 0;

	while(i < task->loader_inf.elf_header.e_phnum)
	{
		phdr = (struct Elf32_Phdr*) &headers[i * task->loader_inf.elf_header.e_phentsize];

		// consider only loadable segments
		if(phdr->p_type != PT_LOAD)
		{
			i++;
			continue;
		}

		// align (to the next page boundary) the end of the segment, in order to load a page if it has something
		// on file
		aligned_seg_end = (UINT32)phdr->p_vaddr + (UINT32)phdr->p_memsz;
		aligned_seg_end += (0x1000 - aligned_seg_end % 0x1000);

		/* See if address is inside the segment */
		if((UINT32)phdr->p_vaddr <= (UINT32)linear && (UINT32)linear < aligned_seg_end)
		{
			/*	ok linear falls on this segment, now
				check wether we must or not read from 
				the file
			*/
			// Section offset is our address substracting the begining (in memory) of the section 
			// and page aligned (down)
			section_offset = ((UINT32)linear - (UINT32)phdr->p_vaddr);
			section_offset -= section_offset % 0x1000;

			if( ((UINT32)phdr->p_vaddr % 0x1000 != 0) && (section_offset == 0) )
			{
				*page_displacement = phdr->p_offset % 0x1000;
			}

			if(section_offset < phdr->p_filesz)
			{
				/* Calculate outpos and outsize */
				*outpos = phdr->p_offset + section_offset;

				size = phdr->p_filesz - section_offset;

				if(size - *page_displacement > 0x1000)
					*outsize = 0x1000 - *page_displacement;
				else
					*outsize = size - *page_displacement;	
				
                if(phdr->p_flags & PF_EXEC)
                    *exec = TRUE;
                else
                    *exec = FALSE;

				if(phdr->p_flags & PF_WRITE)
					*perms = PGATT_WRITE_ENA;
				else
					*perms = 0;	// I think this allows only reading in sartoris :)
				
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		i++;
	}
	return FALSE;
}


/* This function will return TRUE if there is an executable section
overlapping the provided interval with readonly or executable flag.
NOTE: The way this has been made, we will require sections to be page aligned.
*/
BOOL loader_collides(struct pm_task *task, ADDR lstart, ADDR lend)
{
	BYTE *headers = task->loader_inf.elf_pheaders;
	UINT32 i = 0;
	struct Elf32_Phdr *phdr = NULL;

	lstart = (ADDR)((UINT32)lstart - SARTORIS_PROCBASE_LINEAR);
	while(lstart < lend - SARTORIS_PROCBASE_LINEAR)
    {
	    while(i < task->loader_inf.elf_header.e_phnum)
	    {
		    phdr = (struct Elf32_Phdr*) &headers[i * task->loader_inf.elf_header.e_phentsize];

		    // consider only loadable segments
		    if(phdr->p_type == PT_LOAD 
                && ((phdr->p_flags & PF_EXEC) || ((phdr->p_flags & PF_READ) && !(phdr->p_flags & PF_WRITE)))
                && ((UINT32)phdr->p_vaddr <= (UINT32)lend 
                && (UINT32)lstart < (UINT32)phdr->p_vaddr + (UINT32)phdr->p_memsz))
            {
                return TRUE;
            }
            
		    i++;
	    }
        lstart += PAGE_SIZE;
    }
	return FALSE;
}

/* 
Callback Function Raised when open operation is finished for a task executable file 
*/
INT32 loader_fileopen_finished(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);
    struct pm_msg_response res_msg;

	if(task && !(task->flags & TSK_FLAG_FILEOPEN)) 
	{		
		if(ioret == IO_RET_OK) 
		{
			task->io_event_src.file_id = task->io_finished.params[0];
			task->flags |= TSK_FLAG_FILEOPEN;

			/* Begin elf loading */
			if(elf_begin(task, io_begin_read, io_begin_seek) == -1)
            {
                res_msg.pm_type = PM_CREATE_TASK;
				res_msg.req_id  = task->command_inf.req_id;
				res_msg.status  = PM_INVALID_FILEFORMAT;
				res_msg.new_id  = task->id;
				res_msg.new_id_aux = 0;

				send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
            }
		} 
		else 
		{
            if(task->flags & TSK_SHARED_LIB)
            {
                // we where loading a shared lib..
                vmm_lib_loaded(task, FALSE);
            }
			else if(task->command_inf.creator_task_id != 0xFFFF)
			{
				/* Send Failure */
				res_msg.pm_type = PM_CREATE_TASK;
				res_msg.req_id  = task->command_inf.req_id;
				res_msg.status  = PM_IO_ERROR;
				res_msg.new_id  = task->id;
				res_msg.new_id_aux = 0;

				send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
                tsk_destroy(task);
			}
		}
	}
	return 1;
}

/*
Calculate expected working set for the task, based on ELF segments
*/
void loader_calculate_working_set(struct pm_task *task)
{
	UINT32 i = 0, new_end = 0;
	BYTE *headers = task->loader_inf.elf_pheaders;
	struct Elf32_Phdr *phdr = NULL;

	task->vmm_info.expected_working_set = 0;
	
	while(i < task->loader_inf.elf_header.e_phnum)
	{
		/* Get the header */
		phdr = (struct Elf32_Phdr*)&headers[i * task->loader_inf.elf_header.e_phentsize];

		// consider only loadable segments
		if(phdr->p_type != PT_LOAD)
		{
			i++;
			continue;
		}

		/* Calculate pages needed for this segment */
		task->vmm_info.expected_working_set += (UINT32)(phdr->p_memsz / 0x1000) + ((phdr->p_memsz % 0x1000 == 0)? 0 : 1);
		
		/* This is the end of the loadable segment */
		new_end = (UINT32)phdr->p_vaddr + (UINT32)phdr->p_memsz;

		if(task->tsk_bss_end < new_end)
			task->tsk_bss_end = new_end;
		
		i++;
	}	
}

BOOL loader_task_loaded(struct pm_task *task, char *interpreter)
{
    struct pm_msg_response res_msg;
    struct pm_task *ldtsk;

	res_msg.pm_type = PM_CREATE_TASK;
	res_msg.status  = PM_IO_ERROR;
	res_msg.new_id_aux = 0;

    /* Set expected_working_set with the size of ELF segments */
	loader_calculate_working_set(task);

	/* Check created task fits in memory */
	if(vmm_can_load(task))
	{				
		// finished loading
		task->state = TSK_NORMAL;
		task->loader_inf.stage = LOADER_STAGE_LOADED;

        if(interpreter != NULL)
        {
            // FIXME: We should load the interpreter reported on interpreter param
            // as a library instead of using an ld service...
            if(ld_task == -1)
            {
                pman_print_dbg("LD service was not loaded");

                /* Task cannot be loaded */
		        res_msg.status  = PM_NOT_ENOUGH_MEM;
		        send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
                    
                return FALSE;
            }
            else
            {
                // now tell vmm to create the library memory region
                if(!vmm_ld_mapped(task, PMAN_MAPPING_BASE, PMAN_MAPPING_BASE+ld_size))
                    return FALSE;

                ldtsk = tsk_get(ld_task);

                /* 
                We need to map LD onto the task address space.
                To do that, we will go through the LD task phdrs and page in
                every PT_LOAD section on it.
                */
                BYTE *headers = task->loader_inf.elf_pheaders;
	            UINT32 i = 0, phnum = task->loader_inf.elf_header.e_phnum;
                UINT32 laddr_ld, laddr, end_addr;
                int att;
	            struct Elf32_Phdr *phdr = NULL;
                struct vmm_page_directory *lddir = ldtsk->vmm_info.page_directory,
                                          *dir = task->vmm_info.page_directory;
	            struct vmm_page_table *ldtbl, *tbl;
                ADDR pg;

                laddr = PMAN_MAPPING_BASE;
                        
	            while(i < phnum)
	            {
		            phdr = (struct Elf32_Phdr*)&headers[i * task->loader_inf.elf_header.e_phentsize];

		            // consider only loadable segments
		            if(phdr->p_type == PT_LOAD)
                    {
                        laddr += (UINT32)phdr->p_vaddr;
                        laddr_ld = (UINT32)phdr->p_vaddr;
                        end_addr = (UINT32)phdr->p_vaddr + (UINT32)phdr->p_memsz;
                        while(laddr_ld < end_addr)
                        {
                            // do we need a page table?
                            ldtbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(lddir->tables[PM_LINEAR_TO_DIR(laddr_ld)].b));
                            if(dir->tables[PM_LINEAR_TO_DIR(laddr)].ia32entry.present == 0)
                            {
                                // get a page for the table
                                pg =  (ADDR)vmm_get_tblpage(task->id, laddr);

					            if(pm_page_in(task->id, (ADDR)laddr, (ADDR)LINEAR2PHYSICAL(pg), 1, PGATT_WRITE_ENA) != SUCCESS)
                                {
                                    /* Task cannot be loaded */
		                            if(task->command_inf.creator_task_id != 0xFFFF)
                                    {
                                        res_msg.status  = PM_NOT_ENOUGH_MEM;
		                                send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
                                    }
                                    return FALSE;
                                }
                                task->vmm_info.page_count++;
                                tbl = (struct vmm_page_table*)pg;
                            }
                            else
                            {
                                tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(dir->tables[PM_LINEAR_TO_DIR(laddr)].b));
                            }
                            
                            // calculate attributes
                            att = 0;

                            if(ldtbl->pages[PM_LINEAR_TO_TAB(laddr_ld)].entry.ia32entry.rw)
                                att = PGATT_WRITE_ENA;
                            if(ldtbl->pages[PM_LINEAR_TO_TAB(laddr_ld)].entry.ia32entry.write_through)
                                att |= PGATT_WRITE_THR;
                            if(ldtbl->pages[PM_LINEAR_TO_TAB(laddr_ld)].entry.ia32entry.cache_disable)
                                att |= PGATT_CACHE_DIS;

                            // if it's a data segment, we need to give the program it's
                            // own copy of it
                            if(phdr->p_flags == PF_EXEC)
                            {
                                pm_page_in(task->id, (ADDR)laddr, (ADDR)PG_ADDRESS(ldtbl->pages[PM_LINEAR_TO_TAB(laddr_ld)].entry.phy_page_addr), 1, att);
                            }
                            else
                            {
                                pg = (ADDR)vmm_get_page(task->id, laddr);
 
                                pm_page_in(task->id, (ADDR)PG_ADDRESS(laddr), (ADDR)LINEAR2PHYSICAL(pg), 2, att);
		
                                /* copy the contents of the page */
                                unsigned int *src = (unsigned int*)PHYSICAL2LINEAR(PG_ADDRESS(ldtbl->pages[PM_LINEAR_TO_TAB(laddr_ld)].entry.phy_page_addr));
                                unsigned int *dst = (unsigned int*)pg;
                                int i;
                                for(i = 0; i < 0x400; i++)
                                {
                                    dst[i] = src[i];
                                    i++;
                                }

                                vmm_unmap_page(task->id, (UINT32)PG_ADDRESS(laddr));

		                        task->vmm_info.page_count++;
                            }
                                                        
                            laddr_ld += PAGE_SIZE;
                            laddr += PAGE_SIZE;
                        }
                    }
		            i++;
                }
                                
                task->flags |= TSK_DYNAMIC;

                task->loader_inf.phdrs_smo = share_mem(task->id, task->loader_inf.elf_pheaders, task->loader_inf.elf_header.e_phnum * task->loader_inf.elf_header.e_phentsize, READ_PERM);

                return TRUE;
            }
        }
        
        /* Task loaded successfully */
		if(task->command_inf.creator_task_id != 0xFFFF)
        {
            res_msg.status = PM_OK;
		    res_msg.new_id = task->id;
		    send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
        }
	}
	else
	{
		/* Task cannot be loaded onto memory */
		if(task->command_inf.creator_task_id != 0xFFFF)
        {
            res_msg.status  = PM_NOT_ENOUGH_MEM;
		    send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
        }
        return FALSE;
	}    
}

void loader_info_init(struct loader_info *loader_inf)
{
	loader_inf->elf_pheaders = NULL;
	loader_inf->file_size = 0;
	loader_inf->full_path = NULL;
	loader_inf->path_len = 0;
	loader_inf->stage = 0;
    loader_inf->phdrs_smo = -1;
}
