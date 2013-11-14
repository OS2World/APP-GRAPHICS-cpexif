CC=gcc
CFLAGS=-Wall -pedantic -O2

cpexif: cpexif.o fail.o options.o inout.o
	$(CC) -o cpexif cpexif.o fail.o options.o inout.o
	-strip cpexif
cpexif.o: cpexif.c cpexif.h fail.h inout.h options.h 
	$(CC) -c $(CFLAGS) cpexif.c
fail.o: fail.c fail.h
	$(CC) -c $(CFLAGS) fail.c
inout.o: inout.c inout.h cpexif.h fail.h
	$(CC) -c $(CFLAGS) inout.c
options.o: options.c options.h fail.h
	$(CC) -c $(CFLAGS) options.c
clean:
	rm -f cpexif *.o core core.*
