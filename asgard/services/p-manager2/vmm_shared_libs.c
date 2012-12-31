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
#include "vm.h"
#include "types.h"
#include "task_thread.h"
#include "kmalloc.h"
#include "helpers.h"
#include "loader.h"
#include "vmm_memarea.h"
#include <services/stds/stdfss.h>

BOOL vmm_ld_mapped(struct pm_task *task, UINT32 vlow, UINT32 vhigh)
{
    struct vmm_memory_region *mreg = NULL;

    // I'll only create a memory region for LD on the task..
    // I won't check collitions because this is always the first lib loaded.
    mreg = kmalloc(sizeof(struct vmm_memory_region));

    if(!mreg) return FALSE;

    if(!rb_free_value(&task->vmm_info.regions_id, &mreg->tsk_id_node.value))
	{
		kfree(mreg);
		return FALSE;
	}

    mreg->owner_task = task->id;
    mreg->next = mreg->prev = NULL;
	mreg->tsk_node.high = TRANSLATE_ADDR(vhigh, UINT32);
	mreg->tsk_node.low = TRANSLATE_ADDR(vlow, UINT32);
	mreg->flags = VMM_MEM_REGION_FLAG_NONE;
	mreg->type = VMM_MEMREGION_LIB;
    mreg->descriptor = NULL;

    ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
    rb_insert(&task->vmm_info.regions_id, &mreg->tsk_id_node, FALSE);

    return TRUE;
}

/*
This function will return:
- 0 if a common page must be assigned
- 1 if mapping was done
- 2 if thread must be blocked
*/
int vmm_page_lib(struct pm_task *task, struct pm_thread *thread, ADDR proc_laddr, struct vmm_memory_region *mreg)
{
    struct pm_task *ltask = tsk_get(((struct vmm_slib_descriptor*)mreg->descriptor)->task);
    UINT32 ld_addr = TRANSLATE_ADDR(PG_ADDRESS(proc_laddr - mreg->tsk_node.low),UINT32);
    BOOL exec = 0;
    UINT32 filepos, readsize;
	INT32 perms, page_displacement;
    ADDR page_addr;
    struct vmm_page_table *tbl;
    
    /* See if it's a loadable page, if not, skip this */
    if(loader_filepos(ltask, (ADDR)ld_addr, &filepos, &readsize, &perms, &page_displacement, &exec))
    {
        // FIXME: we should check the page is not on swap.
        /* See if the page is present on the lib task (only executable pages) */  
        if(exec)
        {
            if(ltask->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(ld_addr)].ia32entry.present == 0)
	        {
                // it does not have a page table.. give it one!
		        /* Get a Page and set taken */
		        page_addr = vmm_get_tblpage(ltask->id, PG_ADDRESS(ld_addr));
		
		        /* Page in the table on task linear space. */
		        pm_page_in(ltask->id, (ADDR)(ld_addr), (ADDR)LINEAR2PHYSICAL(page_addr), 1, PGATT_WRITE_ENA);

                tbl = (struct vmm_page_table*)page_addr;

		        ltask->vmm_info.page_count++;
	        }
            else
            {
                // table is present.. the page?
                tbl = (struct vmm_page_table*)PHYSICAL2LINEAR(PG_ADDRESS(ltask->vmm_info.page_directory->tables[PM_LINEAR_TO_DIR(ld_addr)].b));
                
                if(tbl->pages[PM_LINEAR_TO_TAB(ld_addr)].entry.ia32entry.present == 1)
                {
                    // map the page 
                    pm_page_in(task->id, (ADDR)(PG_ADDRESS(proc_laddr)), (ADDR)PG_ADDRESS(tbl->pages[PM_LINEAR_TO_TAB(ld_addr)].entry.phy_page_addr), 2, perms);

                    return 1; // return to the thread execution
                }
            }
        }

        /* See if there are other threads waiting for this shared lib page */                        
        if(rb_insert(&ltask->vmm_info.wait_root, &thread->vmm_info.pg_node, TRUE) == 2)
        {
            pman_print_dbg("vmm_page_lib Other Thread was waiting (lib) \n");
            sch_deactivate(thread);
            
			/* Page was being retrieved already for other thread! Block the thread.	*/
			thread->state = THR_BLOCKED;
			thread->flags |= THR_FLAG_PAGEFAULT;
			thread->vmm_info.fault_address = proc_laddr;
			thread->vmm_info.read_size = readsize;
			thread->vmm_info.page_perms = perms;
			thread->vmm_info.page_displacement = page_displacement;
			thread->vmm_info.fault_task = ltask->id;
            thread->vmm_info.pg_node.value = PG_ADDRESS(ld_addr);

			return 2;	// thread will remain on hold
		}
                    
        /*
        We will bring the page, as if it was a page fault on the library task,
        but once we have brought the page, we will check if the task is
        a shared library and page in the page on all the tasks of the threads
        waiting for it.
        */
        /* Thread must be blocked until data is read from the lib executable file */
		sch_deactivate(thread);

		thread->state = THR_BLOCKED;
		thread->flags |= THR_FLAG_PAGEFAULT;
		thread->vmm_info.fault_address = proc_laddr;
		thread->vmm_info.read_size = readsize;
		thread->vmm_info.page_perms = perms;
		thread->vmm_info.page_displacement = page_displacement;
        thread->vmm_info.fault_task = ltask->id;
        thread->vmm_info.pg_node.value = PG_ADDRESS(ld_addr);

		/* 
		Page will be granted before hand, so page stealing thread won't attempt to take our page table.
		We also set taken entry here.
		*/
		thread->vmm_info.page_in_address = vmm_get_page(ltask->id, PG_ADDRESS(proc_laddr));
        
		ltask->vmm_info.page_count++;
        
        // if exec is not 0, we are bringing a page for
        // an executable section of a shared library.
        if(exec)
        {
            vmm_set_flags(ltask->id, thread->vmm_info.page_in_address, FALSE, TAKEN_PG_FLAG_LIBEXE, TRUE);
        }

        /* IO Lock page */
        vmm_set_flags(task->id, thread->vmm_info.page_in_address, TRUE, TAKEN_EFLAG_IOLOCK | TAKEN_EFLAG_PF, TRUE);

		thread->io_finished.callback = vmm_elffile_seekend_callback;
		
        // switch the IO source to the lib task one
        io_set_src(&thread->io_event_src, &ltask->io_event_src);

		io_begin_seek(&thread->io_event_src, filepos);

        // we must insert the thread on the pg wait tree
        // in case another thread asks for the same page.
        rb_insert(&ltask->vmm_info.wait_root, &thread->vmm_info.pg_node, FALSE);
        
        return 2;
    }
    return 0;
}

extern int tsk_lower_bound[];
extern int tsk_upper_bound[];

int vmm_lib_load(struct pm_task *task, char *path, int plength, UINT32 vlow, UINT32 vhigh)
{
    struct vmm_slib_descriptor *lib = (struct vmm_slib_descriptor*)vmm_lib_get(path);
    struct vmm_memory_region *mreg = NULL;
    UINT16 libtask;
    int ret;
	struct pm_task *libtsk = NULL;

    pman_print_dbg("PM: vmm_lib_load %s l: %x h: %x\n", path, vlow, vhigh);

    vlow = TRANSLATE_ADDR(vlow,UINT32);
    vhigh = TRANSLATE_ADDR(vhigh,UINT32);

    if(ma_collition(&task->vmm_info.regions, vlow, vhigh))
        return FALSE;
        
    pman_print_dbg("PM: vmm_lib_load no collition\n");

    mreg = kmalloc(sizeof(struct vmm_memory_region));

    if(!mreg) return 0;

    if(!rb_free_value(&task->vmm_info.regions_id, &mreg->tsk_id_node.value))
	{
		kfree(mreg);
		return FALSE;
	}

    pman_print_dbg("PM: vmm_lib_load id %i\n", &mreg->tsk_id_node.value);

    if(!lib)
    {
        pman_print_dbg("PM: vmm_lib_load loading lib for the first time\n");

        lib = kmalloc(sizeof(struct vmm_slib_descriptor));

        if(!lib) 
        {
            kfree(mreg);
            return 0;
        }

        /* search for a free task id */
	    libtask = tsk_get_id(tsk_lower_bound[1], tsk_upper_bound[1]);
    
	    if(libtask == 0xFFFF) 
	    {
		    kfree(mreg);
            return 0;
        }
  
	    libtsk = tsk_create(libtask);
        if(libtsk == NULL)
        {
            kfree(mreg);
            return 0;
        }
	
        pman_print_dbg("PM: vmm_lib_load task created %i\n", libtask);

	    libtsk->flags = TSK_SHARED_LIB;
        libtsk->state = TSK_NORMAL;

	    if(path[plength-1] == '\0') plength--;
        	            
        lib->path = path;
        lib->references = 1;
        lib->regions = NULL;
        lib->task = libtask;
        lib->loaded = 0;

        // add it to the global loaded libs list
        if(vmm.slibs)
            vmm.slibs->prev = lib;
        lib->next = vmm.slibs;
        lib->prev = NULL;
        vmm.slibs = lib;

        mreg->owner_task = task->id;
        mreg->next = mreg->prev = NULL;
	    mreg->tsk_node.high = vhigh;
	    mreg->tsk_node.low = vlow;
	    mreg->flags = VMM_MEM_REGION_FLAG_NONE;
	    mreg->type = VMM_MEMREGION_LIB;
        mreg->descriptor = lib;

        lib->regions = mreg;

        pman_print_dbg("PM: vmm_lib_load invoke loader reg: %x\n", mreg);
   
	    ret = loader_create_task(libtsk, path, plength, (unsigned int)lib, 1, LOADER_CTASK_TYPE_LIB);

	    if(ret != PM_OK)
	    {
            tsk_destroy(libtsk);
		    kfree(mreg);
            return 0;
	    }
        
        pman_print_dbg("PM: vmm_lib_load loader create task ret OK\n");
        return 1;
    }

    pman_print_dbg("PM: vmm_lib_load lib is already loaded! ref: %i \n", lib->references);
    
    lib->references++;

    // complete the memory region for this task
    mreg->owner_task = task->id;
    mreg->tsk_node.high = vhigh;
	mreg->tsk_node.low = vlow;
	mreg->flags = VMM_MEM_REGION_FLAG_NONE;
	mreg->type = VMM_MEMREGION_LIB;
    mreg->descriptor = lib;

    lib->regions->next = mreg;
    mreg->prev = lib->regions;
    mreg->next = NULL;
	
    if(!lib->loaded)
    {
        pman_print_dbg("PM: vmm_lib_load lib is not loaded yet\n");
        return 1;
    }
    else
    {
        pman_print_dbg("PM: vmm_lib_load lib is loaded!\n");
         /* Add new_mreg on the task lists */
        ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
        rb_search(&task->vmm_info.regions_id, mreg->tsk_id_node.value);
    }

    return 2;
}

INT32 lib_fileclosed_callback(struct fsio_event_source *iosrc, INT32 ioret)
{
    // file was closed because of an error, destroy the task
    tsk_destroy(tsk_get(iosrc->id));
}

/*
This function will be invoked by the loader when pheaders have been read.
*/
BOOL vmm_lib_loaded(struct pm_task *libtask, BOOL ok)
{
    struct pm_task *task;
    struct pm_msg_response res_msg;
    struct vmm_memory_region *mreg, *nmreg;
    struct vmm_slib_descriptor *lib = (struct vmm_slib_descriptor*)task->loader_inf.param;

    if(ok)
        pman_print_dbg("PM: Lib ELF loaded OK\n");
    else
        pman_print_dbg("PM: Lib ELF load ERROR\n");

    res_msg.pm_type = PM_LOAD_LIBRARY;
	res_msg.req_id  = task->command_inf.command_req_id;
	res_msg.new_id  = task->id;
	res_msg.new_id_aux = 0;
        
    // finalize any pending creations
    if(ok && lib->regions)  // might have been unloaded while fetching it's headers because the creating task was destroyed
    {
        lib->loaded = TRUE;
        mreg = lib->regions;

        while(mreg)
        {            
            task = tsk_get(mreg->owner_task);

            pman_print_dbg("PM: Inserting region for task %i\n", task->id);
            
            ma_insert(&task->vmm_info.regions, &mreg->tsk_node);
            rb_insert(&task->vmm_info.regions_id, &mreg->tsk_id_node, FALSE);

            task->flags &= ~TSK_LOADING_LIB;

            /* Send success */
			res_msg.status  = PM_OK;
			send_msg(task->id, task->command_inf.command_ret_port, &res_msg );
		    
            mreg = mreg->next;
        }
    }
    else
    {
        mreg = lib->regions;

        while(mreg)
        {
            nmreg = mreg->next;

            task = tsk_get(mreg->owner_task);

            kfree(mreg);

            /* Send Failure */
			res_msg.status  = PM_ERROR;			            
			send_msg(task->id, task->command_inf.command_ret_port, &res_msg );
		    
            task->flags &= ~TSK_LOADING_LIB;

            if(task->flags & TSK_FLAG_FILEOPEN)
            {
                task->io_finished.callback = lib_fileclosed_callback;

		        /* Close the file */
		        io_begin_close(&task->io_event_src);
            }
            mreg = nmreg;
        }

        // remove the lib from the list
        if(lib->prev == NULL)
            vmm.slibs = lib->next;
        else
            lib->prev->next = lib->next;

        if(lib->next)
            lib->next->prev = lib->prev;

        kfree(lib);
    }
}

/*
Unload the specified lib. 
*/
BOOL vmm_lib_unload(struct pm_task *task, struct vmm_memory_region *mreg)
{
    struct vmm_slib_descriptor *lib = (struct vmm_slib_descriptor*)mreg->descriptor;
 
    if(lib == NULL)
    {
        ma_remove(&task->vmm_info.regions, &mreg->tsk_node);
        rb_remove(&task->vmm_info.regions_id, &mreg->tsk_id_node);
        kfree(mreg);
        pman_print_dbg("PMAN vmm_lib_unload: Lib is LD\n");
        return TRUE;    // this can only happen on the LD mapping
    }

    pman_print_dbg("PMAN vmm_lib_unload: Claim memory from task.\n");

    // claim memory from the referencing task
    vmm_claim_region(task, mreg);

    ma_remove(&task->vmm_info.regions, &mreg->tsk_node);
    rb_remove(&task->vmm_info.regions_id, &mreg->tsk_id_node);
    
    // fix regions list on the descriptor
    if(mreg->prev == NULL)
        lib->regions = mreg->next;
    else
        mreg->prev->next = mreg->next;
    if(mreg->next)
        mreg->next->prev = mreg->prev;

    kfree(mreg);

    lib->references--;

    pman_print_dbg("PMAN vmm_lib_unload: Lib references %i\n", lib->references);

    if(!lib->references && lib->loaded)
    {
        struct pm_task *ltask = tsk_get(lib->task);

        pman_print_dbg("PMAN vmm_lib_unload: Lib UNLOADING\n");

		/* Close the file but dont wait for a callback */
        struct stdfss_close msg_close;

	    msg_close.command  = STDFSS_CLOSE;
	    msg_close.thr_id   = -1;
	    msg_close.file_id  = ltask->io_event_src.file_id;
	    msg_close.ret_port = 50; // this port is clearly not open

	    send_msg(ltask->io_event_src.fs_service, STDFSS_PORT, &msg_close);

        // destroy the task
        tsk_destroy(ltask); // this should also free the path!

        // remove it from the loaded list
        if(lib->prev == NULL)
        {
            vmm.slibs = lib->next;
            vmm.slibs->prev = NULL;
        }
        else
        {
            lib->prev->next = lib->next;
        }
        if(lib->next != NULL)
            lib->next->prev = lib->prev;

        kfree(lib);

        pman_print_dbg("PMAN vmm_lib_unload: Lib FREED\n");
    }

    return TRUE;
}


/*
If the library is already loaded, this function will return a pointer to
it's descriptor.
*/
ADDR vmm_lib_get(char *path)
{
    struct vmm_slib_descriptor *lib;

    lib = vmm.slibs;

    while(lib != NULL)
    {
        if(strcmp(path,lib->path))
        {
            return lib;
        }
        lib = lib->next;
    }
    return NULL;
}
