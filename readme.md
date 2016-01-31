# Atari Disk Tools

Use atr to manipulate .atr disk image files.

Use atr2imd and imd2atr to convert between .atr disk images and .img disk
images.  These are useful if you are trying to read Atari disks on an IBM PC
using ImageDisk.

## atr

Manipulate .atr disk image files.  Allows you to read, write or
delete files in .atr disk images.

### Limitations

Handles DOS 2.0S single density 90K disks and DOS 2.5 enhanced density 130K
disks only.  This is for Cygwin or Linux (add 'b' flag to fopen()s for
Windows I think).

### Compile

	cc -o atr atr.c

### Syntax

	atr path-to-diskette command args

### Commands

      ls [-la1]                     Directory listing
                  -l for long
                  -a to show system files
                  -1 to show a single name per line

      cat atari-name                Type file to console

      get atari-name [local-name]   Copy file from diskette to local-name

      put local-name [atari-name]   Copy file to diskette to atari-name

      free                          Print amount of free space

      rm atari-name                 Delete a file

      check                         Check filesystem


For example:

	./atr dos2_0s.atr ls -al

	-rw-s    694 (  6) autorun.sys   (load_start=$2800 load_end=$29db)
	-rw--  31616 (253) choplift.exe  (load_start=$4500 load_end=$bfff)
	-rw-s   4875 ( 39) dos.sys      
	-rw--  19852 (159) frogger.exe   (load_start=$2480 load_end=$71ff)
	-rw--  16739 (134) jumpjr.exe    (load_start=$1f00 load_end=$6056)

	5 entries

	591 sectors, 73776 bytes

	116 free sectors, 14848 free bytes


## atr2imd

Convert Nick Kennedy's .ATR (Atari) disk image file format to
Dave Dunfield's .IMD (ImageDisk) file format


## imd2atr

Convert Nick Kennedy's .ATR (Atari) disk image file format to
Dave Dunfield's .IMD (ImageDisk) file format


You could use these to read and write Atari 800 disks using an IBM PC floppy
drive with ImageDisk.  Note however that the floppy drive should be adjusted
for 288 RPM instead of 300 RPM.


### Compiling instructions

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
