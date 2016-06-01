# Atari Disk Tools

Use atr to manipulate .atr disk image files.

Use atr2imd and imd2atr to convert between .atr disk images and .img disk
images.  These are useful if you are trying to read Atari disks on an IBM PC
using ImageDisk.

Use detok to convert .m65 tokenized assembly source files to ASCII.

## atr

Manipulate .atr disk image files.  Allows you to read, write or
delete files in .atr disk images.

ATR also provides a file system checker and will not crash when manipulating
damaged images.  The filesystem checker verifies the following things:

* That the file size field in the directory matches the number of sectors used by the file
* That no files are marked as open
* That each file's sector linked list is not used by more than one file or is infinite
* That directory entry number matches file number in sector linked list
* That there are no directory entries used after the end of directory mark (which is the first directory entry marked as never used)
* That VTOC version field is 2
* That total sectors and free sectors fields in VTOC are correct
* Reconstruct the allocation bitmap from files and verify that it matches VTOC bitmap

ATR is for Cygwin or Linux (add 'b' flag to fopen()s for Windows).

### Image formats

ATR handles DOS 2.0S single density images.  These images should normally be
92,176 bytes (16 byte .atr header + 40 tracks * 18 sectors per track * 128
bytes per sector), but ATR assumes that any image below 131,088 is single
density.  131,088 is the smallest viable enhanced density image.

ATR also handles DOS 2.5 enhanced density images.  These images should
normally be 133,136 bytes (16 byte .atr header + 40 tracks * 26 sectors per
track * 128 bytes per sector), but ATR assumes that any image below 183,952
is enhanced density.

### Compile

	cc -o atr atr.c

### Syntax

	atr path-to-diskette command [options] args

### Commands

      ls [-la1]                     Directory listing
                  -l for long
                  -a to show system files
                  -1 to show a single name per line

      cat [-l] atari-name           Type file to console
                  -l to convert line ending from 0x9b to 0x0a

      get [-l] atari-name [local-name]
                                    Copy file from diskette to local-name
                  -l to convert line ending from 0x9b to 0x0a

      put [-l] local-name [atari-name]
                                    Copy file from local-name to diskette
                  -l to convert line ending from 0x0a to 0x9b

      free                          Print amount of free space

      rm atari-name                 Delete a file

      check                         Check filesystem


Example of 'ls', result is sorted as in UNIX:

	./atr "Osaplus Pro 2.12.atr" ls -a

	basic.com    config.src   do.com       dupdbl.com   help.com                  
	ciobas.usr   copy.com     dos.sys      dupsng.com   initdbl.c

Example of 'ls -al', shows full details:

	./atr dos2_0s.atr ls -al

	-rw-s    694 (  6) autorun.sys   (load=2800-29db load=2a4d-2a92 
	                                 load=110-18b load=2e0-2e1 run=2800)
	-rw--  31616 (253) choplift.exe  (load=4500-bfff load=2e0-2e1 run=5f00)
	-rw-s   4875 ( 39) dos.sys      
	-rw--  19852 (159) frogger.exe   (load=2480-71ff load=2e0-2e1 run=7180)
	-rw--  16739 (134) jumpjr.exe    (load=1f00-6056 load=2e0-2e1 run=1f3f)

	5 entries

	591 sectors, 73776 bytes

	116 free sectors, 14848 free bytes

Example of 'check':

	./atr "Osaplus Pro 2.12.atr" ls -a

	Checking dos.sys (file_no 0)
	  Found 44 sectors
	Checking copy.com (file_no 1)
	  ** Warning: size in directory (74) does not match size on disk (75) for file copy.com
	  Found 75 sectors
	Checking do.com (file_no 2)
	  Found 3 sectors
	Checking drive.com (file_no 3)
	  ** Warning: size in directory (35) does not match size on disk (36) for file drive.com
	  Found 36 sectors
	Checking dupdbl.com (file_no 4)
	  Found 11 sectors
	Checking dupsng.com (file_no 5)
	  Found 10 sectors
	Checking format.com (file_no 6)
	  Found 6 sectors
	Checking help.com (file_no 7)
	Checking initdbl.com (file_no 8)
	  ** Warning: size in directory (22) does not match size on disk (23) for file initdbl.com
	  Found 23 sectors
	Checking rs232.com (file_no 9)
	  Found 1 sectors
	Checking config.com (file_no 10)
	  Found 1 sectors
	Checking config.src (file_no 11)
	  Found 5 sectors
	Checking basic.com (file_no 12)
	  ** Warning: size in directory (150) does not match size on disk (154) for file basic.com
	  Found 154 sectors
	Checking ciobas.usr (file_no 13)
	  Found 2 sectors
	Checking diskcat.msb (file_no 14)
	  ** Warning: size in directory (16) does not match size on disk (17) for file diskcat.msb
	  Found 17 sectors
	431 sectors in use, 289 sectors free
	Checking VTOC header...
	  Checking that VTOC unused count matches bitmap...
	    It's OK (count is 289)
	  Checking that VTOC usable sector count is 707...
	    It's OK
	  Checking that VTOC type code is 2...
	    It's OK
	Compare VTOC bitmap with reconstructed bitmap from files...
	  It's OK.
	All done.

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
