# Introduction #

In order to compile Sartoris, you'll need a unix like system os if you use Windows, [cygwin](http://www.cygwin.com/) will do as well.

Sartoris is programmed on C and assembly languaje and it's compile process has been encapsulated on a GNU make script.

You need the following programs installed in your system:

  * GNU gcc
  * binutils
  * make
  * nasm, the Netwide Assembler.

You can also download the bochs IA32 emulator, in order to test your OS, as suggested on [this](OSTesting_Bochs.md) page.

For example, Under Debian (or Ubuntu), all you need are the build-essential, nasm and bochs-x packages. If you are using Cygwin, you need to get nasm, make and bochs, and you´ll also need an elf toolchain.

## Windows ##

On windows, theres no easy way (or at least we did not try hard enough ;)) to compile Satoris on Visual Studio IDE. However, it's quite simple to do so on a cygwin environment, with a proper elf toolchain for cross-compilation (you can download a toolchain [here](http://sartoris.googlecode.com/files/toolchain.zip)).

In order to specify the name of the compiler and linker sartoris must use to compile, you **must** modify `compenv.mk` located on `sartoris/arch/i386` as suggested [here](i386CompileEnv.md).

## Compiling ##

Once you've verified you have all the proper tools installed you must follow these two steps:

  * Run `./symlinks.sh` shell script located on sartrois base directory.
> > This will setup the target directory for compilation to i386, which is the only
> > implementation we are maintaining nowadays.

  * Run `make` while at sartoris base directory.
    * GNU make scripts located on sartoris directories will invoke the C compiler and the assembler generating sartoris architecture dependant and independant objects, and linking them onto `sartoris.img` located on `sartoris/arch/i386/sartoris`.
    * The compilation process will also create two user librares `kserv.o` and `kserv-user.o` containing a C interface to Sartoris System Calls. `kserv.o` contains system calls utilizad by Operating System services while `kserv-user.o` only contains those System Calls with privilege level 3 (i.e User programs).