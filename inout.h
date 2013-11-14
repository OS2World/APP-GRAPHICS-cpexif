/* big and little endian according to TIFF specification */
#define BE	0x4D4D
#define LE	0x4949

extern void open_input(const char *);
extern void close_input(void);
extern void read_from_file(void *, size_t);
extern void set_read_pos(int, long);
extern U32 get_read_pos(void);
extern U32 convert_32b(int, const char *);
extern U16 convert_16b(int, const char *);
extern U32 read_32b(int);
extern U16 read_16b(int);
extern U16 read_8b(void);

extern void open_output(const char *);
extern void open_tmp_output(char *);
extern void close_output(void);
extern void write_to_file(void *, size_t);
extern void set_write_pos(int, long);
extern U32 get_write_pos(void);
extern void store_32b(int, char *, U32);
extern void store_16b(int, char *, U16);
extern void write_32b(int, U32);
extern void write_16b(int, U16);
extern void write_8b(U16);

extern void copy_data(size_t);
extern void copy_till_eof(void);
