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

#define DISK_SIZE 720

#define SECTOR_SIZE 128

/* Specific sectors */

#define SECTOR_VTOC 0x168
#define SECTOR_DIR 0x169
#define SECTOR_DIR_SIZE 8

/* Directory entry */

#define FLAG_NEVER_USER 0x00
#define FLAG_DELETED 0x80
#define FLAG_IN_USE 0x40
#define FLAG_LOCKED 0x20
#define FLAG_DOS2 0x02
#define FLAG_OPENED 0x01

struct dirent {
        unsigned char flag;
        unsigned char count_lo;
        unsigned char count_hi;
        unsigned char start_lo;
        unsigned char start_hi;
	unsigned char name[8];
	unsigned char suffix[3];
};

/* Bytes within data sectors */

#define DATA_SIZE 125 /* Normal size of data sectors */

#define DATA_NUM_USED 125
#define DATA_FILE_NUM 125 /* Upper 6 bits */
#define DATA_NEXT_HIGH 125 /* Lower 2 bits */
#define DATA_NEXT_LOW 126 /* All 8 bits */
#define DATA_BYTES 127 /* Lower 7 bits */
#define DATA_SHORT 127 /* bit 7 set for short sector */

/* Bytes within VTOC */

/* Bytes 0 - 9 is a header */
#define VTOC_TYPE 0
#define VTOC_NUM_SECTS 1
#define VTOC_NUM_UNUSED 3
#define VTOC_RESERVED 5
#define VTOC_UNUSED 6

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

FILE *disk;

void getsect(unsigned char *buf, int sect)
{
        if (!sect) {
                printf("Oops, requested sector 0\n");
                exit(-1);
        }
        sect -= 1;
        fseek(disk, sect * SECTOR_SIZE + 16, SEEK_SET);
        fread((char *)buf, SECTOR_SIZE, 1, disk);
}

void putsect(unsigned char *buf, int sect)
{
        if (!sect) {
                printf("Oops, requested sector 0\n");
                exit(-1);
        }
        sect -= 1;
        fseek(disk, sect * SECTOR_SIZE + 16, SEEK_SET);
        fwrite((char *)buf, SECTOR_SIZE, 1, disk);
}

int lower(int c)
{
        if (c >= 'A' && c <= 'Z')
                return c - 'A' + 'a';
        else
                return c;
}

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
        int load_start;
        int load_size;
        int init;
        int run;
        int size;
};

struct name *names[(SECTOR_DIR_SIZE * SECTOR_SIZE) / sizeof(struct dirent)];
int name_n;

int comp(struct name **l, struct name **r)
{
        return strcmp((*l)->name, (*r)->name);
}

/* Fine an empty directory entry to use for a new file */

int find_empty_entry()
{
        unsigned char buf[SECTOR_SIZE];
        int x, y;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                getsect(buf, x);
                for (y = 0; y != SECTOR_SIZE; y += sizeof(struct dirent)) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (!(d->flag & FLAG_IN_USE)) {
                                return ((x - SECTOR_DIR) / sizeof(struct dirent)) + (y / sizeof(struct dirent));
                        }
                }
        }
        return -1;
}


/* Find a file, return sector number of its rib */

int find_file(char *filename, int del)
{
        unsigned char buf[SECTOR_SIZE];
        int x, y;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                getsect(buf, x);
                for (y = 0; y != SECTOR_SIZE; y += sizeof(struct dirent)) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (d->flag & FLAG_IN_USE) {
                                char s[50];
                                int p = 0;
                                int r;
                                int i;
                                for (i = 0; i != sizeof(d->name); i++) {
                                        s[p++] = lower(d->name[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                r = p;
                                s[p++] = '.';
                                for (i = 0; i != sizeof(d->suffix); i++) {
                                        s[p++] = lower(d->suffix[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                if (p == r + 1) --p; /* No . if no extension */
                                s[p] = 0;
                                if (!strcmp(s, filename)) {
                                        if (del) {
                                                d->flag = 0x80;
                                                putsect(buf, x);
                                        }
                                        return (d->start_hi << 8) + d->start_lo;
                                }
                        }
                }
        }
        return -1;
}

/* Read a file */

void read_file(int sector, FILE *f)
{

        do {
                unsigned char buf[SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;
                int short_sect = 0;

                getsect(buf, sector);

                next = (int)buf[DATA_NEXT_LOW] + ((int)(0x3 & buf[DATA_NEXT_HIGH]) << 8);
                file_no = ((buf[DATA_FILE_NUM] >> 2) & 0x3F);
                bytes = buf[DATA_BYTES];

//                bytes = (buf[DATA_BYTES] & 0x7F); /* Correct for 256 byte sectors? */
#if 0
                if (buf[DATA_SHORT] & 0x80)
                        short_sect = 1;
                else
                        short_sect = 0;
#endif

                // printf("Sector %d: next=%d, bytes=%d, file_no=%d, short=%d\n",
                //        sector, next, bytes, file_no, short_sect);

                fwrite(buf, bytes, 1, f);

                sector = next;
        } while(sector);
}

/* cat a file */

void cat(char *name)
{
        int sector = find_file(name, 0);
        if (sector == -1) {
                printf("File '%s' not found\n", name);
                exit(-1);
        } else {
                /* printf("Found file.  Sector of rib is %d\n", sector); */
                read_file(sector, stdout);
        }
}

/* get a file from the disk */

int get_file(char *mdos_name, char *local_name)
{
        int sector = find_file(mdos_name, 0);
        if (sector == -1) {
                printf("File '%s' not found\n", mdos_name);
                return -1;
        } else {
                FILE *f = fopen(local_name, "w");
                if (!f) {
                        printf("Couldn't open local file '%s'\n", local_name);
                        return -1;
                }
                /* printf("Found file.  Sector of rib is %d\n", sector); */
                read_file(sector, f);
                if (fclose(f)) {
                        printf("Couldn't close local file '%s'\n", local_name);
                        return -1;
                }
                return -1;
        }
}

/* Mark a sector as allocated or free */

void mark_space(unsigned char *cat, int start, int alloc)
{
        if (alloc) {
                cat[10 + (start >> 3)] &= ~(1 << (7 - (start & 7)));
        } else {
                cat[10 + (start >> 3)] |= (1 << (7 - (start & 7)));
        }
}

/* Delete file */

int del_file(int sector)
{
        unsigned char cat[SECTOR_SIZE];
        getsect(cat, SECTOR_VTOC);

        do {
                unsigned char buf[SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;
                int short_sect = 0;

                getsect(buf, sector);

                next = (int)buf[DATA_NEXT_LOW] + ((int)(0x3 & buf[DATA_NEXT_HIGH]) << 8);
                file_no = ((buf[DATA_FILE_NUM] >> 2) & 0x3F);
                bytes = buf[DATA_BYTES];

                // printf("Sector %d: next=%d, bytes=%d, file_no=%d, short=%d\n", sector, next, bytes, file_no, short_sect);

                mark_space(cat, sector, 0);

                sector = next;
        } while(sector);

        putsect(cat, SECTOR_VTOC);
        return 0;
}

/* Delete file name */

int rm(char *name, int ignore)
{
        int rib = find_file(name, 1);
        if (rib != -1) {
                if (del_file(rib)) {
                        printf("Error deleting file '%s'\n", name);
                        return -1;
                } else {
                }	return 0;
        } else {
                if (!ignore)
                        printf("File '%s' not found\n", name);
                return -1;
        }
}

/* Count free sectors */

int amount_free(unsigned char *cat)
{
        int total = 0;
        int x;

        for (x = 0; x != DISK_SIZE; ++x) {
                if (cat[10 + (x >> 3)] & (1 << (7 - (x & 7))))
                        ++total;
        }
        return total;
}

/* Free command */

int do_free(void)
{
        int amount;
        unsigned char cat[SECTOR_SIZE];
        getsect(cat, SECTOR_VTOC);
        amount = amount_free(cat);
        printf("%d free sectors, %d free bytes\n", amount, amount * SECTOR_SIZE);
        return 0;
}

/* Check disk: regen bit map */

int do_check()
{
        unsigned char buf[SECTOR_SIZE];
        unsigned char fbuf[SECTOR_SIZE];
        int x, y;
        int total;
        char map[DISK_SIZE];
        char *name[DISK_SIZE];

        /* Mark all as free */
        for (x = 0; x != DISK_SIZE; ++x) {
                map[x] = -1;
                name[x] = 0;
        }

        /* Mark sector we can't reach as allocated */
        map[0] = 64;

        /* Mark VTOC and DIR */
        map[SECTOR_VTOC] = 64;
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x)
                map[x] = 64;

        /* Boot loader */
        map[1] = 64;
        map[2] = 64;
        map[3] = 64;

        /* Step through each file */
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                getsect(buf, x);
                for (y = 0; y != SECTOR_SIZE; y += sizeof(struct dirent)) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (d->flag & FLAG_IN_USE) {
                                char s[50];
                                int p = 0;
                                int r;
                                int i;
                                int sector;
                                int sects;
                                int count = 0;
                                int fileno = (y / sizeof(struct dirent)) + ((x - SECTOR_DIR) * SECTOR_SIZE / sizeof(struct dirent));
                                char *filename;
                                for (i = 0; i != sizeof(d->name); i++) {
                                        s[p++] = lower(d->name[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                r = p;
                                s[p++] = '.';
                                for (i = 0; i != sizeof(d->suffix); i++) {
                                        s[p++] = lower(d->suffix[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                if (p == r + 1) --p; /* No . if no extension */
                                s[p] = 0;
                                filename = strdup(s);
                                sector = (d->start_hi << 8) + d->start_lo;
                                sects = (d->count_hi << 8) + d->count_lo;
                                printf("Checking %s (file %d)\n", s, fileno);
                                do {
                                        int next;
                                        getsect(fbuf, sector);
                                        if (map[sector] != -1) {
                                                printf("  Uh oh.. sector %d already in use by %s (%d)\n", sector, name[sector] ? name[sector] : "reserved", map[sector]);
                                        }
                                        map[sector] = fileno;
                                        name[sector] = filename;
                                        ++count;
                                        next = (int)fbuf[DATA_NEXT_LOW] + ((int)(0x3 & fbuf[DATA_NEXT_HIGH]) << 8);

                                        sector = next;
                                } while (sector);
                                if (count != sects) {
                                        printf("  Warning: size in directory (%d) does not match size on disk (%d) for file %s\n",
                                               sects, count, filename);
                                }
                                printf("  Found %d sectors\n", count);
                        }
                }
        }
        total = 0;
        for (x = 0; x != DISK_SIZE; ++x) {
                if (map[x] != -1) {
                        ++total;
//                        if (map[x] == 64)
//                                printf("%d reserved\n", x);
//                        else {
//                                printf("%d file number %d (%s)\n", x, map[x], name[x]);
//                        }
                }
        }
        printf("%d sectors in use, %d sectors free\n", total, DISK_SIZE - total);

        printf("Checking VTOC...\n");
        getsect(buf, SECTOR_VTOC);
        for (x = 0; x != DISK_SIZE; ++x) {
                int is_alloc;
                if (buf[10 + (x >> 3)] & (1 << (7 - (x & 7))))
                        is_alloc = 0;
                else
                        is_alloc = 1;
                if (is_alloc && map[x] == -1) {
                        printf("vtoc shows sector %d allocated, but it should be free\n", x);
                }
                if (!is_alloc && map[x] != -1) {
                        printf("vtoc shows sector %d free, but it should be allocated\n", x);
                }
        }
        printf("done\n");
        return 0;
}

/* Allocate space for file */

int alloc_space(unsigned char *cat, int *list, int sects)
{
        while (sects) {
                int x;
                for (x = 1; x != DISK_SIZE; ++x) {
                        if (cat[10 + (x >> 3)] & (1 << (7 - (x & 7)))) {
                                *list++ = x;
                                cat[10 + (x >> 3)] &= ~(1 << (7 - (x & 7)));
                                break;
                        }
                }
                if (x == DISK_SIZE) {
                        printf("Not enough space\n");
                        return -1;
                }
                --sects;
        }
        return 0;
}

/* Write a file */

int write_file(unsigned char *cat, char *buf, int sects, int fileno, int size)
{
        int x;
        int rib_sect;
        unsigned char bf[SECTOR_SIZE];
        int list[DISK_SIZE];
        memset(list, 0, sizeof(list));

        if (alloc_space(cat, list, sects))
                return -1;

        for (x = 0; x != sects; ++x) {
                memcpy(bf, buf + (DATA_SIZE) * x, DATA_SIZE);
                if (x + 1 == sects) {
                        // Last sector
                        bf[DATA_NEXT_LOW] = 0;
                        bf[DATA_NEXT_HIGH] = 0;
                        bf[DATA_BYTES] = size;
                } else {
                        bf[DATA_NEXT_LOW] = list[x + 1];
                        bf[DATA_NEXT_HIGH] = (list[x + 1] >> 8);
                        bf[DATA_BYTES] = DATA_SIZE;
                }
                bf[DATA_FILE_NUM] |= (fileno << 2);
                size -= DATA_SIZE;
                // printf("Writing sector %d %d %d %d\n", list[x], bf[125], bf[126], bf[127]);
                putsect(bf, list[x]);
        }
        return list[0];
}

/* Write directory entry */

int write_dir(int fileno, char *name, int rib_sect, int sects)
{
        struct dirent d[1];
        unsigned char dir_buf[SECTOR_SIZE];
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
        d->start_hi = (rib_sect >> 8);
        d->start_lo = rib_sect;
        d->count_hi = (sects >> 8);
        d->count_lo = sects;
        d->flag = FLAG_IN_USE;
        
        getsect(dir_buf, SECTOR_DIR + fileno / (SECTOR_SIZE / sizeof(struct dirent)));
        memcpy(dir_buf + sizeof(struct dirent) * (fileno % (SECTOR_SIZE / sizeof(struct dirent))), d, sizeof(struct dirent));
        putsect(dir_buf, SECTOR_DIR + fileno / (SECTOR_SIZE / sizeof(struct dirent)));
        return 0;
}

/* Put a file on the disk */

int put_file(char *local_name, char *mdos_name)
{
        FILE *f = fopen(local_name, "r");
        long size;
        long up;
        long x;
        unsigned char *buf;
        unsigned char cat[SECTOR_SIZE];
        int rib_sect;
        int fileno;
        if (!f) {
                printf("Couldn't open '%s'\n", local_name);
                return -1;
        }
        if (fseek(f, 0, SEEK_END)) {
                printf("Couldn't get file size of '%s'\n", local_name);
                fclose(f);
                return -1;
        }
        size = ftell(f);
        if (size < 0)  {
                printf("Couldn't get file size of '%s'\n", local_name);
                fclose(f);
                return -1;
        }
        rewind(f);
        // Round up to a multiple of (DATA_SIZE)
        up = size + (DATA_SIZE) - 1;
        up -= up % (DATA_SIZE);
        buf = (unsigned char *)malloc(up);
        if (size != fread(buf, 1, size, f)) {
                printf("Couldn't read file '%s'\n", local_name);
                fclose(f);
                free(buf);
                return -1;
        }
        fclose(f);
#if 0
        /* Convert UNIX line endings to Atari */
        for (x = 0; x != size; ++x)
                if (buf[x] == '\n')
                        buf[x] = 0x9b;
#endif
        /* Fill with NULs to end of sector */
        for (x = size; x != up; ++x)
                buf[x] = 0;

        /* Delete existing file */
        rm(mdos_name, 1);

        /* Get cat... */
        getsect(cat, SECTOR_VTOC);

        /* Prepare directory entry */
        fileno = find_empty_entry();
        if (fileno == -1) {
                return -1;
        }

        /* Allocate space and write file */
        rib_sect = write_file(cat, buf, up / (DATA_SIZE), fileno, size);

        if (rib_sect == -1) {
                printf("Couldn't write file\n");
                return -1;
        }

        if (write_dir(fileno, mdos_name, rib_sect, up / (DATA_SIZE))) {
                printf("Couldn't write directory entry\n");
                return -1;
        }

        /* Success! */
        putsect(cat, SECTOR_VTOC);
        return 0;
}

/* Get info about file: actual size, etc. */

void get_info(struct name *nam)
{
        int state = 0;
        int sector = nam->sector;
        int total = 0;
        unsigned char bf[6];
        int ptr = 0;
        do {
                unsigned char buf[SECTOR_SIZE];
                int next;
                int file_no;
                int bytes;
                int short_sect = 0;

                getsect(buf, sector);

                next = (int)buf[DATA_NEXT_LOW] + ((int)(0x3 & buf[DATA_NEXT_HIGH]) << 8);
                file_no = ((buf[DATA_FILE_NUM] >> 2) & 0x3F);
                bytes = buf[DATA_BYTES];

//                bytes = (buf[DATA_BYTES] & 0x7F); /* Correct for 256 byte sectors? */
#if 0
                if (buf[DATA_SHORT] & 0x80)
                        short_sect = 1;
                else
                        short_sect = 0;
#endif


                // Look at file...
                if (!state) {
                        if (bytes > 6 && buf[0] == 0xFF && buf[1] == 0xFF) {
                                nam->load_start = (int)buf[2] + ((int)buf[3] << 8);
                                nam->load_size = (int)buf[4] + ((int)buf[5] << 8) + 1 - nam->load_start;
                        }
                        state = 1;
                }

                total += bytes;

                sector = next;
        } while(sector);

        nam->size = total;
}

void mdos_dir(int all, int full, int single, int only_ascii)
{
        unsigned char buf[SECTOR_SIZE];
        unsigned char rib[SECTOR_SIZE];
        int x, y;
        int rows;
        int cols = (80 / 13);
        for (x = SECTOR_DIR; x != SECTOR_DIR + SECTOR_DIR_SIZE; ++x) {
                int y;
                getsect(buf, x);
                for (y = 0; y != SECTOR_SIZE; y += sizeof(struct dirent)) {
                        struct dirent *d = (struct dirent *)(buf + y);
                        if (d->flag & FLAG_IN_USE) {
                                struct name *nam;
                                char s[50];
                                int p = 0;
                                int r;
                                int i;
                                for (i = 0; i != sizeof(d->name); i++) {
                                        s[p++] = lower(d->name[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                r = p;
                                s[p++] = '.';
                                for (i = 0; i != sizeof(d->suffix); i++) {
                                        s[p++] = lower(d->suffix[i]);
                                }
                                while (p && s[p - 1] == ' ') --p;
                                if (p == r + 1) --p; /* No . if no extension */
                                s[p] = 0;
                                nam = (struct name *)malloc(sizeof(struct name));
                                nam->name = strdup(s);
                                if (d->flag & FLAG_LOCKED)
                                        nam->locked = 1;
                                else
                                        nam->locked = 0;
                                nam->sector = d->start_lo + (d->start_hi * 256);
                                nam->sects = d->count_lo + (d->count_hi * 256);
                                nam->load_start = -1;
                                nam->load_size = -1;
                                nam->init = -1;
                                nam->run = -1;
                                nam->size = -1;
                                get_info(nam);

                                if (d->suffix[0] == 'S' && d->suffix[1] == 'Y' && d->suffix[2] == 'S')
                                        nam->is_sys = 1;
                                else
                                        nam->is_sys = 0;

                              //  printf("\nName=%s\n", nam->name);
                              //  printf("Starting sector=%d\n", nam->sector);
                              //  printf("Size in sectors=%d, %d bytes\n", nam->sects, nam->sects * SECTOR_SIZE);
                                // printf("load size=%d sectors, %d bytes\n", nam->size, (nam->size - 1) * SECTOR_SIZE + nam->last_size);
                                // printf("Initial pc=%x\n", nam->pc);
                                // printf("Load addr=%x\n", nam->load);
                                // printf("Last_size=%d\n", nam->last_size);

                                if ((all || !nam->is_sys))
                                        names[name_n++] = nam;
                        }
                }
        }
        qsort(names, name_n, sizeof(struct name *), (int (*)(const void *, const void *))comp);

        if (full) {
                int totals = 0;
                int total_bytes = 0;
                printf("\n");
                for (x = 0; x != name_n; ++x) {
                        if (names[x]->load_start != -1)
                                printf("-r%c%c%c %6d (%3d) %-13s (load_start=$%x load_end=$%x)\n",
                                       (names[x]->locked ? '-' : 'w'),
                                       (names[x]->is_cm ? 'x' : '-'),
                                       (names[x]->is_sys ? 's' : '-'),
                                       names[x]->size, names[x]->sects, names[x]->name, names[x]->load_start, names[x]->load_start + names[x]->load_size - 1);
                        else
                                printf("-r%c%c%c %6d (%3d) %-13s\n",
                                       (names[x]->locked ? '-' : 'w'),
                                       (names[x]->is_cm ? 'x' : '-'),
                                       (names[x]->is_sys ? 's' : '-'),
                                       names[x]->size, names[x]->sects, names[x]->name);
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
                                        printf("%-11s  ", names[n]->name);
                                else
                                        printf("             ");
                        }
                        printf("\n");
                }
        }
}

int main(int argc, char *argv[])
{
        int all = 0;
        int full = 0;
        int single = 0;
        int only_ascii = 0;
	int x;
	char *disk_name;
	x = 1;
	if (x == argc) {
                printf("\nAtari DOS diskette access\n");
                printf("\n");
                printf("Syntax: atr path-to-diskette command args\n");
                printf("\n");
                printf("  Commands:\n");
                printf("      ls [-la1A]                    Directory listing\n");
                printf("                  -l for long\n");
                printf("                  -a to show system files\n");
                printf("                  -1 to show a single name per line\n");
                printf("                  -A show only ASCII files\n");
                printf("      cat atari-name                Type file to console\n");
                printf("      get atari-name [local-name]   Copy file from diskette to local-name\n");
                printf("      put local-name [atari-name]   Copy file to diskette to mdos-name\n");
                printf("      free                          Print amount of free space\n");
                printf("      rm atari-name                 Delete a file\n");
                printf("\n");
                return -1;
	}
	disk_name = argv[x++];
	disk = fopen(disk_name, "r+");
	if (!disk) {
	        printf("Couldn't open '%s'\n", disk_name);
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
	                        case 'A': only_ascii = 1; break;
	                        default: printf("Unknown option '%c'\n", opt); return -1;
	                }
	        }
	        ++x;
	}

	if (x == argc) {
	        /* Just print a directory listing */
	        mdos_dir(all, full, single, only_ascii);
	        return 0;
        } else if (!strcmp(argv[x], "ls")) {
                ++x;
                goto dir;
        } else if (!strcmp(argv[x], "free")) {
                return do_free();
        } else if (!strcmp(argv[x], "check")) {
                return do_check();
	} else if (!strcmp(argv[x], "cat")) {
	        ++x;
	        if (x == argc) {
	                printf("Missing file name to cat\n");
	                return -1;
	        } else {
	                cat(argv[x++]);
	                return 0;
	        }
	} else if (!strcmp(argv[x], "get")) {
                char *local_name;
                char *mdos_name;
                ++x;
                if (x == argc) {
                        printf("Missing file name to get\n");
                        return -1;
                }
                mdos_name = argv[x];
                local_name = mdos_name;
                if (x + 1 != argc)
                        local_name = argv[++x];
                return get_file(mdos_name, local_name);
        } else if (!strcmp(argv[x], "put")) {
                char *local_name;
                char *mdos_name;
                ++x;
                if (x == argc) {
                        printf("Missing file name to put\n");
                        return -1;
                }
                local_name = argv[x];
                if (strrchr(local_name, '/'))
                        mdos_name = strrchr(local_name, '/') + 1;
                else
                        mdos_name = local_name;
                printf("%s\n", mdos_name);
                if (x + 1 != argc)
                        mdos_name = argv[++x];
                return put_file(local_name, mdos_name);
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
