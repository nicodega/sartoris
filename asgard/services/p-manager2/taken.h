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


#ifndef PMANTAKENH
#define PMANTAKENH

/* Taken structure. */

/* Taken structure entries. (32 bits)

  There are three kind of entries:
	- Page taken: taken for a process address space.
	- Dir taken: taken for a process page directory.
	- Tbl taken: taken for a process page table.
*/
struct taken_pg
{
	unsigned int taken:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int flags:10;
	unsigned int tbl_index:16;	// table index, or region ID
} PACKED_ATT;

struct taken_pdir
{
	unsigned int taken:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int reserved:10;
	unsigned int taskid:16;
} PACKED_ATT;

struct taken_ptbl
{
	unsigned int taken:1;
	unsigned int dir:1;
	unsigned int tbl:1;
	unsigned int eflags:3;
	unsigned int dir_index:10;
	unsigned int taskid:16;
} PACKED_ATT;

/* A taken entry contains one of the previously defined structures. */
struct taken_entry
{
	union
    {
		unsigned int b;
		struct taken_pg b_pg;
		struct taken_ptbl b_ptbl;
		struct taken_pdir b_pdir;
    } data;
} PACKED_ATT;

struct taken_table
{
	struct taken_entry entries[1024];
};

struct taken_directory
{
	struct taken_table *tables[1024];
};

/* Flags field for PG entries */
#define TAKEN_PG_FLAG_PHYMAP	0x1     // physical map
#define TAKEN_PG_FLAG_SHARED	0x2     // shared page
#define TAKEN_PG_FLAG_FILE		0x4     // file mapping
#define TAKEN_PG_FLAG_PMAN		0x8     // taken for pman
#define TAKEN_PG_FLAG_LIBEXE	0x10    // taken for a library executable

/* eflags */
#define TAKEN_EFLAG_NONE		0x0
#define TAKEN_EFLAG_IOLOCK		0x1
#define TAKEN_EFLAG_SERVICE		0x2
#define TAKEN_EFLAG_PF			0x4



#endif
