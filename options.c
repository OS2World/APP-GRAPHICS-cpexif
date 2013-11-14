#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"
#include "fail.h"

int nomakernote = 0;
int noisofix = 0;

static const char *progname;

static const char *
base_name(const char *pathname)
{
	const char *base, *pch;
	char ch;

	for (pch = base = pathname; (ch = *pch++) != '\0'; )
		if (ch == '/')
			base = pch;
	return base;
}

static void
usage_info(void)
{
	printf(
	  "Usage:\n"
	  "  %s --help\n"
	  "  %s --version\n"
	  "  %s source.jpg destination.jpg\n"
	  "      Copy the EXIF data from the source JPEG file\n"
	  "      to the destination JPEG file.\n"
	  "  %s [options] source.nef destination.jpg\n"
	  "      options:\n"
	  "          --nomakernote    do not copy the MakerNote field\n"
	  "          --noisofix       do not fix the missing ISO field\n"
	  "      Copy the EXIF data from the source NEF file\n"
	  "      (Nikon RAW file) to the destination JPEG file.\n"
	  "      Thumbnails are not copied.\n",
	  progname,progname,progname,progname);
}

static void
version_info(void)
{
	printf(
	  "%s version 0.2 - Copyright (C)2005 by Vlado Potisk\n\n"
	  "This program is free software released under the terms of the\n"
	  "GNU General Public License. There is no warranty, use it at\n"
	  "your own risk.\n\n"
	  "Visit http://www.clex.sk/cpexif/ for more information.\n",
	  progname);
}

extern char **
process_options(int ac, char **av)
{
	const char *opt;

	progname = base_name(av[0]);
	while (--ac > 0 && strncmp(*++av,"--",2) == 0) {
		opt = *av + 2;
		if (strcmp(opt,"help") == 0) {
			usage_info();
			exit(0);
		}
		if (strcmp(opt,"version") == 0) {
			version_info();
			exit(0);
		}
		if (strcmp(opt,"nomakernote") == 0)
			nomakernote = 1;
		else if (strcmp(opt,"noisofix") == 0)
			noisofix = 1;
		else
			fail_prog("Incorrect option '--%s'. "
			  "Try '%s --help' for more information",opt,progname);
	}
	if (ac != 2)
		fail_prog("Incorrect usage. "
		  "Try '%s --help' for more information",progname);
	return av;
}
