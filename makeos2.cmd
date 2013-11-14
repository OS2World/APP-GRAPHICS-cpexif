gcc -O2 -c cpexif.c fail.c options.c inout.c
gcc -static -o cpexif.exe cpexif.o fail.o options.o inout.o
del cpexif.o fail.o options.o inout.o > NUL
REM lxlite cpexif.exe
