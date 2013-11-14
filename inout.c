#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cpexif.h"
#include "inout.h"
#include "fail.h"

static FILE *ifp, *ofp;
static const char *ifile, *ofile;

/*** input ***/

void
open_input(const char *file)
{
	if ( (ifp = fopen(ifile = file,"rb")) == 0)
		fail_sys("Cannot open file '%s' for reading",ifile);
}

void
close_input(void)
{
	if (fclose(ifp))
		fail_sys("Cannot close file '%s'",ifile);
}

void
read_from_file(void *buff, size_t bytes)
{
	if (fread(buff,1,bytes,ifp) != bytes) {
		if (feof(ifp))
			fail_prog("Cannot read from file '%s'.\n"
			  "Error: End of file is reached",ifile);
		fail_sys("Cannot read from file '%s'",ifile);
	}
}

void
set_read_pos(int whence, long offset)
{
	if (fseek(ifp,offset,whence) < 0)
		fail_sys("Cannot set read offset for file '%s'",ifile);
}

U32
get_read_pos(void)
{
	return ftell(ifp);
}

U32
convert_32b(int endian, const char *bytes)
{
	U32 num;
	int i;

	for (num = 0, i = 0; i < 4; i++) {
		num <<= 8;
		num += bytes[endian == BE ? i : 3 - i] & 0xFF;
	}
	return num;
}

U16
convert_16b(int endian, const char *bytes)
{
	U16 b0, b1;

	b0 = bytes[0] & 0xFF;
	b1 = bytes[1] & 0xFF;
	return endian == BE ? (b0 << 8) + b1 : (b1 << 8) + b0;
}

U32
read_32b(int endian)
{
	char buff[4];

	read_from_file(buff,4);
	return convert_32b(endian,buff);
}

U16
read_16b(int endian)
{
	char buff[2];

	read_from_file(buff,2);
	return convert_16b(endian,buff);
}

U16
read_8b(void)
{
	char buff;

	read_from_file(&buff,1);
	return buff & 0xFF;
}

/*** output ****/

void
open_output(const char *file)
{
	if ( (ofp = fopen(ofile = file,"wb")) == 0)
		fail_sys("Cannot open file %s for writing",ofile);
}

void
open_tmp_output(char *template)
{
#ifdef WIN32
	/* no mkstemp() */
	ofile = mktemp(template);
	if ( (ofp = fopen(ofile, "wb")) == 0)
		fail_sys("Cannot create temporary file '%s'",ofile);
#else
	int fd;

	if ( (fd = mkstemp(template)) < 0)
		fail_sys("Cannot create temporary file '%s'",template);
	ofile = template;
	if ( (ofp = fdopen(fd,"wb")) == 0)
		fail_sys("Cannot open temporary file '%s' for writing",ofile);
#endif
}

void
close_output(void)
{
	if (fclose(ofp))
		fail_sys("Cannot close file '%s'",ofile);
}

void
write_to_file(void *buff, size_t bytes)
{
	if (fwrite(buff,1,bytes,ofp) != bytes)
		fail_sys("Cannot write to file '%s'",ofile);
}

void
set_write_pos(int whence, long offset)
{
	if (fseek(ofp,offset,whence) < 0)
		fail_sys("Cannot set write offset for '%s'",ofile);
}

U32
get_write_pos(void)
{
	return ftell(ofp);
}

void
store_32b(int endian, char *bytes, U32 num)
{
	int i;

	for (i = 0; i < 4; i++) {
		bytes[endian == LE ? i : 3 - i] = (num & 0xFF);
		num >>= 8;
	}
}

void
store_16b(int endian, char *bytes, U16 num)
{
	U16 b0, b1;

	b0 = num & 0xFF;
	b1 = (num >> 8) & 0xFF;
	if (endian == LE) {
		bytes[0] = b0;
		bytes[1] = b1;
	}
	else {
		bytes[1] = b0;
		bytes[0] = b1;
	}
}

void
write_32b(int endian, U32 num)
{
	char buff[4];

	store_32b(endian,buff,num);
	write_to_file(buff,4);
}

void
write_16b(int endian, U16 num)
{
	char buff[2];

	store_16b(endian,buff,num);
	write_to_file(buff,2);
}

void
write_8b(U16 num)
{
	char buff;

	buff = num & 0xFF;
	write_to_file(&buff,1);
}

/*** copy ***/

#define COPY_BUFF	16384
static char copy_buff[COPY_BUFF];

void
copy_data(size_t bytes)
{
	size_t chunk;

	for (; bytes > 0; bytes -= chunk) {
		chunk = bytes > COPY_BUFF ? COPY_BUFF : bytes;
		read_from_file(copy_buff,chunk);
		write_to_file(copy_buff,chunk);
	}
}

void
copy_till_eof(void)
{
	size_t chunk;

	while ( (chunk = fread(copy_buff,1,COPY_BUFF,ifp)) )
		write_to_file(copy_buff,chunk);
	if (ferror(ifp))
		fail_sys("Cannot read from file '%s'",ifile);
}
