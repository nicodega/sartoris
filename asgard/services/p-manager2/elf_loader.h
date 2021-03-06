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

#ifndef ELFLOADERH
#define ELFLOADERH

INT32 elf_begin(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset));
INT32 elf_seekphdrs(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset));
INT32 elf_readphdrs(struct pm_task *task, INT32 (*ioread)(struct fsio_event_source *iosrc, UINT32 size, ADDR ptr), INT32 (*ioseek)(struct fsio_event_source *iosrc, UINT32 offset));
INT32 elf_check_header(struct pm_task *task);
INT32 elf_check(struct pm_task *task);

INT32 elf_read_finished_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 elf_readh_finished_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 elf_seek_finished_callback(struct fsio_event_source *iosrc, INT32 ioret);

#endif
