/*	Atari diskette access
 *	Copyright
 *		(C) 2011 Joseph H. Allen
 *
 * This is free software; you can redistribute it and/or modify it under the 
 * terms of the GNU General Public License as published by the Free Software 
 * Foundation; either version 1, or (at your option) any later version.  
 *
 * It is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more 
 * details.  
 * 
 * You should have received a copy of the GNU General Public License along with 
 * this software; see the file COPYING.  If not, write to the Free Software Foundation, 
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Atari disk access */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Disks: .ATR file has a 16 byte header, then data:
 *
 *  DOS 2.0S single density (40 tracks, 18 sectors, 128 byte sectors): 92160 bytes
 *
 *     Drive numbers sectors 1..720 but allocation map numbers sectors
 *     0..719.  Since there is no sector 0, it's always marked in-use in the
 *     allocation map.  Sector 720 can not be allocated since there is no
 *     allocation map bit for it.
 *
 *     Boot sectors = 1..3.  These are allocated and written on newly
 *     formatted disks even if there is no dos.sys.
 *
 *     VTOC sector = 360 (0x168)
 *
 *     Directory sectors = 361..368 (0x169..0x170)
 *
 *     Out of reach sector = 720 (no bitmap bit for it)
 *
 *     VTOC:
 *         0: DOS version code:  2 for Atari DOS 2.0
 *      1..2: Initial number of free sectors in allocation map. Excludes pre-allocated
 *            sectors including sector 0, boot, VTOC, and directory.  Should be 707.
 *
 *      3..4: Current number of free sectors 0..707
 *      5..9: unused
 *    10..99: allocation bitmap for sectors 0..719.  0 means in use.
 *   100-127: unused
 *
 *    Directory entry: (8 entries per 128 byte sector)
 *         0: flag byte (0 means unused, 0x42 means in use)
 *             bit 0: opened for output
 *             bit 1: created by DOS 2
 *             bit 5: file locked
 *             bit 6: this entry in use
 *             bit 7: this entry has been deleted
 *      1..2: number of sectors in file
 *      3..4: starting sector number
 *     5..12: 8 byte file name
 *    13..15: 3 byte extension
 *
 *    Data sectors:
 *      0..124   Contain data
 *      125      File number in upper 6 bits.  Upper 2 bits of next 
 *               sector number in lower two bits.
 *      126      Lower 8 bits of next sector number.
 *      127      Number of data bytes in sector: Usually 125 except for last sector
 *     
 *  DOS 2.5 Enhanced density (40 tracks, 26 sectors, 128 byte sectors): 133120 bytes
 *
 *     Drive numbers sectors 1..1040 but allocation map numbers sectors
 *     0..1023.  Since there is no sector 0, it's always marked in-use in the
 *     allocation map.  Out of reach sector 1024 is used for VTOC2.  Sectors
 *     1025..1040 not used because next sector number is only 10 bits.
 *
 *     Note: on new disks, DOS 2.5 allocates sector 720 even though it is not used
 *     for anything.  I think this is to enhance backward compatibility with DOS 2.0s
 *     (where some programs might use sector 720, knowing that the OS will not normally use
 *     use it).
 *
 *     Boot sectors = 1..3.  These are allocated and written on newly formatted disks
 *     even if there is no dos.sys written.
 *
 *     VTOC sector = 360 (0x168)
 *
 *     Directory sectors = 361..368
 *
 *     VTOC2 = 1024 (has more bitmap bits)
 *
 *     Out of reach sectors = 1024..1040 (because next sector number is 10 bits).
 *
 *     VTOC: Same as 2.0s, except:
 *
 *       Initial number of free sectors in allocation map.  Excludes pre-allocated
 *       sector 0, boot, VTOC, directory and sector 720.  Should be 1010 but 1011
 *       is probably OK as well (for a format without pre-allocating 720).
 *
 *       Current number of free sectors below 720.  Should be 707 on a new disk since
 *       sector 0, boot sectors, VTOC and directory sectors are pre-allocated.
 *
 *     VTOC2:
 *         0..83: Repeat VTOC bitmap for sectors 48..719 (write these, do not read them)
 *       84..121: Bitmap for sectors 720..1023
 *      122..123: Current number of free sectors above sector 719.  Should be 303 on a new
 *                disk because sector 720 is pre-allocated.
 *      124..127: Unused.
 *
 *     Directory: same as DOS 2.0s
 *
 *     Data sectors: same as DOS 2.0s
 *
 *  DOS 2.0d Double density (40 tracks, 18 sectors, 256 byte sectors):
 *  184320 - 384 = 183936 bytes (subtract 384 because first three sectors have 128 bytes).
 *
 *     Sector numbering: Same as DOS 2.0s
 *
 *     Boot sectors = 1..3 Only first 128 bytes of each used even though on the disk they
 *     are 256 bytes.  Usually these sectors use 128 bytes in the .ATR file, but not
 *     always.
 *
 *     VTOC sector = 360 (0x168)
 *
 *     Directory sectors = 361..368 (0x169..0x170)
 *
 *     Out of reach sector = 720 (out of reach because no bitmap bit for it)
 *
 *     VTOC: Same as DOS 2.0s, except balance of 256 byte sector is left unused.
 *
 *     Directory: Same as DOS 2.0s.  Note that each directory sector has 8
 *     entries even though 16 would fit.  Bytes 128 - 255 of each directory sector
 *     are left unused.
 *
 *     Data sectors:
 *      0..252   Contain data
 *      253      File number in upper 6 bits.  Upper 2 bits of next 
 *               sector number in lower two bits.
 *      254      Lower 8 bits of next sector number.
 *      255      Number of data bytes in sector: Usually 253 except for last sector
 */

/* Error status */
int status = 0;

/* Set if fixes were made */
int fixes = 0;

/* Sector size in bytes */
#define SECTOR_SIZE 128
#define DD_SECTOR_SIZE 256

/* True if disk is double-density */
int disk_dd = 0;
int sector_size = SECTOR_SIZE;

/* Largest reachable sector + 1 */
int disk_size = 720;
#define SD_DISK_SIZE 720
#define ED_DISK_SIZE 1024
#define DD_DISK_SIZE 720

/* Specific sectors */
#define SECTOR_VTOC 0x168 /* VTOC / free space bitmap */
#define SECTOR_VTOC2 0x400 /* VTOC2 */
#define SECTOR_DIR 0x169 /* First directory sector */

/* Number of directory sectors */
#define SECTOR_DIR_SIZE 8

/* Directory entry */
#define FLAG_NEVER_USED 0x00
#define FLAG_DELETED 0x80
#define FLAG_IN_USE 0x40
#define FLAG_LOCKED 0x20
#define FLAG_DOS2 0x02
#define FLAG_OPENED 0x01

/* Directory entry */
struct dirent {
        unsigned char flag;
        unsigned char count_lo;
        unsigned char count_hi;
        unsigned char start_lo;
        unsigned char start_hi;
        unsigned char name[8];
        unsigned char suffix[3];
};

/* Size of a directory entry */
#define ENTRY_SIZE 16

/* Bytes within data sectors */

/* First 125 bytes are used for data */
#define DATA_SIZE 125
#define DD_DATA_SIZE 253
int data_size = DATA_SIZE;

/* Byte 125 has file number in upper 6 bits */
#define DATA_FILE_NUM 125 /* Upper 6 bits (0..63) */
#define DD_DATA_FILE_NUM 253
int data_file_num = DATA_FILE_NUM;

/* Byte 125 and 126 have next sector number: valid values (1..719) or (1..1023) */
#define DATA_NEXT_HIGH 125 /* Lower 2 bits */
#define DD_DATA_NEXT_HIGH 253
int data_next_high = DATA_NEXT_HIGH;

#define DATA_NEXT_LOW 126 /* All 8 bits */
#define DD_DATA_NEXT_LOW 254 /* All 8 bits */
int data_next_low = DATA_NEXT_LOW;

/* Byte 127 has number of bytes used */
#define DATA_BYTES 127
#define DD_DATA_BYTES 255
int data_bytes = DATA_BYTES;

void set_density(int dd)
{
        if (dd) {
                sector_size = DD_SECTOR_SIZE;
                data_size = DD_DATA_SIZE;
                data_file_num = DD_DATA_FILE_NUM;
                data_next_high = DD_DATA_NEXT_HIGH;
                data_next_low = DD_DATA_NEXT_LOW;
                data_bytes = DD_DATA_BYTES;
                disk_dd = 1;
        } else {
                sector_size = SECTOR_SIZE;
                data_size = DATA_SIZE;
                data_file_num = DATA_FILE_NUM;
                data_next_high = DATA_NEXT_HIGH;
                data_next_low = DATA_NEXT_LOW;
                data_bytes = DATA_BYTES;
                disk_dd = 0;
        }
}

/* Bytes within VTOC */

/* Bytes 0 - 9 is a header */
#define VTOC_TYPE 0
#define VTOC_NUM_SECTS 1
#define VTOC_NUM_UNUSED 3
#define VTOC_RESERVED 5
#define VTOC_UNUSED 6

/* Location of bitmap in VTOC */
#define VTOC_BITMAP 10
/* Bytes 10 - 99 used
     If the bit is a 1, the sector is free.
     If the bit is a 0, the sector is in use.
     Left most bit of byte 10 is sector 0 (does not exist)
     Right most bit of byte 99 is sector 719
     Sector 720 can not be used by DOS.
     First real sector is sector 1.
*/

/* Bytes 100 - 127 unused */

/* VTOC2 format */

/* Size of bitmap */
#define SD_BITMAP_SIZE 90
#define ED_BITMAP_SIZE 128

/* Offset to first bitmap byte copied to VTOC2 */
#define ED_BITMAP_START 6
/* ED disks have 128 bytes for their bitmap:
 *  VTOC has bytes 0 - 99 and is located at offset 10 within the VTOC sector
 *  VTOC2 has bytes 6 - 127 and is located at offset 0 within the VTOC2 sector
 */

#define VTOC2_NUM_UNUSED 122

FILE *disk;

int getsect(unsigned char *buf, int sect)
{
        int offset;
        size_t size;

        if (!sect) {
                fprintf(stderr,"Oops, tried to read sector 0\n");
                return -1;
        }
        sect -= 1;

        if (disk_dd) {
                if (sect < 3) {
                        size = SECTOR_SIZE;
                        offset = SECTOR_SIZE * sect;
                } else {
                        size = DD_SECTOR_SIZE;
                        offset = SECTOR_SIZE * 3 + DD_SECTOR_SIZE * (sect - 3);
                }
        } else {
                size = SECTOR_SIZE;
                offset = SECTOR_SIZE * sect;
        }

        if (fseek(disk, offset + 16, SEEK_SET)) {
                fprintf(stderr,"Oops, tried to seek past end (sector %d)\n", sect + 1);
                status = 1;
                return -1;
        }
        if (size != fread((char *)buf, 1, size, disk)) {
                fprintf(stderr,"Oops, read error (sector %d)\n", sect + 1);
                status = 1;
                return -1;
        }
        return 0;
}

void putsect(unsigned char *buf, int sect)
{
        int offset;
        size_t size;

        if (!sect) {
                fprintf(stderr,"Oops, requested sector 0\n");
                exit(-1);
        }
        sect -= 1;
        if (disk_dd) {
                if (sect < 3) {
                        size = SECTOR_SIZE;
                        offset = SECTOR_SIZE * sect;
                } else {
                        size = DD_SECTOR_SIZE;
                        offset = SECTOR_SIZE * 3 + DD_SECTOR_SIZE * (sect - 3);
                }
        } else {
                size = SECTOR_SIZE;
                offset = SECTOR_SIZE * sect;
        }

        if (fseek(disk, offset + 16, SEEK_SET)) {
                fprintf(stderr,"Oops, seek error during write (sector %d)\n", sect + 1);
                exit(-1);
        }
        if (size != fwrite((char *)buf, 1, size, disk)) {
                fprintf(stderr,"Oops, write error (sector %d)\n", sect + 1);
                exit(-1);
        }
}

/* Count number of free sectors in a bitmap */

int count_free(unsigned char *bitmap, int len)
{
        int count = 0;
        int x;
        while (len--) {
                for (x = 1; x != 256; x *= 2) {
                        if (*bitmap & x)
                                ++count;
                }
                ++bitmap;
        }
        return count;
}

/* Fix it? */

int fix;

int fixit()
{
        if (!fix)
                return 0;
        for (;;) {
                char buf[80];
                printf("Fix it (y,n)? ");
                fflush(stdout);
                if (fgets(buf,sizeof(buf),stdin)) {
                        if (buf[0] == 'y' || buf[0] == 'Y')
                                return 1;
                        else if (buf[0] == 'n' || buf[0] == 'N')
                                return 0;
                }
        }
}

/* Get allocation bitmap */

void getmap(unsigned char *bitmap, int check)
{
        unsigned char vtoc[DD_SECTOR_SIZE];
        unsigned char vtoc2[DD_SECTOR_SIZE];
        int upd = 0;

        if (getsect(vtoc, SECTOR_VTOC)) {
                fprintf(stderr," (trying to read VTOC)\n");
                exit(-1);
        }
        memcpy(bitmap, vtoc + VTOC_BITMAP, SD_BITMAP_SIZE);

        if (check) {
                int count = count_free(bitmap, SD_BITMAP_SIZE);
                int vtoc_count = vtoc[VTOC_NUM_UNUSED] + (256 * vtoc[VTOC_NUM_UNUSED + 1]);
                int vtoc_total = vtoc[VTOC_NUM_SECTS] + (256 * vtoc[VTOC_NUM_SECTS + 1]);
                int expected_size;
                printf("  Checking that VTOC current free sector count matches bitmap...\n");
                if (count != vtoc_count) {
                        fprintf(stderr,"    ** It doesn't match: bitmap has %d free, but VTOC count is %d\n", count, vtoc_count);
                        status = 1;
                        if (fixit()) {
                                vtoc[VTOC_NUM_UNUSED] = (0xFF & count);
                                vtoc[VTOC_NUM_UNUSED + 1] = (0xFF & (count >> 8));
                                upd = 1;
                        }
                } else {
                        printf("    It's OK (count is %d)\n", count);
                }
                if (disk_size == ED_DISK_SIZE)
                        expected_size = 1010; /* 1011 if we don't pre-allocate 720 */
                else
                        expected_size = 707;
                printf("  Checking that VTOC initial free sector count is %d...\n", expected_size);
                if (vtoc_total != expected_size) {
                        fprintf(stderr,"    ** It's wrong, we found: %d\n", vtoc_total);
                        status = 1;
                        if (fixit()) {
                                vtoc[VTOC_NUM_SECTS] = (0xFF & expected_size);
                                vtoc[VTOC_NUM_SECTS + 1] = (0xFF & (expected_size >> 8));
                                upd = 1;
                        }
                } else
                        printf("    It's OK\n");
                printf("  Checking that VTOC type code is 2...\n");
                if (vtoc[VTOC_TYPE] == 2)
                        printf("    It's OK\n");
                else {
                        fprintf(stderr, "    ** It's wrong, we found: %d\n", vtoc[VTOC_TYPE]);
                        status = 1;
                        if (fixit()) {
                                vtoc[VTOC_TYPE] = 2;
                                upd = 1;
                        }
                }
                if (upd) {
                        printf("Saving VTOC1 fixes...\n");
                        putsect(vtoc, SECTOR_VTOC);
                        printf("  done.\n");
                        upd = 0;
                        fixes = 1;
                }
        }

        if (disk_size == ED_DISK_SIZE) {
                if (getsect(vtoc2, SECTOR_VTOC2)) {
                        printf(" (trying to read VTOC2)\n");
                        exit(-1);
                }
                memcpy(
                        bitmap + SD_BITMAP_SIZE,
                        vtoc2 + (SD_BITMAP_SIZE - ED_BITMAP_START),
                        ED_BITMAP_SIZE - SD_BITMAP_SIZE
                );
                if (check) {
                        int count = count_free(bitmap + SD_BITMAP_SIZE, ED_BITMAP_SIZE - SD_BITMAP_SIZE);
                        int vtoc2_count = vtoc2[VTOC2_NUM_UNUSED] + 256 * vtoc2[VTOC2_NUM_UNUSED + 1];
                        printf("  Checking that VTOC2 current free sector count matches bitmap...\n");
                        if (count != vtoc2_count) {
                                fprintf(stderr,"    ** It doesn't match: bitmap has %d free, but VTOC2 count is %d\n", count, vtoc2_count);
                                status = 1;
                                if (fixit()) {
                                        vtoc2[VTOC2_NUM_UNUSED] = (count & 0xFF);
                                        vtoc2[VTOC2_NUM_UNUSED + 1] = (0xFF & (count >> 8));
                                        upd = 1;
                                }
                        } else {
                                printf("    It's OK (count is %d)\n", count);
                        }
                        if (upd) {
                                printf("Saving VTOC2 fixes...\n");
                                putsect(vtoc2, SECTOR_VTOC2);
                                printf("  done.\n");
                                upd = 0;
                                fixes = 1;
                        }
                }
        }
}

/* Write back allocation bitmap */

void putmap(unsigned char *bitmap)
{
        unsigned char vtoc[DD_SECTOR_SIZE];
        int count;
        unsigned char vtoc2[DD_SECTOR_SIZE];

        if (getsect(vtoc, SECTOR_VTOC)) {
                fprintf(stderr," (trying to read VTOC)\n");
                exit(-1);
        }
        memcpy(vtoc + VTOC_BITMAP, bitmap, SD_BITMAP_SIZE);

        /* Update free count */
        count = count_free(bitmap, SD_BITMAP_SIZE);
        vtoc[VTOC_NUM_UNUSED] = count;
        vtoc[VTOC_NUM_UNUSED + 1] = (count >> 8);

        putsect(vtoc, SECTOR_VTOC);

        if (disk_size == ED_DISK_SIZE) {
                if (getsect(vtoc2, SECTOR_VTOC2)) {
                        fprintf(stderr," (trying to read VTOC2)\n");
                        exit(-1);
                }
                memcpy(vtoc2, bitmap + ED_BITMAP_START, ED_BITMAP_SIZE - ED_BITMAP_START);

                /* Update free count */
                count = count_free(bitmap + SD_BITMAP_SIZE, ED_BITMAP_SIZE - SD_BITMAP_SIZE);
                vtoc2[VTOC2_NUM_UNUSED] = count;
                vtoc2[VTOC2_NUM_UNUSED + 1] = (count >> 8);

                putsect(vtoc2, SECTOR_VTOC2);
        }
}

/* Segment list */
struct segment
{
        struct segment *next;
        int start; /* 2E0=RUN, 2E2=INIT */
        int size;
        int init;
        int run;
};

/* File stored internally for nice formatting */
struct name
{
        char *name;

        /* From directory entry */
        int locked; /* Set if write-protected */
        int sector; /* Starting sector of file */
        int sects; /* Sector count */

        int is_sys; /* Set if it's a .SYS file */
        int is_cm; /* Set if it's a .COM file */

        

        /* From file itself */
        struct segment *segments;
        int size;
};

/* Array of internal file names for formatting */
struct name *names[(SECTOR_DIR_SIZE * SECTOR_SIZE) / ENTRY_SIZE];
int name_n;

/* For qsort */
int comp(struct name **l, struct name **r)
{
        return strcmp((*l)->name, (*r)->name);
}

/* Fine an empty directory entry to use for a new file */

int find_empty_entry()
{
        unsigned char buf[DD_SECTOR_SIZE];
        int x;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                if (getsect(buf, x)) {
                        fprintf(stderr," (trying to read directory sector)\n");
                        exit(-1);
                }
                for (y = 0; y != SECTOR_SIZE; y += ENTRY_SIZE) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (!(d->flag & FLAG_IN_USE)) {
                                return (x - SECTOR_DIR) * SECTOR_SIZE / ENTRY_SIZE + (y / ENTRY_SIZE);
                        }
                }
        }
        return -1;
}

int lower(int c)
{
        if (c >= 'A' && c <= 'Z')
                return c - 'A' + 'a';
        else
                return c;
}

/* Convert file name from directory into UNIX zero-terminated C string name */

char *getname(struct dirent *d)
{
        static char s[50];
        int p = 0;
        int r;
        int i;
        /* Get name */
        for (i = 0; i != sizeof(d->name); i++) {
                s[p++] = lower(d->name[i]);
        }
        /* Zap trailing spaces */
        while (p && s[p - 1] == ' ') --p;
        /* Append '.' */
        s[p++] = '.';
        r = p;
        /* Get extension */
        for (i = 0; i != sizeof(d->suffix); i++) {
                s[p++] = lower(d->suffix[i]);
        }
        /* Zap tailing spaces */
        while (p && s[p - 1] == ' ') --p;
        /* Zap '.' if no extension */
        if (p == r) --p;
        /* Terminate */
        s[p] = 0;
        return s;
}

/* Write UNIX name into Atari directory entry */

void putname(struct dirent *d, char *name)
{
        int x;
        /* Copy file name into directory entry */
        x = 0;
        while (*name && *name != '.' && x < 8) {
                if (*name >= 'a' && *name <= 'z')
                        d->name[x++] = *name++ - 'a' + 'A';
                else
                        d->name[x++] = *name++;
        }
        while (x < 8) {
                d->name[x++] = ' ';
        }
        x = 0;
        while (*name && *name != '.')
                ++name;
        if (*name == '.') {
                ++name;
                while (*name && x < 3) {
                        if (*name >= 'a' && *name <= 'z')
                                d->suffix[x++] = *name++ - 'a' + 'A';
                        else
                                d->suffix[x++] = *name++;
                }
        }
        while (x < 3) {
                d->suffix[x++] = ' ';
        }
}

/* Find a file, return number of its first sector */
/* If del is set, mark directory for deletion */

int find_file(char *filename, int del, char *new_name)
{
        unsigned char buf[DD_SECTOR_SIZE];
        int x;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                if (getsect(buf, x)) {
                        fprintf(stderr," (trying to read directory)\n");
                        goto done;
                }
                for (y = 0; y != SECTOR_SIZE; y += ENTRY_SIZE) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        /* OSS OS/A+ disks put junk after first never used directory entry */
                        if (!(d->flag & (FLAG_IN_USE | FLAG_DELETED)))
                                goto done;
                        if (d->flag & FLAG_IN_USE) {
                                char *s = getname(d);
                                if (!strcmp(s, filename)) {
                                        if (del) {
                                                d->flag = 0x80;
                                                putsect(buf, x);
                                        }
                                        if (new_name) {
                                                putname(d, new_name);
                                                putsect(buf, x);
                                        }
                                        return (d->start_hi << 8) + d->start_lo;
                                }
                        }
                }
        }
        done:
        return -1;
}

/* Read a file */

int cvt_ending = 0;

void read_file(int sector, FILE *f)
{
        int count = 0;

        do {
                unsigned char buf[DD_SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;

                if (count == 2048) {
                        fprintf(stderr," (file too long)\n");
                        status = 1;
                        break;
                }
                if (getsect(buf, sector)) {
                        fprintf(stderr," (trying to read from file)\n");
                        status = 1;
                        return;
                }
                ++count;

                next = (int)buf[data_next_low] + ((int)(0x3 & buf[data_next_high]) << 8);
                file_no = ((buf[data_file_num] >> 2) & 0x3F);
                bytes = buf[data_bytes];

                /* printf("Sector %d: next=%d, bytes=%d, file_no=%d, short=%d\n",
                        sector, next, bytes, file_no, short_sect); */
                
                if (cvt_ending) {
                        int x;
                        for (x = 0; x != bytes; ++x)
                                if (buf[x] == 0x9b) {
                                        buf[x] = '\n';
                                }
                }

                fwrite(buf, bytes, 1, f);

                sector = next;
        } while(sector);
}

/* cat a file */

void cat(char *name)
{
        int sector = find_file(name, 0, NULL);
        if (sector == -1) {
                fprintf(stderr,"File '%s' not found\n", name);
                exit(-1);
        } else {
                /* printf("Found file.  First sector of file is %d\n", sector); */
                read_file(sector, stdout);
        }
}

/* get a file from the disk */

int get_file(char *atari_name, char *local_name)
{
        int sector = find_file(atari_name, 0, NULL);
        if (sector == -1) {
                fprintf(stderr,"File '%s' not found\n", atari_name);
                return -1;
        } else {
                FILE *f = fopen(local_name, "w");
                if (!f) {
                        fprintf(stderr,"Couldn't open local file '%s'\n", local_name);
                        return -1;
                }
                /* printf("Found file.  First sector of file is %d\n", sector); */
                read_file(sector, f);
                if (fclose(f)) {
                        fprintf(stderr,"Couldn't close local file '%s'\n", local_name);
                        return -1;
                }
                return status;
        }
}

/* Mark a sector as allocated or free */

void mark_space(unsigned char *bitmap, int start, int alloc)
{
        if (alloc) {
                bitmap[start >> 3] &= ~(1 << (7 - (start & 7)));
        } else {
                bitmap[start >> 3] |= (1 << (7 - (start & 7)));
        }
}

/* Delete file */

int del_file(int sector)
{
        unsigned char bitmap[ED_BITMAP_SIZE];
        int count = 0;
        getmap(bitmap, 0);

        do {
                unsigned char buf[DD_SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;

                if (count == 2048) {
                        fprintf(stderr," (file too long)\n");
                        break;
                }
                if (getsect(buf, sector)) {
                        fprintf(stderr," (while deleting a file)\n");
                        break;
                }
                ++count;

                next = (int)buf[data_next_low] + ((int)(0x3 & buf[data_next_high]) << 8);
                file_no = ((buf[data_file_num] >> 2) & 0x3F);
                bytes = buf[data_bytes];

                /* printf("Sector %d: next=%d, bytes=%d, file_no=%d, short=%d\n", sector, next, bytes, file_no, short_sect); */

                mark_space(bitmap, sector, 0);

                sector = next;
        } while(sector);

        putmap(bitmap);
        return 0;
}

/* Delete file name */

int rm(char *name, int ignore)
{
        int first_sect = find_file(name, 1, NULL);
        if (first_sect != -1) {
                if (del_file(first_sect)) {
                        fprintf(stderr,"Error deleting file '%s'\n", name);
                        return -1;
                } else {
                        return status;
                }
        } else {
                if (!ignore) {
                        fprintf(stderr,"File '%s' not found\n", name);
                        status = 1;
                }
                return -1;
        }
}

/* Count free sectors */

int amount_free(unsigned char *bitmap)
{
        int total = 0;
        int x;

        for (x = 0; x != disk_size; ++x) {
                if (bitmap[(x >> 3)] & (1 << (7 - (x & 7))))
                        ++total;
        }
        return total;
}

/* Free command */

int do_free(void)
{
        int amount;
        unsigned char bitmap[ED_BITMAP_SIZE];
        getmap(bitmap, 0);
        amount = amount_free(bitmap);
        printf("%d free sectors, %d free bytes\n", amount, amount * sector_size);
        return 0;
}

/* Check a single file */

int check_file(struct dirent *d, int y, int x, char *map, char *name[])
{
        unsigned char fbuf[DD_SECTOR_SIZE];
        char *filename = strdup(getname(d));
        int upddir = 0;
        int sector;
        int sects;
        int count = 0;
        int upd = 0;
        int file_no = (y / ENTRY_SIZE) + ((x - SECTOR_DIR) * SECTOR_SIZE / ENTRY_SIZE);
        sector = (d->start_hi << 8) + d->start_lo;
        sects = (d->count_hi << 8) + d->count_lo;
        printf("Checking %s (file_no %d)\n", filename, file_no);
        if (d->flag & FLAG_OPENED) {
                printf("  ** Warning: file is marked as opened\n");
                if (fixit()) {
                        d->flag &= ~FLAG_OPENED;
                        upddir = 1;
                }
        }
        do {
                int next;
                int sector_file_no;
                ++count;
                if (count == 2048) {
                        fprintf(stderr," (file too long)\n");
                        status = 1;
                        break;
                }
                if (getsect(fbuf, sector)) {
                        fprintf(stderr," (reading file)\n");
                        exit(-1);
                }
                if (map[sector] != -1) {
                        fprintf(stderr,"  ** Uh oh.. sector %d already in use by %s (%d)\n", sector, name[sector] ? name[sector] : "reserved", map[sector]);
                        status = 1;
                }
                if (map[sector] == file_no) {
                        fprintf(stderr,"  ** Warning: Infinite linked list detected\n");
                        status = 1;
                        break;
                } else {
                        int dsize;
                        map[sector] = file_no;
                        name[sector] = filename;
                        next = (int)fbuf[data_next_low] + ((int)(0x3 & fbuf[data_next_high]) << 8);
                        sector_file_no = ((int)fbuf[data_file_num] >> 2);
                        if (sector_file_no != file_no) {
                                fprintf(stderr,"  ** Warning: Sector %d claims to belong to file %d\n", sector, sector_file_no);
                                status = 1;
                                if (fixit()) {
                                        fbuf[data_file_num] = (fbuf[data_file_num] & 0x3) | (file_no << 2);
                                        upd = 1;
                                }
                        }
                        dsize = (int)fbuf[data_bytes];
                        if (next) {
                                if (dsize != data_size) {
                                        fprintf(stderr, "  ** Warning: Sector %d is short\n", sector);
                                }
                        } else {
                                if (dsize == 0) {
                                        fprintf(stderr, "  ** Warning: Sector %d (last sector of file) is empty\n", sector);
                                }
                        }
                }
                if (upd) {
                        putsect(fbuf, sector);
                        fixes = 1;
                        upd = 0;
                }
                sector = next;
        } while (sector);
        if (count != sects) {
                fprintf(stderr,"  ** Warning: size in directory (%d) does not match size on disk (%d) for file %s\n",
                       sects, count, filename);
                status = 1;
                if (fixit()) {
                        d->count_hi = (0xFF & (count >> 8));
                        d->count_lo = (0xFF & count);
                        upddir = 1;
                }
        }
        printf("  Found %d sectors\n", count);
        return upddir;
}

/* Check disk: regen bit map */

int do_check()
{
        unsigned char bitmap[ED_BITMAP_SIZE];
        unsigned char buf[DD_SECTOR_SIZE];
        int x;
        int total;
        int ok;
        int found_eod = 0;
        char map[ED_DISK_SIZE];
        char *name[ED_DISK_SIZE];

        if (disk_size == ED_DISK_SIZE)
                printf("Checking DOS 2.5 enhanced density disk...\n");
        else if (disk_dd)
                printf("Checking DOS 2.0d double density disk...\n");
        else
                printf("Checking DOS 2.0s single density disk...\n");

        /* Mark all as free */
        for (x = 0; x != ED_DISK_SIZE; ++x) {
                map[x] = -1;
                name[x] = 0;
        }

        /* Mark non-existent sector 0 as allocated */
        map[0] = 64;

        /* Mark VTOC and DIR */
        map[SECTOR_VTOC] = 64;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x)
                map[x] = 64;

        /* Boot loader */
        map[1] = 64;
        map[2] = 64;
        map[3] = 64;

        /* Sector 720 if we have an ED disk */
        if (disk_size == ED_DISK_SIZE)
                map[720] = 64;

        /* Step through each file */
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                int upd = 0;
                if (getsect(buf, x)) {
                        fprintf(stderr," (reading directory)\n");
                        exit(-1);
                }
                for (y = 0; y != SECTOR_SIZE; y += ENTRY_SIZE) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (!(d->flag & (FLAG_IN_USE | FLAG_DELETED))) {
                                found_eod = 1;
                        }
                        if (d->flag & FLAG_IN_USE) {
                                if (found_eod == 1) {
                                        fprintf(stderr,"** Error: found in use directory entry after end of directory mark:\n");
                                        status = 1;
                                        found_eod = 2;
                                }
                                upd |= check_file(d, y, x, map, name);
                        }
                }
                if (upd) {
                        printf("Writing back modified directory sector...\n");
                        putsect(buf, x);
                        printf("  done.\n");
                        fixes = 1;
                }
        }
        total = 0;
        for (x = 0; x != disk_size; ++x) {
                if (map[x] != -1) {
                        ++total;
//                        if (map[x] == 64)
//                                printf("%d reserved\n", x);
//                        else {
//                                printf("%d file number %d (%s)\n", x, map[x], name[x]);
//                        }
                }
        }
        printf("%d sectors in use, %d sectors free\n", total, disk_size - total);

        printf("Checking VTOC header...\n");
        getmap(bitmap, 1);
        printf("Compare VTOC bitmap with reconstructed bitmap from files...\n");
        ok = 1;
        for (x = 0; x != disk_size; ++x) {
                int is_alloc;
                if (bitmap[x >> 3] & (1 << (7 - (x & 7))))
                        is_alloc = 0;
                else
                        is_alloc = 1;
                if (is_alloc && map[x] == -1) {
                        fprintf(stderr,"  ** VTOC shows sector %d allocated, but it should be free\n", x);
                        status = 1;
                        ok = 0;
                }
                if (!is_alloc && map[x] != -1) {
                        fprintf(stderr,"  ** VTOC shows sector %d free, but it should be allocated\n", x);
                        status = 1;
                        ok = 0;
                }
        }
        if (ok) {
                printf("  It's OK.\n");
        } else if (fixit()) {
                memset(bitmap, 0xFF, ED_BITMAP_SIZE);
                for (x = 0; x != ED_DISK_SIZE; ++x) {
                        if (map[x] != -1) {
                                bitmap[x >> 3] &= ~(1 << (7 - (x & 7)));
                        }
                }
                printf("Updating allocation bitmap...\n");
                putmap(bitmap);
                printf("  done.\n");
                fixes = 1;
        }
        printf("All done.\n");
        if (status)
                fprintf(stderr, "Errors were detected\n");
        if (fixes)
                printf("Fixes were made - recommend you rerun check\n");
        return status;
}

/* Allocate space for file */

int alloc_space(unsigned char *bitmap, int *list, int sects)
{
        while (sects) {
                int x;
                for (x = 1; x != disk_size; ++x) {
                        if (bitmap[x >> 3] & (1 << (7 - (x & 7)))) {
                                *list++ = x;
                                bitmap[x >> 3] &= ~(1 << (7 - (x & 7)));
                                break;
                        }
                }
                if (x == disk_size) {
                        fprintf(stderr, "Not enough space\n");
                        status = 1;
                        return -1;
                }
                --sects;
        }
        return 0;
}

/* Write a file */

int write_file(unsigned char *bitmap, unsigned char *buf, int sects, int file_no, int size)
{
        int x;
        unsigned char bf[DD_SECTOR_SIZE];
        int list[ED_DISK_SIZE];
        memset(list, 0, sizeof(list));

        if (alloc_space(bitmap, list, sects))
                return -1;

        for (x = 0; x != sects; ++x) {
                memcpy(bf, buf + (data_size) * x, data_size);
                if (x + 1 == sects) {
                        // Last sector
                        bf[data_next_low] = 0;
                        bf[data_next_high] = 0;
                        bf[data_bytes] = size;
                } else {
                        bf[data_next_low] = list[x + 1];
                        bf[data_next_high] = (list[x + 1] >> 8);
                        bf[data_bytes] = data_size;
                }
                bf[data_file_num] |= (file_no << 2);
                size -= data_size;
                // printf("Writing sector %d %d %d %d\n", list[x], bf[125], bf[126], bf[127]);
                putsect(bf, list[x]);
        }
        return list[0];
}

/* Write directory entry */

int write_dir(int file_no, char *name, int first_sect, int sects)
{
        struct dirent d[1];
        unsigned char dir_buf[DD_SECTOR_SIZE];

        /* Copy file name into directory entry */
        putname(d, name);

        d->start_hi = (first_sect >> 8);
        d->start_lo = first_sect;
        d->count_hi = (sects >> 8);
        d->count_lo = sects;
        /* DOS complains on some file operations if FLAG_DOS2 is not there: */
        d->flag = FLAG_IN_USE | FLAG_DOS2;
        
        if (getsect(dir_buf, SECTOR_DIR + file_no / (SECTOR_SIZE / ENTRY_SIZE))) {
                fprintf(stderr, " (trying to read directory)\n");
                exit(-1);
        }
        memcpy(dir_buf + ENTRY_SIZE * (file_no % (SECTOR_SIZE / ENTRY_SIZE)), d, ENTRY_SIZE);
        putsect(dir_buf, SECTOR_DIR + file_no / (SECTOR_SIZE / ENTRY_SIZE));
        return 0;
}

/* Put a file on the disk */

int put_file(char *local_name, char *atari_name)
{
        FILE *f = fopen(local_name, "r");
        long size;
        long up;
        long x;
        unsigned char *buf;
        unsigned char bitmap[ED_BITMAP_SIZE];
        int first_sect;
        int file_no;
        int num_sects;
        if (!f) {
                fprintf(stderr, "Couldn't open '%s'\n", local_name);
                return -1;
        }
        if (fseek(f, 0, SEEK_END)) {
                fprintf(stderr,"Couldn't get file size of '%s'\n", local_name);
                status = 1;
                fclose(f);
                return -1;
        }
        size = ftell(f);
        if (size < 0)  {
                fprintf(stderr, "Couldn't get file size of '%s'\n", local_name);
                status = 1;
                fclose(f);
                return -1;
        }
        rewind(f);
        // Round up to a multiple of (DATA_SIZE)
        up = size + (data_size) - 1;
        up -= up % (data_size);
        num_sects = up / data_size;
        buf = (unsigned char *)malloc(up);
        if (size != fread(buf, 1, size, f)) {
                fprintf(stderr, "Couldn't read file '%s'\n", local_name);
                status = 1;
                fclose(f);
                free(buf);
                return -1;
        }
        fclose(f);

        if (cvt_ending) {
                /* Convert UNIX line endings to Atari */
                for (x = 0; x != size; ++x)
                        if (buf[x] == '\n')
                                buf[x] = 0x9b;
        }

        /* Fill with NULs to end of sector */
        for (x = size; x != up; ++x)
                buf[x] = 0;

        /* Delete existing file */
        rm(atari_name, 1);

        /* Get bitmap... */
        getmap(bitmap, 0);

        /* Prepare directory entry */
        file_no = find_empty_entry();
        if (file_no == -1) {
                return -1;
        }

        /* Allocate space and write file */
        first_sect = write_file(bitmap, buf, num_sects, file_no, size);

        if (first_sect == -1) {
                fprintf(stderr, "Couldn't write file\n");
                status = 1;
                return -1;
        }

        if (write_dir(file_no, atari_name, first_sect, num_sects)) {
                fprintf(stderr, "Couldn't write directory entry\n");
                status = 1;
                return -1;
        }

        /* Success! */
        putmap(bitmap);
        return status;
}

/* Rename a file */

int atari_rename(char *old_name, char *new_name)
{
        if (find_file(new_name, 0, NULL) != -1) {
                fprintf(stderr, "'%s' already exists\n", new_name);
                return -1;
        }
        return find_file(old_name, 0, new_name);
}

/* Get info about file: actual size, etc. */

void get_info(struct name *nam)
{
        unsigned char bigbuf[65536 * 2];
        unsigned char membuf[65536];
        size_t total = 0;
        int sector = nam->sector;
        struct segment *lastseg = 0;
        do {
                unsigned char buf[DD_SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;

                if (total + 125 >= sizeof(bigbuf)) {
                        fprintf(stderr, " (file %s too long)\n", nam->name);
                        status = 1;
                        break;
                }
                if (getsect(buf, sector)) {
                        fprintf(stderr, " (trying to read file %s)\n", nam->name);
                        break;
                }

                next = (int)buf[data_next_low] + ((int)(0x3 & buf[data_next_high]) << 8);
                file_no = ((buf[data_file_num] >> 2) & 0x3F);
                bytes = buf[data_bytes];

                if (bytes && total + bytes <= sizeof(bigbuf)) {
                        memcpy(bigbuf + total, buf, bytes);
                }

                total += bytes;

                sector = next;
        } while(sector);

        nam->size = total;
        nam->segments = 0;

        // Look at file...
        if (total >= 2 && bigbuf[0] == 0xFF && bigbuf[1] == 0xFF) { /* Magic number for binary file */
                size_t idx;
                int ok = 1;
                int segsize;
                for (idx = 0; ok && idx < total; idx += segsize) {
                        segsize = 0;
                        ok = 0;
                        /* Each segment can optionally start with 0xFFFF, skip it */
                        if (idx + 2 <= total && bigbuf[idx] == 0xFF && bigbuf[idx + 1] == 0xFF) {
                                idx += 2;
                                ok = 1;
                        }
                        /* Get header */
                        if (idx + 4 <= total) {
                                struct segment *segment;
                                int first = (int)bigbuf[idx + 0] + ((int)bigbuf[idx + 1] << 8);
                                int last = (int)bigbuf[idx + 2] + ((int)bigbuf[idx + 3] << 8);
                                segsize = last - first + 1;
                                idx += 4;
                                ok = 1;
                                if (segsize < 1) { /* Bad load format? */
                                        break;
                                }
                                /* Ignore short segments (DUP.SYS loader will not skip them) */
                                if (segsize > 1) {
                                        segment = (struct segment *)malloc(sizeof(struct segment));
                                        segment->start = first;
                                        segment->size = segsize;
                                        segment->next = 0;
                                        segment->init = -1;
                                        segment->run = -1;
                                        if (!nam->segments)
                                                nam->segments = segment;
                                        if (lastseg)
                                                lastseg->next = segment;
                                        lastseg = segment;
                                        membuf[0x2e0] = 0xFE;
                                        membuf[0x2e1] = 0xFE;
                                        membuf[0x2e2] = 0xFE;
                                        membuf[0x2e3] = 0xFE;
                                        memcpy(membuf + first, bigbuf + idx, segsize);
                                        if (membuf[0x2e0]!=0xFE || membuf[0x2e1]!=0xFE)
                                                segment->run = (int)membuf[0x2e0] + ((int)membuf[0x2e1] << 8);
                                        if (membuf[0x2e2]!=0xFE || membuf[0x2e3]!=0xFE)
                                                segment->init = (int)membuf[0x2e2] + ((int)membuf[0x2e3] << 8);
                                }
                        }
                }
        }
}

/* Read directory into names/name_n array
 * If info_flg is set, read file to measure it real length
 * If all_flg is set, included system files in
 */

void read_dir(int all_flg, int info_flg)
{
        unsigned char buf[DD_SECTOR_SIZE];
        int x;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                if (getsect(buf, x)) {
                        fprintf(stderr," (trying to read directory)\n");
                        break;
                }
                for (y = 0; y != SECTOR_SIZE; y += ENTRY_SIZE) {
                        struct dirent *d = (struct dirent *)(buf + y);

                        if (!(d->flag & (FLAG_IN_USE | FLAG_DELETED)))
                                goto done;

                        if (d->flag & FLAG_IN_USE) {
                                struct name *nam;
                                char *s = getname(d);
                                nam = (struct name *)malloc(sizeof(struct name));
                                nam->name = strdup(s);
                                if (d->flag & FLAG_LOCKED)
                                        nam->locked = 1;
                                else
                                        nam->locked = 0;
                                nam->sector = d->start_lo + (d->start_hi * 256);
                                nam->sects = d->count_lo + (d->count_hi * 256);
                                nam->segments = 0;
                                nam->size = -1;
                                if (info_flg)
                                        get_info(nam);

                                if (d->suffix[0] == 'S' && d->suffix[1] == 'Y' && d->suffix[2] == 'S')
                                        nam->is_sys = 1;
                                else
                                        nam->is_sys = 0;

                                if ((all_flg || !nam->is_sys))
                                        names[name_n++] = nam;
                        }
                }
        }
        done:;
}

#define FLUSHLINE do { \
        if (strlen(linebuf) + 15 >= 78) { \
                int n; \
                printf("%s\n", linebuf); \
                for (n = 0; n != ofst; ++n) linebuf[n] = ' '; \
                linebuf[n] = 0; \
        } \
} while (0)

void atari_dir(int all, int full, int single)
{
        int x, y;
        int rows;
        int cols = (80 / 13);
        read_dir(all, 1);

        qsort(names, name_n, sizeof(struct name *), (int (*)(const void *, const void *))comp);

        if (full) {
                int totals = 0;
                int total_bytes = 0;
                printf("\n");
                for (x = 0; x != name_n; ++x) {
                        char linebuf[100];
                        int ofst;
                        int extra = 0;
                        struct segment *seg;
                        sprintf(linebuf, "-r%c%c%c %6d (%3d) %-13s",
                               (names[x]->locked ? '-' : 'w'),
                               (names[x]->is_cm ? 'x' : '-'),
                               (names[x]->is_sys ? 's' : '-'),
                               names[x]->size, names[x]->sects, names[x]->name);
                        ofst = strlen(linebuf) + 1;
                        for (seg = names[x]->segments; seg; seg = seg->next) {
                                if (!extra) {
                                        strcat(linebuf, " (");
                                        extra = 1;
                                } else {
                                        strcat(linebuf, " ");
                                }
                                FLUSHLINE;
                                sprintf(linebuf + strlen(linebuf), "load=%x-%x", seg->start, seg->start + seg->size - 1);
                                if (seg->init != -1) {
                                        FLUSHLINE;
                                        sprintf(linebuf + strlen(linebuf), " init=%x", seg->init);
                                }
                                if (seg->run != -1) {
                                        FLUSHLINE;
                                        sprintf(linebuf + strlen(linebuf), " run=%x", seg->run);
                                }
                        }
                        if (extra)
                                strcat(linebuf, ")");
                        printf("%s\n", linebuf);
                        totals += names[x]->sects;
                        total_bytes += names[x]->size;
                }
                printf("\n%d entries\n", name_n);
                printf("\n%d sectors, %d bytes\n", totals, total_bytes);
                printf("\n");
                do_free();
                printf("\n");
        } else if (single) {
                int x;
                for (x = 0; x != name_n; ++x) {
                        printf("%s\n", names[x]->name);
                }
        } else {

                /* Rows of 12 names each ordered like ls */

                rows = (name_n + cols - 1) / cols;

                for (y = 0; y != rows; ++y) {
                        for (x = 0; x != cols; ++x) {
                                int n = y + x * rows;
                                /* printf("%11d  ", n); */
                                if (n < name_n)
                                        printf("%-12s ", names[n]->name);
                                else
                                        printf("             ");
                        }
                        printf("\n");
                }
        }
}

int mkfs(char *disk_name, int type, char* boot_sectors_file_path)
{
        unsigned char hdr[16];
        unsigned char bf[256];
        unsigned char bitmap[ED_BITMAP_SIZE];
        int size;
        int n;
        disk = fopen(disk_name, "w+");
        if (!disk) {
                fprintf(stderr, "Couldn't open '%s'\n", disk_name);
                return -1;
        }
        /* .ATR header */
        memset(hdr, 0, 16);
        hdr[0] = 0x96;
        hdr[1] = 0x02;
        switch (type) {
                case 1: {
                        disk_size = SD_DISK_SIZE;
                        size = 40*18*128;
                        hdr[2] = size/16;
                        hdr[3] = size/16/256;
                        hdr[4] = 0x80;
                        hdr[5] = 0x00;
                        break;
                } case 2: {
                        disk_size = ED_DISK_SIZE;
                        size = 40*26*128;
                        hdr[2] = size/16;
                        hdr[3] = size/16/256;
                        hdr[4] = 0x80;
                        hdr[5] = 0x00;
                        break;
                } case 3: {
                        disk_size = DD_DISK_SIZE;
                        set_density(1);
                        size = 40*18*256 - 3*128;
                        hdr[2] = size/16;
                        hdr[3] = size/16/256;
                        hdr[4] = 0x00;
                        hdr[5] = 0x01;
                        break;
                }
        }
        if (16 != fwrite(hdr, 1, 16, disk)) {
                fprintf(stderr, "Couldn't write to '%s'\n", disk_name);
                return -1;
        }
        memset(bf, 0, 256);
        for (n = 0; n != size; n += 128) {
                if (128 != fwrite(bf, 1, 128, disk)) {
                        fprintf(stderr, "Couldn't write to '%s'\n", disk_name);
                        return -1;
                }
        }
        /* VTOC */
        bf[0] = 2;
        if (disk_size == ED_DISK_SIZE) {
                bf[1] = (255 & 1010);
                bf[2] = 1010/256;
        } else {
                bf[1] = (255 & 707);
                bf[2] = 707/256;
        }
        putsect(bf, SECTOR_VTOC);
        memset(bitmap, 0xFF, ED_BITMAP_SIZE);
        mark_space(bitmap, 0, 1); /* Sector zero */
        mark_space(bitmap, 1, 1); /* Boot sectors */
        mark_space(bitmap, 2, 1);
        mark_space(bitmap, 3, 1);
        mark_space(bitmap, SECTOR_VTOC, 1); /* VTOC */
        for (n = 0; n != SECTOR_DIR_SIZE; ++n) /* DIR */
                mark_space(bitmap, SECTOR_DIR + n, 1);
        mark_space(bitmap, 720, 1); /* Reserved */
        putmap(bitmap);
        if (boot_sectors_file_path != NULL) {
                FILE* boot_sectors_file = fopen(boot_sectors_file_path, "rb");
                if (!boot_sectors_file) {
                        fprintf(stderr, "Couldn't open '%s'\n", boot_sectors_file_path);
                        return -1;
                }
                n=0;
                while (n++, !feof(boot_sectors_file)) {
                        size = ( n < 3 ? SECTOR_SIZE : sector_size);
                        size = fread(bf, size, 1, boot_sectors_file);
                        putsect(bf, n);
                }
                fclose(boot_sectors_file);
        }
        fclose(disk);
        return 0;
}

int should_extract(char* filename, int list_start, int argc, char* argv[])
{
    int i;
    if (!list_start) {
        return 1;   //  no list
    }
    for (i = list_start; i < argc; ++i) {
        if(!strcmp(filename, argv[i])) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
        int all = 0;
        int full = 0;
        int single = 0;
        long size;
        int x;
        char *disk_name;
        x = 1;
        if (x == argc || !strcmp(argv[x], "--help") || !strcmp(argv[x], "-h")) {
                printf("\nAtari DOS 2.0s, DOS 2.0d and DOS 2.5 diskette access\n");
                printf("\n");
                printf("Syntax: atr path-to-diskette [command] [args]\n");
                printf("\n");
                printf("  Commands: (with no command, ls is assumed)\n\n");
                printf("      ls [-la1]                    Directory listing\n");
                printf("                  -l for long\n");
                printf("                  -a to show system files\n");
                printf("                  -1 to show a single name per line\n\n");
                printf("      cat [-l] atari-name           Type file to console\n");
                printf("                  -l to convert line ending from 0x9b to 0x0a\n\n");
                printf("      get [-l] atari-name [local-name]\n");
                printf("                                    Copy file from diskette to local-name\n");
                printf("                  -l to convert line ending from 0x9b to 0x0a\n\n");
                printf("      x [-aol] [list]               Extract all files\n");
                printf("                  -a to include system files\n");
                printf("                  -o OUTDIR extract to output director\n");
                printf("                  -l to convert line endings from 0x9b to 0x0a\n");
                printf("                  list is a space separated list of files to extract\n\n");
                printf("      put local-name [atari-name]\n");
                printf("                                    Copy file from local-name to diskette\n");
                printf("                  -l to convert line ending from 0x0a to 0x9b\n\n");
                printf("      w names...                    Write all named files to diskette\n\n");
                printf("      free                          Print amount of free space\n\n");
                printf("      mv old-name new-name          Rename a file\n\n");
                printf("      rm atari-name                 Delete a file\n\n");
                printf("      check                         Check filesystem (read only)\n\n");
                printf("      fix                           Check and fix filesystem (prompts\n");
                printf("                                    for each fix).\n\n");
                printf("      mkfs dos2.0s|dos2.0d|dos2.5 [file with boot sectors]\n");
                printf("                                    Write a new filesystem\n");
                return -1;
        }
        disk_name = argv[x++];

        if (argv[x] && !strcmp(argv[x], "mkfs")) {
                /* Create a filesystem */
                int type = 0;
                char* boot_sectors_file_path = NULL;
                ++x;
                if (argv[x] && !strcmp(argv[x], "dos2.0s"))
                        type = 1;
                else if (argv[x] && !strcmp(argv[x], "dos2.5"))
                        type = 2;
                else if (argv[x] && !strcmp(argv[x], "dos2.0d"))
                        type = 3;
                else {
                        fprintf(stderr, "Unknown format\n");
                        return -1;
                }
                if (argc > x) {
                        // file containing bootsectors specified
                        boot_sectors_file_path = argv[x+1];
                }
                return mkfs(disk_name, type, boot_sectors_file_path);
        }

        /* Open disk image */
        disk = fopen(disk_name, "r+");
        if (!disk) {
                fprintf(stderr, "Couldn't open '%s'\n", disk_name);
                return -1;
        }

        /* Determine image type */
        if (fseek(disk, 0, SEEK_END)) {
                fprintf(stderr, "Couldn't seek disk?\n");
                return -1;
        }
        size = ftell(disk);
//	if (size - 16 == 40 * 18 * 128) {
        if (size - 16 < 1024 * 128) {
                /* Minimum size for enhanced density is 1024 sectors */
                /* Anything less: assume single-density */
                /* printf("Single density DOS 2.0S disk assumed\n"); */
                disk_size = SD_DISK_SIZE;
//	} else if (size - 16 == 40 * 26 * 128) {
        } else if (size - 16 < 128*3 + 256*717) {
                /* Minimum size of double density is 3 128 byte sectors + 717 256 byte sectors */
                /* Anything less: assume enhanced density */
                /* printf("Enhanced density DOS 2.5 disk assumed\n"); */
                disk_size = ED_DISK_SIZE;
        } else if (size - 16 == 128*3 + 256*717) {
                disk_size = SD_DISK_SIZE;
                set_density(1);
                /* printf("Double density DOS 2.0D disk assumed\n"); */
        } else {
                printf("Unknown disk size.  Expected:\n");
                printf("  .ATR header is 16 bytes, so:\n");
                printf("  16 + 40*18*128 = 92,176 bytes for DOS 2.0s single density\n");
                printf("  16 + 40*26*128 = 133,136 bytes for DOS 2.5 enhanced density\n");
                printf("  16 + 40*18*256 - 3*128 = 183,952 bytes for DOS 2.0d double density\n");
                return -1;
        }

        /* Directory options */
        dir:
        while (x != argc && argv[x][0] == '-') {
                int y;
                for (y = 1;argv[x][y];++y) {
                        int opt = argv[x][y];
                        switch (opt) {
                                case 'l': full = 1; break;
                                case 'a': all = 1; break;
                                case '1': single = 1; break;
                                default: printf("Unknown option '%c'\n", opt); return -1;
                        }
                }
                ++x;
        }

        if (x == argc) {
                /* Just print a directory listing */
                atari_dir(all, full, single);
                return status;
        } else if (!strcmp(argv[x], "ls")) {
                ++x;
                goto dir;
        } else if (!strcmp(argv[x], "free")) {
                return do_free();
        } else if (!strcmp(argv[x], "check")) {
                return do_check();
        } else if (!strcmp(argv[x], "fix")) {
                fix = 1;
                return do_check();
        } else if (!strcmp(argv[x], "cat")) {
                ++x;
                if (x != argc && !strcmp(argv[x], "-l")) {
                        cvt_ending = 1;
                        ++x;
                }
                if (x == argc) {
                        fprintf(stderr,"Missing file name to cat\n");
                        return -1;
                } else {
                        cat(argv[x++]);
                        return status;
                }
        } else if (!strcmp(argv[x], "get")) {
                char *local_name;
                char *atari_name;
                ++x;
                if (x != argc && !strcmp(argv[x], "-l")) {
                        cvt_ending = 1;
                        ++x;
                }
                if (x == argc) {
                        printf("Missing file name to get\n");
                        return -1;
                }
                atari_name = argv[x];
                local_name = atari_name;
                if (x + 1 != argc)
                        local_name = argv[++x];
                return get_file(atari_name, local_name);
        } else if (!strcmp(argv[x], "x")) {
                int all_flg = 0;
                int status = 0;
                int list_start = 0;
                char* out_dir = NULL;
                int n;
                char* out_filename;
                while (++x != argc) {
                    if ('-' == argv[x][0]) {
                        if (!strcmp(argv[x], "-a")) {
                            all_flg = 1;
                        } else if(!strcmp(argv[x], "-l")) {
                            cvt_ending = 1;
                        } else if(!strcmp(argv[x], "-o")) {
                            ++x;
                            if (x != argc) {
                                out_dir = argv[x];
                            }
                        }
                    } else {
                        list_start = x;
                        break;
                    }
                }
                read_dir(all_flg, 0);
                for (n = 0; n != name_n; ++n) {
                        if (!should_extract(names[n]->name, list_start, argc, argv)) {
                            continue;
                        }
                        if (out_dir) {
                            int out_dir_len = strlen(out_dir);
                            out_filename = (char*)malloc(out_dir_len + strlen(names[n]->name) + 2);
                            strcat(out_filename, out_dir);
                            if ('/' != out_dir[out_dir_len - 1]) {
                                strcat(out_filename, "/");
                            }
                            strcat(out_filename, names[n]->name);
                        } else {
                            out_filename = names[n]->name;
                        }
                        printf("extracting %s\n", names[n]->name);
                        status |= get_file(names[n]->name, out_filename);
                        if (out_dir) {
                            free(out_filename);
                        }
                }
                return status;
        } else if (!strcmp(argv[x], "put")) {
                char *local_name;
                char *atari_name;
                ++x;
                if (x != argc && !strcmp(argv[x], "-l")) {
                        cvt_ending = 1;
                        ++x;
                }
                if (x == argc) {
                        fprintf(stderr, "Missing file name to put\n");
                        return -1;
                }
                local_name = argv[x];
                if (strrchr(local_name, '/'))
                        atari_name = strrchr(local_name, '/') + 1;
                else
                        atari_name = local_name;
                printf("%s\n", atari_name);
                if (x + 1 != argc)
                        atari_name = argv[++x];
                return put_file(local_name, atari_name);
        } else if (!strcmp(argv[x], "w")) {
                int status = 0;
                ++x;
                while (argv[x]) {
                        printf("writing %s\n", argv[x]);
                        status |= put_file(argv[x], argv[x]);
                        ++x;
                }
                return status;
        } else if (!strcmp(argv[x], "mv")) {
                char *old_name;
                char *new_name;
                ++x;
                if (!argv[x]) {
                        fprintf(stderr,"missing name\n");
                        return -1;
                } else {
                        old_name = argv[x];
                        ++x;
                }
                if (!argv[x]) {
                        fprintf(stderr,"missing name\n");
                        return -1;
                } else {
                        new_name = argv[x];
                        ++x;
                }
                return atari_rename(old_name, new_name);
        } else if (!strcmp(argv[x], "rm")) {
                char *name;
                ++x;
                if (x == argc) {
                        printf("Missing name to delete\n");
                        return -1;
                } else {
                        name = argv[x];
                }
                return rm(name, 0);
        } else {
                printf("Unknown command '%s'\n", argv[x]);
                return -1;
        }
        return 0;
}
