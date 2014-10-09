atr2imd:

	Convert Nick Kennedy's .ATR (Atari) disk image file format to
Dave Dunfield's .IMD (ImageDisk) file format


imd2atr:

	Convert Nick Kennedy's .ATR (Atari) disk image file format to
Dave Dunfield's .IMD (ImageDisk) file format


You could use these to read and write Atari 800 disks using an IBM PC floppy
drive with ImageDisk.  Note however that the floppy drive should be adjusted
for 288 RPM instead of 300 RPM.


Compiling instructions:

I use the DJGPP 32-bit GNU-C based compiler: http://www.delorie.com/djgpp/
(so you need a 386 or better machine to run these on)

	gcc -o atr2imd.exe atr2imd.c

	gcc -o imd2atr.exe imd2atr.c

Then I use CWSDPMI as the DOS extender: http://homer.rice.edu/~sandmann/cwsdpmi/index.html

This allows the programs to run in plain MS-DOS or under Windows (the DOS
extender disables itself if it sees the DPMI provided by Windows):

	exe2coff imd2atr.exe

	exe2coff atr2imd.exe

	copy /b CWSDSTUB.EXE+imd2atr imd2atr.exe

	copy /b CWSDSTUB.EXE+atr2imd atr2imd.exe
