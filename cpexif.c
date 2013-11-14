/*
 * cpexif - copies EXIF data from NEF/TIFF/JPEG file to JPEG file
 *
 * Copyright (C)2005 by Vlado Potisk
 *
 * This program is free software released under the terms
 * of the GNU General Public License. There is no warranty,
 * use it at your own risk.
 *
 * Visit http://www.clex.sk/cpexif/ for more information.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpexif.h"
#include "fail.h"
#include "inout.h"
#include "options.h"

/* JPG -> JPG mode variables */
static char *app1 = 0;		/* JPEG APP1 segment without first 12B */
static U16 app1_len = 0;	/* length of the APP1 segment */

/* NEF -> JPG mode variables and definitions */
#define IFD_SIZE	12

#define TYPE_ASCII		2
#define TYPE_USHORT		3
#define TYPE_ULONG		4
#define TYPE_URATIO		5

#define TAG_IFD0_MAKE		0x010F
#define TAG_IFD0_EXIF		0x8769
#define TAG_IFD0_GPS		0x8825
#define TAG_EXIF_ISO		0x8827
#define TAG_EXIF_MAKERNOTE	0x927C
#define TAG_EXIF_INTEROP	0xA005
#define TAG_NIKON_ISO		0x2
#define TAG_NIKON_ISOCODE	0x6

typedef struct ifd_entry {
	short int valid;		/* flag */
	char raw[IFD_SIZE];		/* literal 12 bytes */
	U16 tag, type;
	U32 count;
	char *data;				/* -> value */
	size_t data_size;		/* in bytes */
	U32 where;				/* offset in the output file */
	struct ifd_entry *next;	/* linked list */
} IFD_ENTRY;

/* sizes of one data element of certain IFD type */
static int memreq[] = { 0,1,1,2,4,8,1,1,2,4,8,4,8 };

static IFD_ENTRY *ifd0, *exif, *gps = 0, *interop = 0;
static IFD_ENTRY *makernote_field;
static U32 offset_zero;	/* offset of TIFF header in output file */

/* general variables */
static int endian;			/* TIFF structure endian */
static const char *cleanup_file = 0;

static void
cleanup(void)
{
	if (cleanup_file)
		remove(cleanup_file);
}

static void *
emalloc(size_t size)
{
	void *mem;

	if ((mem = malloc(size)) == 0)
		fail_prog("Could not allocate %lu bytes of memory",
		  (unsigned long)size);
	return mem;
}

static IFD_ENTRY *
new_entry(U16 tag, U16 type, U32 count)
{
	IFD_ENTRY *new;

	assert(type >= 1 && type <= 12);

	new = emalloc(sizeof(IFD_ENTRY));
	new->valid = 1;
	store_16b(endian,new->raw    ,new->tag   = tag);
	store_16b(endian,new->raw + 2,new->type  = type);
	store_32b(endian,new->raw + 4,new->count = count);
	store_32b(endian,new->raw + 8,0);
	new->data_size = count * memreq[type];
	new->data = new->data_size <= 4 ?
	  new->raw + 8 : emalloc(new->data_size);
	return new;
}

static IFD_ENTRY *
find_entry(U16 tag, U16 type /* 0 = any type */, IFD_ENTRY *pifd)
{
	for (; pifd; pifd = pifd->next) {
		if (!pifd->valid)
			continue;
		if (pifd->tag == tag && (type == 0 || pifd->type == type))
			return pifd;
		if (pifd->tag > tag)
			break;
	}
	return 0;
}

static void
insert_entry(IFD_ENTRY *entry, IFD_ENTRY *list)
{
	while (list->next &&
	  (list->next->valid == 0 || list->next->tag < entry->tag))
		list = list->next;
	entry->next = list->next;
	list->next = entry;
}

static IFD_ENTRY *
parse_directory(U32 start)
{
	U32 offset;
	U16 i, entries;
	IFD_ENTRY *pifd, *first, *prev;

	set_read_pos(SEEK_SET,start);
	if ( (entries = read_16b(endian) ) == 0)
		fail_prog("Empty IFD structure encountered");
	first = prev = emalloc(sizeof(IFD_ENTRY));
	first->valid = 0;	/* dummy to simplify insert operations */
	for (i = 0; i < entries; i++) {
		prev->next = pifd = emalloc(sizeof(IFD_ENTRY));
		pifd->valid = 1;
		pifd->next = 0;
		read_from_file(pifd->raw,IFD_SIZE);
		pifd->tag   = convert_16b(endian,pifd->raw);
		pifd->type  = convert_16b(endian,pifd->raw + 2);
		pifd->count = convert_32b(endian,pifd->raw + 4);
		if (pifd->type < 1 || pifd->type > 12)
			fail_prog("IFD entry with tag %X has invalid type %d",
			  pifd->tag,pifd->type);
		prev = pifd;
	}
	/* start_of_the_next_ifd = read_32b(endian); */

	for (pifd = first->next; pifd; pifd = pifd->next) {
		pifd->data_size = pifd->count * memreq[pifd->type];
		if (pifd->data_size <= 4)
			pifd->data = pifd->raw + 8;
		else {
			pifd->data = emalloc(pifd->data_size);
			offset = convert_32b(endian,pifd->raw + 8);
			set_read_pos(SEEK_SET,offset);
			read_from_file(pifd->data,pifd->data_size);
		}
	}

	return first;
}

static void
parse_nef(const char *nef_file)
{
	IFD_ENTRY *p;

	ifd0 = parse_directory(read_32b(endian));
	if ( (p = find_entry(TAG_IFD0_MAKE,TYPE_ASCII,ifd0)) == 0 ||
	  (strncmp(p->data,"NIKON",5) && strncmp(p->data,"Nikon",5)))
		fail_prog("File '%s' was not produced by a Nikon camera,\n"
		  "manufacturer is '%s'",nef_file,p ? p->data : "<unknown>");
	if ( (p = find_entry(TAG_IFD0_EXIF,TYPE_ULONG,ifd0)) == 0)
		fail_prog("No EXIF data found in '%s'",nef_file);
	exif = parse_directory(convert_32b(endian,p->data));
	if ( (p = find_entry(TAG_EXIF_INTEROP,TYPE_ULONG,exif)) )
		 interop = parse_directory(convert_32b(endian,p->data));
	if ( (p = find_entry(TAG_IFD0_GPS,TYPE_ULONG,ifd0)) )
		gps = parse_directory(convert_32b(endian,p->data));
}

static void
process_ifd0(void)
{
	static U16 allowed_tags[] = {
		0x10E /* ImageDescription */,	TAG_IFD0_MAKE,
		0x110 /* Model */,				0x112 /* Orientation */,
		0x11A /* XResolution */,		0x11B /* Yresolution */,
		0x128 /* Resolution unit */,	0x131 /* Software */,
		0x132 /* DateTime */,			0x13B /* Artist */,
		0x213 /* YCbCrPositioning */,	0x8298 /* Copyright */,
		TAG_IFD0_EXIF,					TAG_IFD0_GPS,
		0
	};
	U16 tag;
	int i;
	IFD_ENTRY *pifd;

	for (pifd = ifd0; pifd; pifd = pifd->next) {
		if (!pifd->valid)
			continue;
		for (i = 0; (tag = allowed_tags[i]); i++)
			if (pifd->tag == tag)
				break;
		if (tag == 0)
			pifd->valid = 0;
	}

	/* mandatory tags: 0x11A, 0x11B, 0x128, 0x213 */
	for (tag = 0x11A; tag <= 0x11B; tag++)
		if (find_entry(tag,0,ifd0) == 0) {
			/* X and Y resolution is 300 */
			pifd = new_entry(tag,TYPE_URATIO,1);
			store_32b(endian,pifd->data,300);
			store_32b(endian,pifd->data + 4,1);
			insert_entry(pifd,ifd0);
		}
	if (find_entry(0x128,0,ifd0) == 0) {
		/* resolution is in dpi */
		pifd = new_entry(0x128,TYPE_USHORT,1);
		store_16b(endian,pifd->data,2);
		insert_entry(pifd,ifd0);
	}
	if (find_entry(0x213,0,ifd0) == 0) {
		/* YCbCrPositioning - the usual value is 2 */
		pifd = new_entry(0x213,TYPE_USHORT,1);
		store_16b(endian,pifd->data,2);
		insert_entry(pifd,ifd0);
	}
}

/*
 * 0 = no or unknown makernote
 * 1 = IFD at offset 0
 * 2 = IFD at offset 8
 * BE = IFD with own TIFF header at offset 10 - big endian
 * LE = IFD with own TIFF header at offset 10 - little endian
 */
static int
makernote_type(void)
{
	const char *ptr;
	U16 val;

	if (makernote_field == 0)
		return 0;
	if (makernote_field->data_size < 18)
		return 0;
	ptr = makernote_field->data;
	if (strcmp(ptr,"Nikon"))
		return 1;
	val = convert_16b(BE,ptr + 10);
	if ((val == BE || val == LE) && convert_16b(val,ptr + 12) == 42)
		return val;
	return 2;
}

/* exit value: 0 = OK, -1 = error */
static int
isofix(void)
{
	static U16 isocode[] = { 80, 0, 160, 0, 320, 100 };
	int mktype, mkendian;
	U16 i, entries, iso, tag, type;
	U32 cnt;
	IFD_ENTRY *pifd;
	const char *ptr;
	size_t size;

	if (find_entry(TAG_EXIF_ISO,0,exif))
		return 0;	/* if it is not broken ... */

	mktype = makernote_type();
	size = makernote_field->data_size;
	if (mktype == 1 || mktype == 2) {
		ptr = makernote_field->data;
		mkendian = endian;
		if (mktype == 2)
			ptr += 8;
	}
	else if (mktype == BE || mktype == LE) {
		mkendian = mktype;
		ptr = makernote_field->data + 10 +
		  convert_32b(mkendian,makernote_field->data + 14);
		if (ptr > makernote_field->data + size)
			return -1;
	}
	else
		return -1;

	/* ptr = start of makernote IFD */
	/* mkendian = makernote endian (BE or LE) */
	iso = 0;
	entries = convert_16b(mkendian,ptr);
	if (entries == 0 || IFD_SIZE * entries > size)
		return -1;
	for (i = 0, ptr += 2; i < entries; i++, ptr += IFD_SIZE) {
		tag  = convert_16b(mkendian,ptr);
		type = convert_16b(mkendian,ptr + 2);
		cnt  = convert_32b(mkendian,ptr + 4);
		if (mktype != 2 && tag == TAG_NIKON_ISO &&
		  type == TYPE_USHORT && (cnt == 1 || cnt == 2)) {
			iso = convert_16b(mkendian,ptr + (cnt == 1 ? 8 : 10));
			break;
		}
		if (mktype == 2 && tag == TAG_NIKON_ISOCODE &&
		  type == TYPE_USHORT && cnt == 1) {
			iso = convert_16b(mkendian,ptr + 8);
			/* 0 = iso80, 2 = iso160, 4 = iso320, 5 = iso100 */
			if (iso >= 0 && iso <= 5) {
				iso = isocode[iso];
				break;
			}
		}
	}
	if (iso == 0)
		return -1;

	pifd = new_entry(TAG_EXIF_ISO,TYPE_USHORT,1);
	store_16b(endian,pifd->data,iso);
	insert_entry(pifd,exif);

	return 0;
}

/* exit value: 0 = OK, -1 = error */
static int
adjust_makernote(void)
{
	int mktype;
	U32 delta;
	U16 type, i, entries;
	char *ptr;
	size_t size;

	mktype = makernote_type();
	if (mktype == BE || mktype == LE)
		return 0;	/* nothing to do */
	if (mktype == 1)
		ptr = makernote_field->data;
	else if (mktype == 2)
		ptr = makernote_field->data + 8;
	else
		return -1;
	size = makernote_field->data_size;

	/* offsets in the makernote IFD need to be recalculated */
	delta = get_write_pos()
	  - convert_32b(endian,makernote_field->raw + 8) - offset_zero;
	entries = convert_16b(endian,ptr);
	if (entries == 0 || IFD_SIZE * entries > size)
		return -1;
	for (i = 0, ptr += 2; i < entries; i++, ptr += IFD_SIZE) {
		type  = convert_16b(endian,ptr + 2);
		if (type < 1 || type > 12)
			return -1;
		if (convert_32b(endian,ptr + 4) * memreq[type] > 4)
			store_32b(endian,ptr + 8,
			  convert_32b(endian,ptr + 8) + delta);
	}
	return 0;
}

static void
write_ifd(IFD_ENTRY *pifd)
{
	U16 cnt;
	U32 data_offset;
	IFD_ENTRY *p;

	/* number of entries */
	for (cnt = 0, p = pifd; p; p = p->next)
		if (p->valid)
			cnt++;
	write_16b(endian,cnt);
	/* directory */
	data_offset = get_write_pos() + IFD_SIZE * cnt + 4 - offset_zero;
	for (p = pifd; p; p = p->next) {
		if (!p->valid)
			continue;
		p->where = get_write_pos();
		if (p->data_size <= 4)
			write_to_file(p->raw,IFD_SIZE);
		else {
			write_to_file(p->raw,IFD_SIZE - 4);
			write_32b(endian,data_offset);
			data_offset += p->data_size + p->data_size % 2;
		}
	}
	/* offset of next IFD */
	write_32b(endian,0);
	/* data */
	for (p = pifd; p; p = p->next)
		if (p->valid && p->data_size > 4) {
			if (p->tag == TAG_EXIF_MAKERNOTE && adjust_makernote() < 0)
				fail_prog("Unknown format of the 'MakerNote' field.\n"
				  "Consider running CPEXIF "
				  "with the --nomakernote option");
			write_to_file(p->data,p->data_size);
			if (p->data_size % 2)
				write_8b(0);	/* padding */
		}
}

static void
fill_addr(U16 tag, IFD_ENTRY *directory)
{
	U32 offset;
	IFD_ENTRY *pifd;

	pifd = find_entry(tag,0,directory);
	assert(pifd != 0);

	offset = get_write_pos() - offset_zero;
	set_write_pos(SEEK_SET,pifd->where + 8);
	write_32b(endian,offset);
	set_write_pos(SEEK_END,0);
}

static void
create_jpeg(const char *jpeg_in)
{
	U32 len32;
	U16 segment, len;
	char *jpeg_out;

	len = strlen(jpeg_in);
	jpeg_out = emalloc(len + 8);
	strcpy(jpeg_out,jpeg_in);
	strcpy(jpeg_out + len,".XXXXXX");	/* mk(s)temp() template */
	open_tmp_output(jpeg_out);
	cleanup_file = jpeg_out;

	write_16b(BE,0xFFD8);	/* JPEG SOI */
	write_16b(BE,0xFFE1);	/* JPEG APP1 */
	write_16b(BE,app1_len);
	write_to_file("Exif\0",6);			/* EXIF marker */
	offset_zero = get_write_pos();	/* TIFF header starts here */
	write_16b(BE,endian);
	write_16b(endian,42);

	if (app1) {
		/* JPEG -> JPEG */
		write_to_file(app1,app1_len - 12);
	}
	else {
		/* NEF -> JPEG */
		write_32b(endian,8);
		write_ifd(ifd0);
		fill_addr(TAG_IFD0_EXIF,ifd0);
		write_ifd(exif);
		if (interop) {
			fill_addr(TAG_EXIF_INTEROP,exif);
			write_ifd(interop);
		}
		if (gps) {
			fill_addr(TAG_IFD0_GPS,ifd0);
			write_ifd(gps);
		}

		/* fill in APP1 length at file offset 4 */
		len32 = get_write_pos() - 2;
		if (len32 > 0xFFFF)
			fail_prog("The EXIF data block is too large, "
			  "cannot copy it to a JPEG file.\n"
			  "Consider running CPEXIF with the --nomakernote option\n"
			  "in order to reduce the size of the EXIF data block");
		set_write_pos(SEEK_SET,4);
		write_16b(BE,(U16)len32 - 2);
		set_write_pos(SEEK_END,0);
	}

	/* copy the original JPEG */
	open_input(jpeg_in);
	if (read_16b(BE) != 0xFFD8)
		fail_prog("File '%s' is not a JPEG",jpeg_in);
	for (;;) {
		while ( (segment = read_8b()) == 0xFF)
			;
		if (segment == 01 || (segment >= 0xD0 && segment <= 0xD7)) {
			write_8b(0xFF);
			write_8b(segment);
			continue;
		}
		if ((len = read_16b(BE)) < 2)
			fail_prog("JPEG File '%s' is corrupted",jpeg_in);
		if (segment == 0xE0 || segment == 0xE1) {
			set_read_pos(SEEK_CUR,len - 2);	/* skip APP0 and/or APP1 */
			continue;
		}
		write_8b(0xFF);
		write_8b(segment);
		write_16b(BE,len);
		if (segment == 0xD9 /* EOI */)
			fail_prog("There is no image data in '%s'",jpeg_in);
		if (segment == 0xDA /* SOS */) {
			copy_till_eof();
			break;
		}
		else
			copy_data(len - 2);
	}
	close_input();
	close_output();
	cleanup_file = 0;

	/* copy data to preserve the file ownership */
	open_output(jpeg_in);
	open_input(jpeg_out);
	copy_till_eof();
	close_input();
	close_output();
	remove(jpeg_out);
}

static void
parse_jpg(const char *filename)
{
	U16 segment, len, id;

	for (;/* until return or break */;) {
		if (read_8b() != 0xFF)
			fail_prog("JPEG file '%s' is corrupted",filename);
		segment = read_8b();
		if (segment == 01 || (segment >= 0xD0 && segment <= 0xD7))
			continue;
		if (segment == 0xD9 /* EOI */ || segment == 0xDA /* SOS */)
			break;
		if ((len = read_16b(BE)) < 2)
			fail_prog("JPEG File '%s' is corrupted",filename);
		if (segment == 0xE1 /* APP1 */ && len >= 16 &&
		  read_32b(BE) == 0x45786966 && read_16b(BE) == 0 &&
		  ((id = read_16b(BE)) == BE || id == LE) &&
		   read_16b(id) == 42) {
			/* Exif\0\0 + TIFF header */
			endian = id;
			app1_len = len;
			app1 = emalloc(app1_len - 12);
			read_from_file(app1,app1_len - 12);
			return;
		}
		set_read_pos(SEEK_CUR,len - 2);
	}
	fail_prog("No EXIF data found in '%s'",filename);
}

static void
process_input(const char *file)
{
	U16 id;

	open_input(file);
	id = read_16b(BE);
	if (id == 0xFFD8) {
		parse_jpg(file);
		if (noisofix || nomakernote)
			fputs("WARNING: command line options ignored "
			  "in the JPEG to JPEG copy mode.\n",stderr);
	}
	else if ((id == BE || id == LE) && read_16b(id) == 42) {
		endian = id;
		parse_nef(file);
		makernote_field = find_entry(TAG_EXIF_MAKERNOTE,0,exif);
		process_ifd0();
		if (nomakernote && makernote_field)
			makernote_field->valid = 0;
		if (!noisofix && isofix() < 0)
			fputs("WARNING: Cannot find the ISO value.\n"
			  "Consider running CPEXIF with the --noisofix option.\n",
			  stderr);
	}
	else
		fail_prog("File '%s' is not a NEF, TIFF, or JPEG file",file);
	close_input();

}

int
main(int argc, char *argv[])
{
	char **av;

	av = process_options(argc,argv);

	umask(022);
	atexit(cleanup);
	process_input(av[0]);
	create_jpeg(av[1]);

	return 0;
}
