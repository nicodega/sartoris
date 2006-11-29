
#ifndef LOADERH
#define LOADERH

#include "types.h"
#include "formats/elf.h"

#define LOADER_STAGE_ELF_HDR    1   /* elf header is being retrived				           */
#define LOADER_STAGE_ELF_PHDR   2   /* elf program headers are being retrieved	           */
#define LOADER_STAGE_LOADED     3   /* Task has already been loaded						   */

#define PMAN_STAGE_INITIALIZING 1
#define PMAN_STAGE_RUNING       2

int pman_stage;

/* Executable info structure Holds information on executable file */
struct loader_info
{
	char *full_path;				   /* Executable File Path          */
	UINT16 path_len;
	UINT8 stage;
	UINT32 file_size;                  /* Size of the Executable File   */

	/* ELF Specific */
	struct Elf32_Ehdr elf_header;	   /* The elf header of the process */
	unsigned char *elf_pheaders;	   /* elf program headers           */
	
} PACKED_ATT;

void loader_info_init(struct loader_info *loader_inf);
void loader_init(ADDR initfs_address);
UINT32 loader_create_task(struct pm_task *task, char *path, UINT32 plength, UINT32 type, BOOL sysservice);
INT32 loader_fileopen_finished(struct fsio_event_source *iosrc, INT32 ioret);
void loader_calculate_working_set(struct pm_task *task);
BOOL loader_filepos(struct pm_task *task, ADDR linear, UINT32 *outpos, UINT32 *outsize, INT32 *perms, INT32 *page_displacement);

#endif


