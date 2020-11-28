# Atari Disk Tools


* [ATR](#atr)<br>
  * [Image Formats](#image-formats)<br>
  * [Compiling instructions](#atr-compiling-instructions)<br>
  * [Syntax](#atr-syntax)<br>
  * [Commands](#commands)<br>
  * [ATR header format](#atr-header-format)<br>
  * [Filesystem technical descriptions](#filesystem-format)<br>
* [ATR2IMD](#atr2imd)<br>
* [IMD2ATR](#imd2atr)<br>
  * [Compiling instructions](#imd2atr-compiling-instructions)<br>
* [detok](#detok)<br>
  * [Compiling instructions](#detok-compiling-instructions)<br>
  * [Syntax](#detok-syntax)<br>

Use ATR to manipulate .atr disk image files.

Use ATR2IMD and IMD2ATR to convert between .atr disk images and .img disk
images.  These are useful if you are trying to read Atari disks on an IBM PC
using ImageDisk.

Use detok to convert .m65 tokenized assembly source files to ASCII.

# ATR

Manipulate .atr disk image files.  Allows you to read, write or
delete files in .atr disk images.

ATR also provides a file system checker and will not crash when manipulating
damaged images.  The filesystem checker verifies and fixes the following
things:

* That the file size field in the directory entry matches the number of sectors used by the file (can fix)
* That no files are marked as open (can fix)
* That each file's sector linked list is not used by more than one file or is infinite
* That directory entry number matches file number in sector linked list (can fix)
* That there are no directory entries used after the end of directory mark (which is the first directory entry marked as never used)
* That VTOC version field is 2 (can fix)
* That total sectors and free sectors fields in VTOC are correct (can fix)
* Reconstruct the allocation bitmap from files and verify that it matches VTOC bitmap (can fix)

ATR is for Cygwin or Linux (add 'b' flag to fopen()s for Windows).

## Image formats

ATR handles DOS 2.0s single density images.  These images should normally be
92,176 bytes (16 byte .atr header + 40 tracks * 18 sectors per track * 128
bytes per sector), but ATR assumes that any image below 131,088 is single
density.  131,088 is the smallest viable enhanced density image.

ATR also handles DOS 2.5 enhanced density images.  These images should
normally be 133,136 bytes (16 byte .atr header + 40 tracks * 26 sectors per
track * 128 bytes per sector), but ATR assumes that any image below 183,952
is enhanced density.

ATR also handles DOS 2.0d double density images.  These images should
normally be 183,952 bytes (16 byte .atr header + 40 track * 18 sectors per
track * 256 bytes per sector - 384 bytes because first three sectors are
short).

## ATR Compiling instructions

	make

## ATR Syntax

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

      x [-a]                        Extract all files
                  -a to include system files

      put [-l] local-name [atari-name]
                                    Copy file from local-name to diskette
                  -l to convert line ending from 0x0a to 0x9b

      w names...                    Write all named files to diskette

      free                          Print amount of free space

      mv old-name new-name          Rename a file

      rm atari-name                 Delete a file

      check                         Check filesystem

      fix                           Check and fix filesystem (prompts
                                    for each fix).

      mkfs dos2.0s|dos2.5|dos2.0d   Create new empty filesystem (deletes image)


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

	./atr "Osaplus Pro 2.12.atr" check

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

## ATR header format

Copied from "Structure of an SIO2PC Atari disk image" in:

[readme.txt](http://pages.suddenlink.net/wa5bdu/readme.txt)

WORD = special code* indicating this is an Atari disk file

* The "code" is the 16 bit sum of the individual ASCII values of the 
string of bytes: "NICKATARI". If you try to load a file without this first 
WORD, you get a "THIS FILE IS NOT AN ATARI DISK FILE" error 
message.

WORD = size of this disk image, in paragraphs (size/16)

WORD = sector size. (128 or 256) bytes/sector

WORD = high part of size, in paragraphs (added by REV 3.00)

BYTE = disk flags such as copy protection and write protect; see copy 
protection chapter.

WORD = 1st (or typical) bad sector; see copy protection chapter.
SPARES 5 unused (spare) header bytes (contain zeroes)

After the header comes the disk image. This is just a continuous string of 
bytes, with the first 128 bytes being the contents of disk sector 1, the 
second being sector 2, etc.

Note however that for 256 bytes per sector disks, the format is ambiguous.  
The issue is that the first three sectors use only 128 bytes, even though
there are 256 bytes on the disk.  This has led to three different formats:

* Logical - Only 128 bytes are supplied in each of the first three sectors
* Physical - The first three sectors contains all 256 bytes as on the disk
* Weird - There are three 128 byte sectors, then three 128 byte sectors of zeros

To determine which of these you have you need to follow this procedure:

1. Check the file size (ignoring the 16-byte header).  If it's evenly
divisible for 128, but not 256 then you have the Logical format.

2. If it's evenly divisible by 256, then you have either the Physical or Weird
formats.  To distinguish between them, check byte 384-767.  If they are all
zeros, you probably have the Weird format, otherwise you have the Physical
format.

## Filesystem format

### DOS 2.0s single density

40 tracks, 18 sectors, 128 byte sectors: 92160 bytes

Drive numbers sectors 1..720 but allocation map numbers sectors
0..719.  Since there is no sector 0, it's always marked in-use in the
allocation map.  Sector 720 can not be allocated since there is no
allocation map bit for it.

Boot sectors = 1..3.  These are allocated and written on newly
formatted disks even if there is no dos.sys.

VTOC sector = 360 (0x168)

Directory sectors = 361..368 (0x169..0x170)

Out of reach sector = 720 (no bitmap bit for it)

VTOC:
* 0: DOS version code:  2 for Atari DOS 2.0
* 1..2: Initial number of free sectors in allocation map. Excludes pre-allocated sectors including sector 0, boot, VTOC, and directory.  Should be 707.
* 3..4: Current number of free sectors 0..707
* 5..9: unused
* 10..99: allocation bitmap for sectors 0..719.  0 means in use.
* 100-127: unused

Directory entry: (8 entries per 128 byte sector):

* 0: flag byte (0 means unused, 0x42 means in use)
  * bit 0: opened for output
  * bit 1: created by DOS 2
  * bit 5: file locked
  * bit 6: this entry in use
  * bit 7: this entry has been deleted
* 1..2: number of sectors in file
* 3..4: starting sector number
* 5..12: 8 byte file name
* 13..15: 3 byte extension

When DOS 2.0s searches for a file, it stops searching when it encounters the
first directory entry which has never been used (flag byte bits 6 and 7 both
0).

Data sectors:
* 0..124:   Contain data
* 125: File number in upper 6 bits.  Upper 2 bits of next sector number in lower two bits.
* 126: Lower 8 bits of next sector number.
* 127: Number of data bytes in sector: Usually 125 except for last sector

File data is stored as a linked-list of sectors.  Each sector has the next sector
number embedded in it.  Each sector has the file number, which is just the
index to the directory entry which owns the file.

Unlike some file systems, for example CP/M, the exact file size is known
since each sector has a byte indicating the number of used bytes in it.

According to [Inside Atari DOS](http://www.atariarchives.org/iad/chapter2.php), any sector can
be short, not just the last one.

### DOS 2.5 Enhanced density

40 tracks, 26 sectors, 128 byte sectors: 133120 bytes

Drive numbers sectors 1..1040 but allocation map numbers sectors
0..1023.  Since there is no sector 0, it's always marked in-use in the
allocation map.  Out of reach sector 1024 is used for VTOC2.  Sectors
1025..1040 not used because next sector number is only 10 bits.

Note: on new disks, DOS 2.5 allocates sector 720 even though it is not used
for anything.  I think this is to enhance backward compatibility with DOS 2.0s
(where some programs might use sector 720, knowing that the OS will not normally use
use it).

Boot sectors = 1..3.  These are allocated and written on newly formatted disks even if there is no dos.sys written.

VTOC sector = 360 (0x168)

Directory sectors = 361..368

VTOC2 = 1024 (has more bitmap bits)

Out of reach sectors = 1024..1040 (because next sector number is 10 bits).

VTOC: Same as 2.0s, except:

Initial number of free sectors in allocation map.  Excludes pre-allocated
sector 0, boot, VTOC, directory and sector 720.  Should be 1010 but 1011
is probably OK as well (for a format without pre-allocating 720).

Current number of free sectors below 720.  Should be 707 on a new disk since
sector 0, boot sectors, VTOC and directory sectors are pre-allocated.

VTOC2:
* 0..83: Repeat VTOC bitmap for sectors 48..719 (write these, do not read them)
* 84..121: Bitmap for sectors 720..1023
* 122..123: Current number of free sectors above sector 719.  Should be 303 on a new disk because sector 720 is pre-allocated.
* 124..127: Unused.

Directory: same as DOS 2.0s

Data sectors: same as DOS 2.0s

### DOS 2.0d Double density

40 tracks, 18 sectors, 256 byte sectors:

184320 - 384 = 183936 bytes (subtract 384 because first three sectors have 128 bytes).

Sector numbering: Same as DOS 2.0s

Boot sectors = 1..3 Only first 128 bytes of each used even though on the disk they
are 256 bytes.  Usually these sectors use 128 bytes in the .ATR file, but not
always.

VTOC sector = 360 (0x168)

Directory sectors = 361..368 (0x169..0x170)

Out of reach sector = 720 (out of reach because no bitmap bit for it)

VTOC: Same as DOS 2.0s, except balance of 256 byte sector is left unused.

Directory: Same as DOS 2.0s.  Note that each directory sector has 8
entries even though 16 would fit.  Bytes 128 - 255 of each directory sector
are left unused.

Data sectors:
* 0..252: Contain data
* 253: File number in upper 6 bits.  Upper 2 bits of next  sector number in lower two bits.
* 254: Lower 8 bits of next sector number.
* 255: Number of data bytes in sector: Usually 253 except for last sector

### Boot sectors

See [Inside Atari DOS - The Boot Process](http://www.atariarchives.org/iad/chapter20.php).

DOS 2.0s and DOS 2.0d use the same boot sectors, except that the BLDISP (at
offset $11 of the first boot sector) "Displacement in Sector to Sector Link"
is $7D for DOS 2.0s, but $FD for DOS 2.0d.

DOS 2.5 boot sectors have more differences.

# ATR2IMD

Convert Nick Kennedy's .ATR (Atari) disk image file format to
Dave Dunfield's .IMD (ImageDisk) file format

You could use this to write Atari 800 disks using an IBM PC floppy
drive with ImageDisk.  Note however that the floppy drive should be adjusted
for 288 RPM instead of 300 RPM.

# IMD2ATR

Convert Dave Dunfield's .IMD (ImageDisk) disk image file format to Nick
Kennedy's .ATR (Atari) disk image file format.

You could use this to read Atari 800 disks using an IBM PC floppy
drive with ImageDisk.

## IMD2ATR Compiling instructions

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

# DETOK

This utility converts Mac65 tokenized assembly language source file into
ASCII and prints the result on the standard output.


## DETOK Compiling instructions

	cc -o detok detok.c

## DETOK Syntax

	detok source.m65

