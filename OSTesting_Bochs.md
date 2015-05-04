# How do I test my OS? #

Since [Bochs](http://bochs.sf.net) IA-32 Emulator is very complex, we will only explain how to configure it, to load a custom floppy image.

Once you compiled you operating system, and created a floppy disk image, you should configure bochs creating a `.bochsrc` configuration file, modifying these lines:
```
floppya: 1_44=1.44a, status=inserted
```
Where `1.44a` is the path to the disk image you created with your OS.

You should also set bochs to boot from the floppy drive:
```
boot: floppy 
```
When installing bochs, it will generate a demo `.bochsrc` configuration file, on which you can modify these parameters.

Once these is changed, all you have to do is run bochs `bochs_install_dir/bochs`.