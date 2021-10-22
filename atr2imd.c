/* Convert Nick Kennedy's .ATR (Atari) disk image file format to
 * Dave Dunfield's .IMD (ImageDisk) file format
 *
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
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

/* A loaded .ATR image */

struct atr {
	long size; /* Size of data in bytes */
	int type; /* 0 = 128 byte sectors, 1 = 256 byte sectors. */
	unsigned char *data; /* Sector data */

	int cyls; /* No. of tracks */
	int sects; /* No. of sectors per track */
	int sec_size; /* Bytes per sector */
	int dd; /* Disk type to use:
		0 = 90 K disk: 18 128-byte sectors / track
		1 = 130 K disk: 26 128-byte sectors / track
		2 = 180 K disk: 18 256-byte sectors / track
		*/
	int *map; /* Interleave map */
};

/* Free .atr image */

void free_atr(struct atr *atr)
{
	if (atr->data)
		free(atr->data);
	free(atr);
}

/* Interleave map for 90K disks */
int sd_map[] = 
  { 1, 3, 5, 7, 9, 11, 13, 15, 17, 2, 4, 6, 8, 10, 12, 14, 16, 18 };

/* Interleave map for 130K disks */
int dd_map[] =
  { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26 };

/* Interleave map for 180K disks */
int hd_map[] = 
  { 1, 3, 5, 7, 9, 11, 13, 15, 17, 2, 4, 6, 8, 10, 12, 14, 16, 18 };

/* Read .atr image */

struct atr *read_atr(char *name, int force_ed, int force_dd)
{
	struct atr *atr;
	FILE *f = fopen(name, "rb");
	unsigned char header[16];
	int sec_size; /* Sector size from header */
	if (!f) {
		fprintf(stderr, "Couldn't open %s\n", name);
		return 0;
	}
	if (1 != fread(header, 16, 1, f)) {
		fprintf(stderr, "Header missing from %s\n", name);
		fclose(f);
		return 0;
	}
	if (header[0] != 0x96 || header[1] != 0x02) {
		fprintf(stderr, "Warning.. magic number is not 0x0296\n");
	}
	atr = (struct atr *)malloc(sizeof(struct atr));
	atr->map = 0;
	atr->data =0;

	/* Get image size from header */
	atr->size = ((long)header[2] + ((long)header[3] << 8) + ((long)header[6] << 16)) * 0x10;


	/* Get sector size from header */
	sec_size = header[4] + (header[5] << 8);

	/* Check sector size */
	if (sec_size == 0x80)
		atr->type = 0;
	else if (sec_size == 0x100)
		atr->type = 1;
	else {
		fprintf(stderr, "Unknown sector size %d\n", sec_size);
		free_atr(atr);
		fclose(f);
		return 0;
	}

	/* Get actual image size, don't trust size from header */
	fseek(f, 0, SEEK_END);
	atr->size = ftell(f) - 16;
	fseek(f, 16, SEEK_SET);

	/* Allocate space for image: give extra space to expand boot sectors */
	atr->data = (unsigned char *)malloc(atr->size + 3 * 128);
	if (!atr->data) {
		fprintf(stderr, "Couldn't allocate space for image\n");
		free_atr(atr);
		fclose(f);
		return 0;
	}

	/* Read image */
	if (1 != fread(atr->data, atr->size, 1, f)) {
		fprintf(stderr, "Error reading from file\n");
		free_atr(atr);
		fclose(f);
		return 0;
	}
	/* Success! */
	fclose(f);

	/* Deal with boot sectors */
	if (sec_size == 256) {
		if ((atr->size >> 7) & 1) {
			/* It has an odd number of 128 bytes chunks.. first
			   three sectors are 128 bytes.  Expand image to make
			   physical sectors. */
			memmove(atr->data + 768, atr->data + 384, atr->size - 384);
			memcpy(atr->data + 512, atr->data + 256, 128);
			memset(atr->data + 640, 0, 128);

			memcpy(atr->data + 256, atr->data + 128, 128);
			memset(atr->data + 384, 0, 128);

			memset(atr->data + 128, 0, 128);
			atr->size += 384;
		} else {
			int x;
			int flg = 0;
			for (x = 384; x != 768; ++x)
				if (atr->data[x])
					flg = 1;
			if (!flg) {
				/* Bytes 384 - 768 are all zeros.  SIO2PC does this */
				memcpy(atr->data + 512, atr->data + 256, 128);
				memset(atr->data + 640, 0, 128);

				memcpy(atr->data + 256, atr->data + 128, 128);
				memset(atr->data + 384, 0, 128);

				memset(atr->data + 128, 0, 128);
			} else {
				/* We already have physical sectors */
			}
		}
	}

	printf("Converting %s (%ld %dB sectors) ", name, atr->size/sec_size, sec_size);

	/* Decide on best disk format to use */
	if (sec_size == 128 && atr->size <= 128 * 18 * 40 && !force_ed && !force_dd) {
		printf("=> 90K disk\n");
		atr->cyls = 40;
		atr->sec_size = 128;
		atr->sects = 18;
		atr->dd = 0;
		atr->map = sd_map;
	} else if (sec_size == 128 && atr->size <= 128 * 26 * 40 && !force_dd) {
		printf("=> 130K disk\n");
		atr->cyls = 40;
		atr->sec_size = 128;
		atr->sects = 26;
		atr->dd = 1;
		atr->map = dd_map;
	} else if (sec_size == 256 && atr->size <= 256 * 18 * 40) {
		printf("=> 180K disk\n");
		atr->cyls = 40;
		atr->sec_size = 256;
		atr->sects = 18;
		atr->dd = 2;
		atr->map = hd_map;
	} else {
		printf("\n");
		fprintf(stderr,"Unknown format\n");
		free(atr->data);
		free(atr);
		return 0;
	}
	return atr;
}

/* True if all bytes are the same */

int is_same(unsigned char *data, int len)
{
	int c = data[0];
	int x;
	for (x = 1; x != len; ++x)
		if (data[x] != c)
			return 0;
	return 1;
}

/* Convert IMD file */

int write_imd(struct atr *atr, char *dest_name, char *comment)
{
	FILE *f;
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	int cyl;
	int sect;
	int x;

	f = fopen(dest_name, "rb");
	if (f) {
		char buf[80];
		fclose(f);
		printf("%s already exists.  Overwrite (y,n)?", dest_name);
		fgets(buf,sizeof(buf)-1,stdin);
		if (buf[0] != 'y' && buf[0] != 'Y') {
			printf("Skipping...\n");
			return 0;
		}
	}

	f = fopen(dest_name, "wb");

	if (!f) {
		fprintf(stderr,"Couldn't open %s for writing\n", dest_name);
		return 1;
	}

	/* Write timestamp */
	fprintf(f, "ATR2IMD 1.0: %2.2d/%2.2d/%4.4d %2.2d:%2.2d:%2.2d\n",
	       tm->tm_mday,tm->tm_mon + 1,tm->tm_year + 1900,tm->tm_hour,
	       tm->tm_min,tm->tm_sec);

	/* Write comment */
	fprintf(f, "%s\n\x1a", comment);

	/* Write tracks */
	for (cyl = 0; cyl != atr->cyls; ++cyl) {
		if (atr->dd)
			fputc(5, f); /* 250 Kbps MFM */
		else
			fputc(2, f); /* 250 Kbps FM */
		fputc(cyl, f); /* Cylinder */
		fputc(0, f); /* Head */
		fputc(atr->sects, f); /* Number of sectors */
		if (atr->sec_size == 256)
			fputc(1, f); /* Bytes per sector 0 = 128 */
		else
			fputc(0, f); /* Bytes per sector 1 = 256 */
		/* Sector map */
		for (x = 0; x != atr->sects; ++x) {
			fputc(atr->map[x], f);
		}
		/* Cylinder map (empty) */
		/* Head map (empty) */
		/* Sectors */
		for (x = 0; x != atr->sects; ++x) {
			int ofst;
			int dbl = 0;
			sect = atr->map[x] - 1;
			ofst = atr->sec_size * (cyl * atr->sects + sect);
			if (ofst >= atr->size) {
				fputc(2, f);
				fputc(~0, f);
			} else if (is_same(atr->data + ofst, atr->sec_size)) {
				fputc(2, f);
				fputc(~atr->data[ofst], f);
			} else {
				int y;
				fputc(1, f);
				if (dbl) {
					for (y = 0; y != 128; ++y) {
						fputc(~atr->data[ofst + y], f);
					}
					for (y = 0; y != 128; ++y) {
						fputc(~0, f);
					}
				} else
					for (y = 0; y != atr->sec_size; ++y) {
						fputc(~atr->data[ofst + y], f);
					}
			}
		}
	}

	fclose(f);
	return 0;
}

int main(int argc, char *argv[])
{
	int x;
	int err = 0;
	int did = 0;
	char *comment = 0;
	int force_ed = 0;
	int force_dd = 0;

	/* Parse args */

	for (x = 1; argv[x]; ++x) {
		if (argv[x][0] == '-') {
			/* Some kind of option */
			if (!strcmp(argv[x], "--comment") && argv[x + 1])
				comment = argv[++x];
			else if (!strcmp(argv[x], "--sd")) {
				force_ed = 0;
				force_dd = 0;
			} else if (!strcmp(argv[x], "--ed")) {
				force_ed = 1;
				force_dd = 0;
			} else if (!strcmp(argv[x], "--dd")) {
				force_ed = 0;
				force_dd = 1;
			} else {
				err = 1;
				break;
			}
		} else {
			char *p;
			struct atr *atr;
			char dest_name[1024];
			char cmnt[1024];
			char *source_name = argv[x];

			/* Create destination name based on source name */
			strcpy(dest_name, source_name);
			if ((p = strrchr(dest_name, '.')))
				*p = 0;
			strcat(dest_name, ".imd");

			/* Create comment if none provided */
			if (!comment) {
				sprintf(cmnt, "Converted from file %s", source_name);
				comment = cmnt;
			}

			/* Read .atr file */
			if (!(atr = read_atr(source_name, force_ed, force_dd)))
				return 1;

			/* Write .imd file */
			if (write_imd(atr, dest_name, comment))
				return 1;

			/* Free .atr image */
			free_atr(atr);

			/* Reset options */
			comment = 0;
			did = 1;
		}
	}

	if (!did || err) {
		fprintf(stderr,"Convert Nick Kennedy's .ATR (ATARI) disk image file format to\n");
		fprintf(stderr,"Dave Dunfield's .IMD (ImageDisk) file format.\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"       version 1.0\n");
		fprintf(stderr,"       by: Joe Allen (2011)\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"atr2imd [options] filename\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"  --comment <comment>   Comment to put in .IMD file (otherwise file name\n");
		fprintf(stderr,"                        is used as the comment)\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"atr2imd creates the smallest disk image needed to fit the .atr file.\n");
		fprintf(stderr,"These options can be used to create a larger than necessary disk image:\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"  --sd                  Force single density (90K disk, 128 byte FM sectors)\n");
		fprintf(stderr,"  --ed                  Force medium density (130K disk, 128 byte MFM sectors)\n");
		fprintf(stderr,"  --dd                  Force double density (180K disk, 256 byte MFM sectors)\n");
		return 1;
	}

	return 0;
}
