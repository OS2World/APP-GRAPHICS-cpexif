#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "fail.h"

void
fail_sys(const char *format, ...)
{
	int save_errno;
	va_list argptr;

	save_errno = errno;
	va_start(argptr,format);
	vfprintf(stderr,format,argptr);
	fputs(".\n",stderr);
	va_end(argptr);
	errno = save_errno;
	perror("Error");
	exit(2);
	/* NOT REACHED */
}

void
fail_prog(const char *format, ...)
{
	va_list argptr;

	va_start(argptr,format);
	vfprintf(stderr,format,argptr);
	fputs(".\n",stderr);
	va_end(argptr);
	exit(2);
	/* NOT REACHED */
}
