# What is the Boot Process? #

We OS freaks (and mostly everyone) refer to the process of starting up a System as The _boot_ process. This process is made up of all the steps performed by the BIOS program and the Operating System in order to start the System.

Once the BIOS set's up most of the hardware and stuff, it will load the first 512 bytes from its selected boot media. Those bytes will be placed at a special location on physical memory and will be executed from the first position. That program is called _boot loader_ and will be in charge of loading the Operating System.

Sartoris takes care of loading itself by providing a two-stage boot loader which will locate the kernel on position 0 of memory and begin the microkernel initialization. Once it has loaded itself, sartoris will create a first [Task](Tasks.md) called the Init Task, whose _id_ will be `INIT_TASK_NUM`, defined on `kernel.h`.

## The Init Task ##

The Init Task, is the first task created on the system. Init Task memory will be initialized with the contents of the first megabyte next to the microkernel image on memory, and will be placed on a specific location on physical memory, defined by implementation. Sartoris will then create a [Thread](Threads.md) for that Task with _id_ `INIT_THREAD_NUM`, whose entry point will be position 0 of the Init Task memory, and begin execution of that thread.

It's important to notice that once the init thread has been loaded Sartoris will assume a _Passive_ role, leaving execution to the OS (or real-time app) on top of it, and acting only through System Calls.

When _Paging_ is enabled, the Init Task will be initially fully paged, up to the size specified by `INIT_SIZE` defined on the architecture dependant section of the Microkernel code.

Bare in mind, the next section on this page, is a very (very) technical one, concerning the boot process. If you are only interested on creating an OS and let Sartoris boot loader take care, without any understanding of what's happening under the hood, then you might want to go on an read the next chapter of the tutorial [Hello World!](OSChapter3.md).

# Boot Process Detailed (x86) #

On the i386 implementation, Sartoris provides a two-stage boot loader which complies to the Multiboot specification.

## The Bootloader ##

### Stage 1 ###

Sartoris boot sector code for the x86 architecture is defined on `arch/i386/boot/stage1.s`.

This stage of the boot loader, will use BIOS functions to load boot loader _Stage 1.5_ code. It will verify the boot media and check for LBA. If LBA is supported, it will begin coping from possition defined on `stage2_sector` variable (currently 1 sector ahead) using int `0x13` function `0x45`, if LBA is not supported, it will translate the value to CHS using Drive Geometry obtained by function 0x8 of int `0x13` and use function `0x2` of the same interrupt to read from media.

Destination memory address for stage 1.5 is defined by `stage1_5seg16` on `arch/i386/boot/stages.inc`.

### Stage 1.5 ###

This stage is only a a loader as the first one. It will use an encoded block-list to load _Stage 2_.

When executed, Stage 1 will have left the following:

  * ds Pointing to `bootseg16`
  * ds:si = segment:offset of disk address packet/drive geometry
  * dl = Boot drive num
  * Stack Pointing to `0xEFFF - 0x4`

This stage will issue read commands to the boot device, using int `0x13` as it was done by Stage 1, but with the block list located at possition `0x187` from the beggining of the stage.

The encoded blocklist format will be:

| Memory location | Name | Size | Default Value |
|:----------------|:-----|:-----|:--------------|
| `0x187` | `block_list_len` | `1 byte` | 1 |
| `0x188` | `block_list_start` | `6 x Block list nodes` | Initial Node |

Starting at `block_list_start` there will be a list of up to 20 nodes with the following structure:

```
struct list_node{
   int start,
   short length
}
```

By default Sartoris will complete the list with only one node, telling stage 1.5 loader to copy starting at lba 2 with a length of (7\*128)+2 512 bytes blocks.

As the blocklist is located at a known possition on Sartoris image, this could be changed to point to a different _Stage 2_ Loader, even when the Microkernel is compiled.

Copy procedure destination on memory, will be taken from `nextstage_seg`, which is set to `stage2seg16:0x0000`.

It's important to notice, that until now, we have been running on 16 bit (real) mode.

### Stage 2 ###

This stage is where most interesting things happen. Current Implementation is contained on `arch/i386/boot/stage2mem.s`.

When executed, Stage 1.5 will have left the following:

  * ds Pointing to `bootseg16`
  * ds:si = segment:offset of disk address packet/drive geometry
  * dl = Boot drive num
  * ax = Stage 2 image size

This stage will perform the following actions:

  * Check Multiboot header

> As you will see on next section, starting at the end of
> Stage 2, is Sartoris Multiboot Header. Stage 2 will verify
> the multiboot magic number (`0x1BADB002`).
  * Enable A20

> You can google for this, but basically what it does here
> is enable the A20 line on the processor, in order to
> remove the 1MB limit set by the processor for backward
> compatibility with _really_ older systems.
  * Calculate Memory Size

> Memory size will be calculated using the BIOS interrupt `0x12` or
> `0x15` function `0x88`.
> Here it will also ask the BIOS to provide a memory map issuing
> interrupt `0x15` Function `0xE820`.
  * Switch to Protected Mode

> In order to switch to protected mode the bootloader defines a GDT
> table and an IDT table.

> The loader will Disable Interrupts, and IDT table will be loaded with only one record with 0.

> GDT table will have the following structure:

| Dummy Descriptor | 0 |
|:-----------------|:--|
| Code segment descriptor | limit=ffff base\_adress=0 p-flag=1, dpl=0, s-flag=1 (non-system segment), type code exec/read g-flag=1, d/b bit=1 (limit mult by 4096, default 32-bit opsize), limit upper bits F |
| Data segment descriptor | limit=ffff, base\_adress=0, p-flag=1, dpl=0, s-flag=1, type read/write, g-flag=1, d/b bit=1, limit upper bits F |


> Once the IDT and GDT are loaded with `lidt` and `lgdt`, `PE` bit (bit 0) of `CR0` will be set to 1 in order to switch to protected mode.

  * Move the Kernel

> The Kernel will be moved to the location specified on the Multiboot header.

> Once the kernel is moved Machine will be left like this:

  * EAX contains the magic value 0x2BADB002
  * EBX contains the 32-bit physical address of the Multiboot information structure provided by the boot loader.
  * CS is a 32-bit read/execute code segment with an ofeset of 0 and a limit of 0xFFFFFFFF. The exact value is undefined.
  * DS,ES,es,GS,SS are 32-bit read/write data segment with an ofeset of 0 and a limit of 0xFFFFFFFF. The exact values are all undefined.
  * A20 gate is enabled.
  * CR0 Bit 31 (PG) is cleared. Bit 0 (PE) is set. Other bits are all undefined.
  * EFLAGS Bit 17 (VM) is cleared. Bit 9 (IF) is cleared. Other bits are all undefined.
  * All other processor registers and flag bits are undefined.

Its important to notice this Machine State is conformig to Multiboot specification as defined [here](http://www.gnu.org/software/grub/manual/multiboot/html_node/Machine-state.html#Machine-state).

## Sartoris Multiboot Loader ##

Sartoris Multiboot loader, is defined on `/arch/i386/boot/sartoris_loader.s`, and is the last step on the boot process, before jumping to Sartoris initialization.

Even though the other stages can be replaced by a Multiboot compliant boot loader, this is the first stage which cannot be replaced. It will check `eax` register contains the Multiboot Magic Number and die if it does not.

The second step, will be to load once more the GDT and IDT, because Multiboot does not guarantee they'll be loaded with something we can use. IDT and GDT used are the same we set on Stage 2 of the loader.

Third step of the loader is to move Multiboot info structure to the address defined by `multiboot_info_address`, and place the Memory Map (if present) next to it, updating its pointer on the structure after relocation.

Last steps are to move the Kernel image to address 0 (this is an important step, since doing this will kill the system BIOS) and the Init Task Image to `init_address`.

after the last step, stack register `esp` will be set to `stack_address` and the loader will jump to possition 0 on physical memory, that happens to be where the Kernel Image begins, and it's entry point.

## Kernel Initialization ##

Architecture specific Kernel initialization will begin when the architecture Independant code invokes `arch_init_cpu` function defined on `/arch/i386/kernel/main.c`.

This function will perform the following steps:

  * Program PIC controler

> PIC interrupts will be mapped to 0x20-0x27 (master PIC) and 0x28-0x2f (slave PIC). It will also set the PICs to expect software EOIs and disable interupts on both PICs by masking them.

  * Initialize GDT

> Kernel GDT will contain the following data:

| Kernel Code Segment | This segment begins at 0x0 and ends at 0xFFFFFFFF |
|:--------------------|:--------------------------------------------------|
| Kernel Data Segment | This segment begins at 0x0 and ends at 0xFFFFFFFF |
| High Memory segment | This is a segment for memory addresses 0xA0000 to 0xFFFFF |
| System Calls | Call gate descriptors for System Calls (`MAX_SCA`) |
| LDTs | LDT descriptors (4) |
| Global TSS | This will hold the global TSS descriptor |

  * Get Capabilities

> Capabilities for the processor will be obtained using the CPUID instruction.

  * Initialize Interrupts

> The Microkernel will initialize a new IDT table, mapping Intel exceptions to a predefined procedure which will dump CPU contents ans halt. All other interrupts will be disabled.

  * Create a Global TSS segment

> Sartoris will initialize the `global_tss` structure, setting `curr_state` to task 0 state. It will also set `esp0` to `KRN_DATA_SS`.

  * Create Init Task/Thread and Initialize Paging

> Init Task will be setup as follows:

```
	tsk.size = INIT_SIZE;  
	tsk.mem_adr = USER_OFFSET;
	tsk.priv_level = 0;
```

> Init thread:

```
	thr.task_num = INIT_TASK_NUM;
	thr.invoke_mode = PRIV_LEVEL_ONLY;
	thr.invoke_level = 1;
	thr.ep = 0x00000000;

	thr.stack = (INIT_SIZE - BOOTINFO_SIZE - 0x4);
```

  * Once Init Task and Thread are created, paging will be initialized by:
    * Build Kernel Page Table (0x0 to 0xA0000)
    * Set 0xA0000 to 0xFFFFF as present, setting `PG_CACHE_DIS` to disable cache on the video memory area.
    * Set up a page directory for the init Task
    * Set first Page table on the directory to the Kernel Page Table
    * Create a Page table, mapping init Physical Memory to `USER_OFFSET`.
    * Map last 64Kb to BootInfo Structure on the table.
    * Set the Table on the directory.
    * Set cr3 to the Physical address of the new Page directory, and enable Paging on CR0 by setting PG bit.

Next step on this tutorial is [Hello World!](OSChapter3.md)