# Introduction #

On this section we describe what you need to change in order to specify which compiler, linker and assembler you want to use when compiling Sartoris.

# Configuration #

In order to make things easy, we created a file named `compenv.mk` on `sartoris/arch/i386`, containing the name and path of applications being used on the kernel build process.

Configurable parameters on the file are:

| **Param Name** | **Description** | **Default Value** |
|:---------------|:----------------|:------------------|
| **CC** | Path and/or name of the C compiler. | cc |
| **LD** | Path and/or name of the Linker | ld |
| **AS** | Path and/or name of the assembler. | nasm |
| **CFLAGS** | This will be the flags set when invoking the C compiler | `-O2 -D FPU_MMX -D _METRICS_ -D PAGING` |
| **ASFLAGS** | This will be the flags set when invoking the assembler. | `-f elf -d FPU_MMX -D _METRICS_ -D PAGING` |

## Flags ##

There are a few flags which can be specified at compile time in order to provide certain functionalities on the microkernel. Such flags should be appended to ASFLAGS **and** CFLAGS after `- D`.

Defined Flags are:

| **Flag Name** | **Description** |
|:--------------|:----------------|
| **PAGING** | This flag provides paging functions on the microkernel. If its not set, microkernel wont use paging. |
| **FPU\_MMX** | This flag provides on-demand preservation of FPU, MMX and SSEn state on the microkernel. If its not set, microkernel wont preserve it's state ever. |
| **`_METRICS_`** | When this flag is set `get_metrics` syscall will be compiled. setting it might slow down some operations a bit on a future. |