
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
INT32 elf_readph_finished_callback(struct fsio_event_source *iosrc, INT32 ioret);
INT32 elf_seek_finished_callback(struct fsio_event_source *iosrc, INT32 ioret);

#endif
