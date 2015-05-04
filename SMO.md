# What is an SMO? #

On Sartoris, each [Task](Tasks.md) has it's unique Linear Address Space, and it's memory regions cannot be seen by other tasks, unless the operating system manually maps some part of the task address space onto another Task Address Space, using page aliasing.

Although the Page Mapping method seemes quite interesting, it would require the microkernel to control paging of memory areas for user and system services. Since Sartoris is ment to be as unobtrusive as it can, it only supports memory sharing by copying, instead of pages mapping.

Shared Memory Objects (SMOs) are the way Sartoris implements inter-task memory access. When a Task wants to let another task read a memory portion of it's address space, it will create a shared memory object with read permissions for the other task. Using that SMO, the other task will be able to read the contents of that region.

# Using SMOs #

The system call `int share_mem(int target_task, void *addr, int size, int perms)` creates a SMO of size `size` bytes at offset `addr` of the current task's address space, that can be accessed by the task (What is meant here is that it can be accessed by any thread of the corresponding task.) `target_task`. The parameter `perms` indicates if access is granted for reading, writing, or both. The function returns an `id` number that identifies the SMO just created, or `-1` in case of failure.

SMOs can be destroyed using the system call `int claim_mem(int smo_id)` and the target task of an SMO can pass it over to another task using the system call `int pass_mem(int smo_id, int target_task)`, which changes the target task to the supplied parameter.

The system call `int read_mem(int smo_id, int off, int size, void *dest)` copies the `size` bytes at offset `off` of the SMO identified by `smo_id` to the address `dest`.

Conversely, the system call `int write_mem(int smo_id, int off, int size, void *src)` copies `size` bytes from address `src` from the current task's address space to offset `off` of the SMO identified by `smo_id`. Of course, in both cases the current task must be the target task of the SMOs, and have the right permission.

All the memory-sharing system calls excepting `share_mem` return 0 in case of success and -1 otherwise.

## Kernel Internals ##

Memory sharing functions are the only functions that need to access the address space of a task distinct from the currently executing one. This must be resolved by each platform implementation, providing functions `arch_cpy_from_task` and `arch_cpy_to_task` that perform inter-task address translation.