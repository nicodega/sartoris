
#ifndef MEMLAYOUTH
#define MEMLAYOUTH

/*********************************************

Memory Layout Defines 

**********************************************/

/* PMANSIZE WILL BE SENT AS A COMPILATION PARAMETER */
#ifndef PMAN_SIZE
#define PMAN_SIZE 4
#endif

#define PMAN_DEFAULT_MEM_SIZE		0x2000000                   // Default memory size = 32 MB 

#define SARTORIS_PROCBASE_LINEAR	0x800000                    // base linear address used by sartoris

/* Task defines */
#define PMAN_TASK_SIZE				(0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR)    // 4GB less base
#define PMAN_THREAD_STACK_BASE		0x7FFFFFFF   				// 2GB 
#define PMAN_MAPPING_BASE			0x80000000					// 2GB this is where we will start mapping libs or other stuff
#define PMAN_TSK_MAX_ADDR			(0xFFFFFFFF - SARTORIS_PROCBASE_LINEAR) //(0x40000000 + SARTORIS_PROCBASE_LINEAR)	   // 1 GB

/* Memory regions size */
#define PMAN_CODE_SIZE				0x200000					// 2 MB for pman code
#define PMAN_INITIMG_SIZE			0x100000					// size of init images region 
#define PMAN_MULTIBOOT_SIZE			0x10000						// size of the multiboot region
#define PMAN_MALLOC_SIZE			(((0x400000 - PMAN_MULTIBOOT_SIZE) - PMAN_CODE_SIZE))

/* Physical Memory Layout */
#define PMAN_PHYS_BASE				0x100000						          	  // where sartoris and non available memory ends
#define PMAN_PDIR_PHYS				PMAN_PHYS_BASE						          // PMAN page directory location
#define PMAN_PTBL_PHYS				(PMAN_PDIR_PHYS + PAGE_SIZE)			      // PMAN first page table location
#define PMAN_CODE_PHYS				(PMAN_PTBL_PHYS + PAGE_SIZE)			      // PMAN code start
#define PMAN_MULTIBOOT_PHYS			(PMAN_CODE_PHYS + PMAN_CODE_SIZE)	          // physical location of multiboot information
#define PMAN_MALLOC_PHYS			(PMAN_MULTIBOOT_PHYS + PMAN_MULTIBOOT_SIZE);
#define PMAN_POOL_PHYS				(0x400000 + PMAN_CODE_PHYS)	                  // the pool will begin at the end of the first PMAN 4MB
#define PMAN_INIT_RELOC_PHYS		(PMAN_POOL_PHYS + 0x400000)	                  // where we will relocate the init images 
																                  // (if this is changed stage 2 copy procedure must be changed too)

#define PMAN_LOWMEM_PHYS			(PMAN_INIT_RELOC_PHYS + PMAN_INITIMG_SIZE)	  // where we can begin assigning memory, under 16mb
#define PMAN_HIGH_MEM_PHYS			0x1000000					                  // 16 MB and up

#define PMAN_SARTORIS_MMAP_PHYS		0x100000	// where bootinfo is left by sartoris on physical memory
#define PMAN_SARTORIS_MMAP_LINEAR	0x3F0000	// where sartoris maps bootinfo on init task
#define PMAN_SARTORIS_INIT_PHYS		0x800000	// where sartoris left the init image

/* PMAN linear memory layout */
#define PMAN_BASE_LINEAR			0x0							                    // where pman code begins (from pman point of view)
#define PMAN_MULTIBOOT_LINEAR		(PMAN_BASE_LINEAR + PMAN_CODE_SIZE)				// multiboot linear position
#define PMAN_MALLOC_LINEAR			(PMAN_MULTIBOOT_LINEAR + PMAN_MULTIBOOT_SIZE)	// where malloc buffer can start
#define PMAN_POOL_LINEAR			(PMAN_MALLOC_LINEAR + PMAN_MALLOC_SIZE)         // this will be exactly at 0x400000	(from sartoris point of view)		

#define PMAN_STAGE1_MAPZONE			0x300000	// were we will map on stage 1 for copy (linear)

#define PMAN_STAGE2_MAPZONE_SOURCE	0x201000	// were we will map on stage 2 for copy (linear)
#define PMAN_STAGE2_MAPZONE_DEST	0x202000	// were we will map on stage 2 for copy (linear)

/* PMAN defines */
#define PMAN_CODE_PAGES				(PMAN_CODE_SIZE / PAGE_SIZE)
#define PMAN_MULTIBOOT_PAGES		(PMAN_MULTIBOOT_SIZE / PAGE_SIZE)
#define PMAN_MALLOC_PAGES			(PMAN_MALLOC_SIZE / PAGE_SIZE)

#define PMAN_INTCOMMON_STACK_ADDR	(PMAN_CODE_SIZE - 0x10000)
#define PMAN_STEALING_STACK_ADDR	(PMAN_CODE_SIZE - 0x8000)
#define PMAN_AGING_STACK_ADDR		(PMAN_CODE_SIZE - 0x6000)	
#define PMAN_PGSTACK_ADDR			(PMAN_CODE_SIZE - 0x4000)
#define PMAN_STACK_ADDR				(PMAN_CODE_SIZE - 0x2000)
#define PMAN_EXSTACK_ADDR			(PMAN_CODE_SIZE)


#endif