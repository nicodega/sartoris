
#include "formats/elf.h"
#include "elf_loader.h"
#include "loader.h"
#include "task_thread.h"
#include "types.h"
#include <sartoris/syscall.h>
#include <services/stds/stdfss.h>
#include <services/pmanager/services.h>

static INT32 tsk_priv[] = { /* Proc */ 2, /* Serv */ 0 };

UINT32 loader_create_task(struct pm_task *task, char *path, UINT32 plength, UINT32 type, BOOL sysservice)
{
	struct task mk_task;
	ADDR pg_address;

	/* Get a Page for the Directory */
	task->vmm_inf.page_directory = (struct vmm_page_directory*)vmm_get_dirpage(task->id);

	if(task->vmm_inf.page_directory == NULL) 
		return PM_NOT_ENOUGH_MEM;
	
	/* if we got here, task id is valid, the task slot is free,
	the path got through fine (smo is ok) and we have enough
	memory at least for a new page directory */

	/* everything is fine. spawn the task, don't fly upside down and be happy! */

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

	/* Reset VMM information on task */
	task->vmm_inf.swap_free_addr = (ADDR)0xFFFFFFFF;
	task->vmm_inf.table_swap_addr = 0;
	task->vmm_inf.expected_working_set = 0;
	task->vmm_inf.swap_read_smo = -1;
	task->vmm_inf.regions.first = NULL;
	task->vmm_inf.regions.total = 0;
	task->vmm_inf.page_count = 2;
	task->vmm_inf.max_addr = PMAN_TSK_MAX_ADDR;
	task->vmm_inf.swap_page_count = 0;

	/* Create microkernel task */
	mk_task.mem_adr = (ADDR)SARTORIS_PROCBASE_LINEAR;  /* paging mapped the memory here */
	mk_task.size = PMAN_TASK_SIZE;						/* task size is 4GB - kernel space size (0x800000) */
	mk_task.priv_level = tsk_priv[type];       

	if(create_task(task->id, &mk_task, NO_TASK_INIT, 0))
		pman_print_and_stop("Could not create task %i ", task->id);

	/* Pagein the Page directory on task address space */
	pm_page_in(task->id, 0, (ADDR)LINEAR2PHYSICAL(task->vmm_inf.page_directory), 0, 0);

	/* Get a Page Table for the task */
	pg_address = vmm_get_tblpage(task->id, SARTORIS_PROCBASE_LINEAR);

	if(pg_address == NULL) 
	{
		vmm_put_page(task->vmm_inf.page_directory);
		task->vmm_inf.page_directory = NULL;
		return PM_NOT_ENOUGH_MEM;
	}

	pm_page_in(task->id, (ADDR)SARTORIS_PROCBASE_LINEAR, (ADDR)LINEAR2PHYSICAL(pg_address), 1, PGATT_WRITE_ENA);

	if(!sysservice)
	{
		/* start file open */
		task->io_finished.callback = loader_fileopen_finished;
		task->io_finished.params[0] = 0;
		task->io_finished.params[1] = 0;

		io_begin_open(&task->io_event_src, task->loader_inf.full_path, task->loader_inf.path_len, IO_OPEN_MODE_READ);
	}

	return PM_OK;
}

/* This function will return file possition to read based on the elf program headers.
if address is not on the file, it'll return 0, else 1, and out_pos and our_size will be valid; 
NOTE: The way this has been made, we will require sections to be page aligned.
*/
BOOL loader_filepos(struct pm_task *task, ADDR linear, UINT32 *outpos, UINT32 *outsize, INT32 *perms, INT32 *page_displacement)
{
	BYTE *headers = task->loader_inf.elf_pheaders;
	UINT32 i = 0, aligned_seg_end, section_offset, size;
	struct Elf32_Phdr *prog_header = NULL;

	linear = (ADDR)((UINT32)linear - SARTORIS_PROCBASE_LINEAR);
	
	*page_displacement = 0;

	while(i < task->loader_inf.elf_header.e_phnum)
	{
		prog_header = (struct Elf32_Phdr*) &headers[i * task->loader_inf.elf_header.e_phentsize];

		// consider only loadable segments
		if(prog_header->p_type != PT_LOAD)
		{
			i++;
			continue;
		}

		// align (to the next page boundary) the end of the segment, in order to load a page if it has something
		// on file
		aligned_seg_end = (UINT32)prog_header->p_vaddr + (UINT32)prog_header->p_memsz;
		aligned_seg_end += (0x1000 - aligned_seg_end % 0x1000);

		/* See if address is inside the segment */
		if((UINT32)prog_header->p_vaddr <= (UINT32)linear && (UINT32)linear < aligned_seg_end)
		{
			/*	ok linear falls on this segment, now
				check wether we must or not read from 
				the file
			*/
			// Section offset is our address substracting the begining (in memory) of the section 
			// and page aligned (down)
			section_offset = ((UINT32)linear - (UINT32)prog_header->p_vaddr);
			section_offset -= section_offset % 0x1000;

			if( ((UINT32)prog_header->p_vaddr % 0x1000 != 0) && (section_offset == 0) )
			{
				*page_displacement = prog_header->p_offset % 0x1000;
			}

			if(section_offset < prog_header->p_filesz)
			{
				/* Calculate outpos and outsize */
				*outpos = prog_header->p_offset + section_offset;

				size = prog_header->p_filesz - section_offset;

				if(size - *page_displacement > 0x1000)
					*outsize = 0x1000 - *page_displacement;
				else
					*outsize = size - *page_displacement;	
				
				if(prog_header->p_flags & PF_WRITE)
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


/* 
Callback Function Raised when open operation is finished for a task executable file 
*/
INT32 loader_fileopen_finished(struct fsio_event_source *iosrc, INT32 ioret)
{
	struct pm_task *task = tsk_get(iosrc->id);

	if(!(task->flags & TSK_FLAG_FILEOPEN)) 
	{		
		if(ioret == IO_RET_OK) 
		{
			task->io_event_src.file_id = task->io_finished.params[0];
			task->flags |= TSK_FLAG_FILEOPEN;

			/* Begin elf loading */
			elf_begin(task, io_begin_read, io_begin_seek);
		} 
		else 
		{
			if(task->command_inf.creator_task_id != 0xFFFF)
			{
				/* Send Failure */
				struct pm_msg_response res_msg;

				res_msg.pm_type = PM_CREATE_TASK;
				res_msg.req_id  = task->command_inf.req_id;
				res_msg.status  = PM_IO_ERROR;
				res_msg.new_id  = task->id;
				res_msg.new_id_aux = 0;

				send_msg(task->command_inf.creator_task_id, task->command_inf.response_port, &res_msg );
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
	struct Elf32_Phdr *prog_header = NULL;

	task->vmm_inf.expected_working_set = 0;
	
	while(i < task->loader_inf.elf_header.e_phnum)
	{
		/* Get the header */
		prog_header = (struct Elf32_Phdr*)&headers[i * task->loader_inf.elf_header.e_phentsize];

		// consider only loadable segments
		if(prog_header->p_type != PT_LOAD)
		{
			i++;
			continue;
		}

		/* Calculate pages needed for this segment */
		task->vmm_inf.expected_working_set += (UINT32)(prog_header->p_memsz / 0x1000) + ((prog_header->p_memsz % 0x1000 == 0)? 0 : 1);
		
		/* This is the end of the loadable segment */
		new_end = (UINT32)prog_header->p_vaddr + (UINT32)prog_header->p_memsz;

		if(task->tsk_bss_end < new_end)
			task->tsk_bss_end = new_end;
		
		i++;
	}	
}

void loader_info_init(struct loader_info *loader_inf)
{
	loader_inf->elf_pheaders = NULL;
	loader_inf->file_size = 0;
	loader_inf->full_path = NULL;
	loader_inf->path_len = 0;
	loader_inf->stage = 0;
}
