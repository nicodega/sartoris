/*
*	Sartoris will leave a Multiboot info structure at possition 0x100000
*
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

/*
 *  Flags to be set in the flags field
 */

/* is there lower/upper memory information? */
#define MB_INFO_MEM			0x00000001
/* is there a boot device set? */
#define MB_INFO_BOOTDEV			0x00000002
/* is the command-line defined? */
#define MB_INFO_CMDLINE			0x00000004

#define MB_INFO_MMAP			0x00000040
#define MB_INFO_DRIVE_INF		0x00000080

#define MB_INFO_CONFIG_TBL		0x00000100

/* Is there a APM table?  */
#define MB_INFO_APM_TBL			0x00000400

struct multiboot_info
{
  /* MultiBoot info version number */
  unsigned long flags;
  
  /* Available memory from BIOS */
  unsigned long mem_lower;
  unsigned long mem_upper;
  
  /* "root" partition */
  unsigned long boot_device;
  
  /* Kernel command line */
  unsigned long cmdline;
  
  /* Boot-Module list */
  unsigned long mods_count;
  unsigned long mods_addr;
  
  union
  {
    struct
    {
      /* (a.out) Kernel symbol table info */
      unsigned long tabsize;
      unsigned long strsize;
      unsigned long addr;
      unsigned long pad;
    }
    a;
    
    struct
    {
      /* (ELF) Kernel section header table */
      unsigned long num;
      unsigned long size;
      unsigned long addr;
      unsigned long shndx;
    }
    e;
  }
  syms;
  
  /* Memory Mapping buffer */
  unsigned long mmap_length;
  unsigned long mmap_addr;
  
  /* Drive Info buffer */
  unsigned long drives_length;
  unsigned long drives_addr;
  
  /* ROM configuration table */
  unsigned long config_table;
  
  /* Boot Loader Name */
  unsigned long boot_loader_name;

  /* APM table */
  unsigned long apm_table;

  /* Video */
  unsigned long vbe_control_info;
  unsigned long vbe_mode_info;
  unsigned short vbe_mode;
  unsigned short vbe_interface_seg;
  unsigned short vbe_interface_off;
  unsigned short vbe_interface_len;
};

struct mmap_entry
{
	unsigned long size;
	unsigned long long start;
	unsigned long long end;
	unsigned long type;
};

#define MULTIBOOT_MMAP_AVAILABLE 1
