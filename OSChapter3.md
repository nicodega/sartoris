# Introduction #

On last chapter, we explained the boot process in a technical perspective. Now let us create the clasic _Hello World!_ example! (if you have not setup a sartoris compilation environment I suggest you do now. [Here](Compiling.md) explains how to do it).

**SETUP**: You can download this example sources here:  [hello\_world.zip](http://sartoris.googlecode.com/files/hello_world.zip)

**IMPORTANT**: Make sure you extract the contents of the zip files so that sartoris folder can be located by using `../sartoris` from the example code directory (where the Makefile is).

As we explained before, the first [Thread](Threads.md) executed on the system is the Init Thread.

For this example we will create a C file named `start.c` with the following code:

```
#include "screen.h"
#include <sartoris/syscall.h>

int main() 
{
	clear_screen();
	cursor_off();

	string_print("hola, mundo!", 0, 0x7);

	while(1);
}
```

We want this code to be executed as the Init Thread, and that's why it has to end with `while(1)` (if it didn't, it would attempt to return from init, and that's not right).

Since Sartoris copies itself over the BIOS and interrupts are not mapped to BIOS functions anymore, we need a way to display our nice spanish "Hello World!" message. `screen.h` and `screen.c` are VGA text printing routines which will access the VGA IO registers and using the Hi-Mem segment they'll print the text.

In order to generate our example, you'll have to compile Sartoris first. You should type `make` on sartoris directory to generate `sartoris.img` containing the microkernel image. Once Sartoris is compiled, you must run `make` this time on `hello_world` directory.

This is the output you should get:

```
gcc -c -I"../sartoris/include" -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -s start.c -o start.o
nasm -f elf screen.s
ld --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N -o bin/code.img start.o screen.o
cp ../sartoris/arch/i386/sartoris/sartoris.img bin/boot.img
cat bin/code.img >> bin/boot.img
cp bin/boot.img bochs/1.44a
./bochs/fill bochs/1.44a -m 1474048
Size: 32186 bytes
1441862 bytes will be added
1+0 records in
1+0 records out
1441862 bytes (1.4 MB) copied, 0.011 s, 131 MB/s
```

This will generate a file named `1.44a` on `hello_world/bochs` which you can [run on bochs](OSTesting_Bochs.md). If you do, you'll see this:

![http://sartoris.googlecode.com/svn/images/hello_world.jpg](http://sartoris.googlecode.com/svn/images/hello_world.jpg)

As you can see, our little "Hello world!" example worked as a charm, and with no effort at all!

## Behind the Scenes ##

Ok... that was good!... but, how did we get Sartoris to load our function as a thread?!

Lets look closer at the compilation output.

First we have a `gcc` invocation:
```
gcc -c -I"../sartoris/include" -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -s start.c -o start.o
```
The -O2 switch tells `gcc` to optimize the code generated for the example and is not rare to use it when compiling other projects, so if you are a programmer you should understand what this does.

The other switches tell `gcc` that the code will run independant from any library (`-ffreestanding`), without using the Standard Includes (`-nostdinc`) nor the Standard C Library (`-nostdlib`) and last but not least, we tell `gcc` not to use Start Files like `cr0` (`-nostartfiles`).

These switches are necesary, because none of these things can be used when runing a standalone process like this one, since all of them are OS dependant.

If we look at the linker output:
```
ld --oformat binary --entry 0x00000000 --Ttext 0x00000000 -N -o bin/code.img start.o screen.o
```

Since the Init thread will be run directly, we cant handle ELF or COFF formated executable code, and that's why we use `--oformat binary`, so the output is just plain binary code.

On [Chapter 2](OSChapter2.md) of this tutorial, you saw Sartoris creates the Init Thread with entry point on byte 0 of the Init Task, and that's why we tell `ld` to generate the entry point at possition 0 of the Text segment, and place that segment also starting at 0x0.

With this, the result is a compiled image of our code, where `main` function starts at position 0x0 of the image.

The remaining lines on the compilation script:
```
cp ../sartoris/arch/i386/sartoris/sartoris.img bin/boot.img
cat bin/code.img >> bin/boot.img
cp bin/boot.img bochs/1.44a
./bochs/fill bochs/1.44a -m 1474048
```
Will copy Sartoris Microkernel image onto a file named boot.img and append our generated image for this little process at the end of it.

Since the Init Task code starts where the kernel image finishes, our code will be the one loaded, and runned at position 0x0, where `main` function begins.

Next step on this tutorial is [Creating Task and Threads](OSChapter4.md)