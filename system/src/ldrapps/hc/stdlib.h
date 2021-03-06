//
// QSINIT API
// stdlib header
//
#ifndef QSINIT_STDLIB
#define QSINIT_STDLIB

#ifdef __cplusplus
extern "C" {
#endif

#include "clib.h"
#include "malloc.h"
#include "stddef.h"

#define SEEK_SET   (0)
#define SEEK_CUR   (1)
#define SEEK_END   (2)
#define EOF        (-1)

#define _MAX_DRIVE    3   ///< maximum length of drive component
#define _MAX_DIR    256   ///< maximum length of path component
#define _MAX_FNAME  256   ///< maximum length of file name component
#define _MAX_EXT    256   ///< maximum length of extension component
#define _MAX_NAME   256   ///< maximum length of file name (with extension)
#define _MAX_PATH   QS_MAXPATH

typedef unsigned   FILE;

int    __stdcall strcmp (const char *s1, const char *s2);

int    __stdcall atoi   (const char *ptr);

s64t   __stdcall atoi64 (const char *ptr);

double __stdcall atof   (const char *ptr);

char*  __stdcall _gcvt  (double value, int digits, char *str);

char*  __stdcall strstr (const char *str, const char *substr);

size_t __stdcall strspn (const char *str, const char *charset);

char*  __stdcall _strspnp(const char *str, const char *charset);

char*  __stdcall strrchr(const char *s, int c);

char*  __stdcall strupr(char *str);

char*  __stdcall strlwr(char *str);

/** binary comparision.
    This function compatible with watcom bcmp, but returns position of
    found difference + 1.
    @param  s1      first binary array
    @param  s2      second binary array
    @param  length  length of data
    @return position of found difference + 1 or 0 if arrays are equal */
u32t   __stdcall bcmp   (const void *s1, const void *s2, u32t length);

s32t   __stdcall strtol (const char *ptr, char **endptr, int base);

s64t   __stdcall strtoll(const char *ptr, char **endptr, int base);

u32t   __stdcall strtoul(const char *ptr, char **endptr, int base);

u64t   __stdcall strtoull(const char *ptr, char **endptr, int base);

double __stdcall strtod (const char *str, char **endptr);

char*  __stdcall strpbrk(const char *str, const char *charset);

char*  __stdcall strcat (char *dst, const char *src);

char*  __stdcall strdup (const char *str);

int    __cdecl   sscanf (const char *in_string, const char *format, ...);

/** stricmp function.
    current code page affects on comparition! */
int    __stdcall stricmp(const char *s1, const char *s2);

char*  __stdcall strncat(char *dst, const char *src, size_t n);

/** dynamically allocated strcat.
    See also sprintf_dyn() and vsprintf_dyn().

    @param  dst  malloc-ed buffer for string, can be 0
    @param  src  string to add
    @return old or realloced dst value with added src string,
            must be freed by free() */
char*  __stdcall strcat_dyn(char *dst, const char *src);

/** exit function.
    exit() called in DLL will terminate current process, but may leave DLL
    loaded, because DLL modules are global.

    @param errorcode  Error code */
void   __stdcall exit   (int errorcode);

/** immediate exit function.
    _exit() called in DLL will terminate current process, but may leave DLL
    loaded, because DLL modules are global.

    @param errorcode  Error code */
void   __stdcall _exit  (int errorcode);

/** immediate exit with a popup message.
    Function shows message box in MSG_POPUP mode and then terminates the
    current process.
    @param errorcode  Error code */
void   __stdcall exit_with_popup(const char *msg, int errorcode);

/** install exit proc.
    Not available in DLL modules.
    @param func  Additional exit proc to install
    @return zero on success */
int    __stdcall atexit (void (*func)(void));

void   __stdcall abort  (void);

/** query original command line.
    Get command line with the program name removed. Function is compatible
    with watcom _bgetcmd.
    @param cmd_line  Buffer for the command line, can be 0
    @param len       Size of buffer above
    @return length command line, excluding the terminating null character */
int    __stdcall _bgetcmd(char *cmd_line, int len);

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    0xFF

int    __stdcall getpid (void);

void   __stdcall perror (const char *prefix);

/** special printf version.
    Function acts as normal printf() as long as QTLS_TPRINTF TLS variable
    value is zero (default). FILE* pointer may be written into it to redirect
    output to another file.
    Function is designed to replace printf() transparently. */
int    __cdecl   tprintf(const char *fmt, ...);

int    __cdecl   sprintf(char *buf, const char *format, ...);

/// sprintf to dynamically allocated buffer
char*  __cdecl   sprintf_dyn(const char *format, ...);

int    __stdcall isspace(int cc);

int    __stdcall isalpha(int cc);

int    __stdcall isalnum(int cc);

int    __stdcall isdigit(int cc);

int    __stdcall isxdigit(int cc);

#define gcvt _gcvt
#define itoa _itoa
#define ltoa _ltoa
#define utoa _utoa
#define ultoa _ultoa
#define ulltoa _utoa64
#define strspnp _strspnp

int    __stdcall abs(int value);

int    __stdcall unlink(const char *path);

int    __stdcall remove(const char *path);

int    __stdcall rename(const char *oldname, const char *newname);

// fopen supports binary mode only!
FILE*  __stdcall fopen  (const char *filename, const char *mode);

// watcom runtime compatible
FILE*  __stdcall _fsopen(const char *filename, const char *mode, int share);

#define SH_COMPAT   0x00    ///< compatibility mode
#define SH_DENYRW   0x10    ///< deny read/write mode
#define SH_DENYWR   0x20    ///< deny write mode
#define SH_DENYRD   0x30    ///< deny read mode
#define SH_DENYNO   0x40    ///< deny none mode

int    __stdcall fclose (FILE *fp);

size_t __stdcall fread  (void *buf, size_t elsize, size_t nelem, FILE *fp);

size_t __stdcall fwrite (const void *buf, size_t elsize, size_t nelem, FILE *fp);

int    __stdcall ferror (FILE *fp);

int    __stdcall feof   (FILE *fp);

int    __stdcall fseek  (FILE *fp, s32t offset, int origin);

s32t   __stdcall lseek  (int handle, s32t offset, int origin);

s64t   __stdcall _lseeki64(int handle, s64t offset, int origin);

void   __stdcall rewind (FILE *fp);

s32t   __stdcall ftell  (FILE *fp);

#define tell(fp) ftell((FILE*)(fp))

s64t   __stdcall _telli64(int handle);

int    __stdcall fflush (FILE *fp);

void   __stdcall clearerr(FILE *fp);

/** detach file from current process.
    File became shared and can be used by all processes.
    Function deny std i/o file handles.
    @param  fp   opened file
    @return zero on success */
int    __stdcall fdetach(FILE *fp);

/** close all files, opened by current process.
    PID used to determine files to close.
    Called automatically after process exit.
    @attention function will close all files (except detached by fdetach()),
               opened by global DLLs while this "process" was active
    @return number of closed files or EOF on error */
int    __stdcall fcloseall(void);

extern FILE  *stdin, *stdout, *stderr, *stdaux, *stdnul;
#pragma aux stdin  "_*";
#pragma aux stdout "_*";
#pragma aux stderr "_*";
#pragma aux stdaux "_*";
#pragma aux stdnul "_*";

int    __cdecl   fprintf(FILE *fp, const char *format, ...);
int    __cdecl   fscanf (FILE *fp, const char *format, ...);
int    __cdecl   scanf  (const char *format, ...);

int    __stdcall getchar(void);
int    __stdcall getc   (FILE *fp);
int    __stdcall fputs  (const char *buf, FILE *fp);
int    __stdcall puts   (const char *buf);
int    __stdcall fputc  (int c, FILE *fp);
int    __stdcall ungetc (int c, FILE *fp);
int    __stdcall ungetch(int ch);
#define putc(c,fp) fputc(c,fp)
#define putchar(c) fputc(c,stdout);

FILE*  __stdcall freopen(const char *filename, const char *mode, FILE *fp);

#define STDIN_FILENO    0   ///< stdin device
#define STDOUT_FILENO   1   ///< stdout device (console)
#define STDERR_FILENO   2   ///< stderr device (console)
#define STDAUX_FILENO   3   ///< stdaux device (system log and debug com port)
#define STDNUL_FILENO   4   ///< nul device

/** fdopen - limited edition (tm).
    Function simulate to standard fdopen(), but returns valid FILE* for
    STDIN_FILENO..STDNUL_FILENO only (i.e. implemented for opening of standard
    i/o handles).
    Function returns NEW handle on every call! */
FILE*  __stdcall fdopen(int handle, const char *mode);

#define TMP_MAX (36*36*36*36*36)

char*  __stdcall tmpnam(char *buffer);

/// watcom analogue
char*  __stdcall _tempnam(char *dir, char *prefix);

/** get existing temp dir path from one of TMP, TEMP, TMPDIR, TEMPDIR
    environment variables.
    @param  buffer  buffer with full name size (_MAX_PATH+1) or 0 for static.
    @return path in buffer/static buffer or 0 if no such variable/dir */
char*  __stdcall tmpdir(char *buffer);

FILE*  __stdcall tmpfile(void);

#define fileno(fp) ((int)(fp))

/** get OS handle.
    WATCOM runtime compatible function.
    @param  handle  stream i/o file handle (just use fileno() for a FILE*)
    @return QS i/o file handle or 0 if no one. */
qshandle __stdcall _os_handle(int handle);

long   __stdcall filelength(int handle);

s64t   __stdcall _filelengthi64(int handle);

int    __stdcall isatty(int handle);

int    __stdcall _chsize(int handle, u32t size);

int    __stdcall _chsizei64(int handle, u64t size);

#define chsize _chsize

#define setfsize(fp,pos) _chsize((int)(fp),pos)

#define fsize(fp) filelength((int)(fp))

void   __stdcall _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext);

/** get full path.
    This function is compatible with watcom _fullpath.
    @param  buffer  target path buffer. NULL can be used - result value will
                    be malloc()-ed
    @param  path    file or path name. NULL can be used for current dir
    @param  size    length of buffer
    @return full path. If buffer was 0 - it must be free()-d */
char*  __stdcall _fullpath(char *buffer, const char *path, size_t size);

char*  __stdcall getenv (const char *name);

/** change environment variable.
    function simulate to watcom setenv.
    @attention this call invalidates all pointers, returned by getenv(), for
               all threads of the current process.
    @param name            variable name
    @param newvalue        variable new value
    @param overwrite       allow overwrite previous value
    @return zero on success */
int    __stdcall setenv (const char *name, const char *newvalue, int overwrite);

#define unsetenv(name) setenv(name,0,1)

int    __stdcall clearenv(void);

/** searches for the file.
    Watcom clib compatible function.
    Searches for the file specified by name in the list of directories assigned
    to the environment variable specified by env_var.
    Common values for env_var are PATH, LIB and INCLUDE.

    @param [out] pathname  buffer for target path (_MAX_PATH+1 bytes)
    @return path in "pathname" or empty string in it */
void   __stdcall _searchenv(const char *name, const char *env_var, char *pathname);

#define  R_OK    4   ///< test for read permission
#define  W_OK    2   ///< test for write permission
#define  X_OK    1   ///< test for execute permission
#define  F_OK    0   ///< test for existence of file

int    __stdcall _access(const char *path, int mode);

#define access _access

void   __stdcall qsort  (void *base, size_t num, size_t width,
                         int __stdcall (*compare)(const void *, const void *));

/***************************************************************************/
// not std C lib, but usable functions
/***************************************************************************/

/** open, read, close file & returns buffer with it.
    Buffer must be freed by hlp_memfree() - for compatibility with hlp_freadfull().
    Function sets errno value as any other fxxxx func.
    Note, that block allocated with QSMA_LOCAL flag (process owned).

    @param [in]  name     File name.
    @param [out] bufsize  File size.
    @return buffer with file or 0 if no file/no memory */
void*  __stdcall freadfull(const char *name, unsigned long *bufsize);

/** open, write & close file.
    Function sets errno value as any other fxxxx func.
    @param [in]  name     File name.
    @param [in]  buf      File data.
    @param [in]  bufsize  File size.
    @return errno value or 0 on success */
int    __stdcall fwritefull(const char *name, void *buf, unsigned long bufsize);

/** change file ext.
    @param [in]  name     File name.
    @param [in]  ext      New file ext.
    @param [out] path     New file name (at least _MAX_PATH len).
    @return path value */
char*  __stdcall _changeext(const char *name, const char *ext, char *path);

/** split file name to dir & name.
    @param [in]  fname    File name to split.
    @param [out] dir      Directory, with '\' only in rare cases like \name
                          or 1:\name (at least _MAX_PATH len, can be 0).
    @param [out] name     Name without path (at least _MAX_PATH len, can be 0).
    @return name value, or dir if name==0 */
char*  __stdcall _splitfname(const char *fname, char *dir, char *name);

/** file name pattern match.
    @param  pattern   Pattern (can be 0 or "" -> *.* assumed).
    @param  str       String (file name) to compare.
    @return true/false flag */
int    __stdcall _matchpattern(const char *pattern, const char *str);

/** file name replacement by a pattern.
    REN command-like name processing
    @param  [in]  frompattern  Pattern to match.
    @param  [in]  topattern    Destination pattern.
    @param  [in]  from         String (file name) to change.
    @param  [out] to           Buffer for output modified string.
    @return 0 if pattern is not match or invalid parameter, 1 on success */
int    __stdcall _replacepattern(const char *frompattern, const char *topattern,
                                 const char *from, char *to);

/** patch binary data.
    Search "times" times in "len" bytes from "addr" for "str" with size "slen"
    and replace "plen" bytes at offset "poffs" from found location to data,
    located in "pstr" array.
    if slen==0, zero-term string assumed in "str".
    optionally return unprocessed len in "stoplen".
    @return number of unprocessed replaces. */
int    __stdcall patch_binary(u8t *addr, u32t len, u32t times, void *str, u32t slen,
                              u32t poffs, void*pstr, u32t plen, u32t *stoplen);

/** make binary dump string.
@code
    String format:
      00000XXXX: 41 42 43 00 00 00 00 00 00 00 00 00 ?? ?? ?? ??  ABC........
@endcode
    @param  buf       Buffer of enough length (can be 0, the application owned
                      heap block is retunrned in this case)
    @param  addr      Source data
    @param  ofs       Offset value for the address position
    @param  owdt      Width of the offset field
    @param  lwdt      Number of data bytes per line (0 equal to default 16)
    @param  len       Number of bytes to print (==lwdt for the full line)
    @param  rlen      # of readable bytes (if <len - remaining indicated by ??)
    @param  asc       Print ASC view (only 0x20..0x7E codes)
    @return formatted string (must be free() if buf==0) */
char*  __stdcall _bindump(char *buf, u8t *addr, u64t ofs, u8t owdt, u8t lwdt,
                          u8t len, u8t rlen, int asc);

/** random function.
    Seed initialized on module loading and individually for each module,
    it can be accessed via _RandSeed variable.
    @param range     Range value.
    @return random number in 0..range-1 */
u32t   __stdcall random(u32t range);

#define RAND_MAX (32767)
int    __stdcall rand(void);
void   __stdcall srand(u32t seed);

/** count number of characters in string.
    @param str   String
    @param ch    Character to count
    @return number of "ch" characters in string */
u32t   __stdcall strccnt(const char *str, char ch);

/// return last character of string
char   __stdcall strclast(const char *str);

// bit scan functions, returns -1 if value is 0
#ifdef __WATCOMC__
int    bsf32(u32t value);
int    bsr32(u32t value);
#pragma aux bsf32 "_*" parm routine modify exact [eax];
#pragma aux bsr32 "_*" parm routine modify exact [eax];
#else
int    __stdcall bsf32(u32t value);
int    __stdcall bsr32(u32t value);
#endif
/** bit scan reverse (BSR) for 64bit value.
    @return -1 if value is 0 */
int    __stdcall bsr64(u64t value);

/** bit scan forward (BSF) for 64bit value.
    @return -1 if value is 0 */
int    __stdcall bsf64(u64t value);

/// swap byte order
u16t   __stdcall bswap16(u16t value);
u32t   __stdcall bswap32(u32t value);
u64t   __stdcall bswap64(u64t value);

/// swap order of "len" datas with size "size"
void   __stdcall bswap  (void*mem, size_t size, size_t len);

#define SBIT_ON       0x0001        ///< set bit, clear if omitted
#define SBIT_STREAM   0x0002        ///< array is a bit stream (dword access used by default)

/** set or clear a part of bit array.
    If SBIT_STREAM used - array is a physical bit stream, i.e. zero index
    have the 7th bit of 1st byte, if no flag is used - array contain data
    in dwords in Intel byteorder format.
    @param  dst      Start of array.
    @param  pos      Index of bit in array.
    @param  count    Number of bits to change.
    @param  flags    SBIT_*. */
void   __stdcall setbits(void *dst, u32t pos, u32t count, u32t flags);

/** search for contiguous area of set or clear bits in bitmap array.
    @param  bmp      start of array.
    @param  size     size of array in *bits*.
    @param  on       search value (1/0)
    @param  length   number of bits to search
    @param  hint     position where to start
    @return position of found area or 0xFFFFFFFF if no one */
u32t   __stdcall findbits(void *bmp, u32t size, int on, u32t length, u32t hint);

// reverse search of value match/mismatch (from the end to start of block)
void*  __stdcall memrchr  (const void*mem, int  chr, u32t buflen);
u8t*   __stdcall memrchrnb(const u8t* mem, u8t  chr, u32t buflen);
u16t*  __stdcall memrchrw (const u16t*mem, u16t chr, u32t buflen);
u16t*  __stdcall memrchrnw(const u16t*mem, u16t chr, u32t buflen);
u32t*  __stdcall memrchrd (const u32t*mem, u32t chr, u32t buflen);
u32t*  __stdcall memrchrnd(const u32t*mem, u32t chr, u32t buflen);
u64t*  __stdcall memrchrq (const u64t*mem, u64t chr, u32t buflen);
u64t*  __stdcall memrchrnq(const u64t*mem, u64t chr, u32t buflen);

// exchange data in two memory blocks
void   __stdcall memxchg  (void *m1, void *m2, u32t length);

/** trim leading characters from the charset.
    @return src value with string moved to the start position. */
char*  __stdcall trimleft (char *src, char *charset);
/** trim trailing characters from the charset.
    @return src value with truncated string. */
char*  __stdcall trimright(char *src, char *charset);

/** get left/right word position.
    @param str          Source string
    @param separators   Word separator list (ex. " \t\n-;,")
    @param pos          Position in the source string ( -pos - search to the
                        left, +pos - search to the right)
    @return found position */
u32t   __stdcall getwordpos(const char *str, const char *separators, int pos);

/** replace one character to another.
    zero is not accepted as "from" parameter.
    @return number of processed replacements. */
u32t   __stdcall replacechar(char *str, char from, char to);

/** string to uint.
    Unlike strtol() it makes only hex & dec convertions, but not octal.
    Pair str2long() function available in clib.h for int values */
u32t   __stdcall str2ulong(const char *str);
/// string to int64. The same as str2ulong()
s64t   __stdcall str2int64(const char *str);
/// string to uint64. The same as str2ulong()
u64t   __stdcall str2uint64(const char *str);

/** safe memcpy.
    Function makes a memmove(), but protected by exception handler.
    Optionally it allows copying from/to page 0 (1st page is mapped as
    read-only in PAE mode and inaccessible from FLAT DS in non-paged mode).

    @param dst    Destination address.
    @param src    Source address.
    @param length Number of bytes to copy
    @param flags  Flags (MEMCPY_PG0 to allow page 0 be a part of source
                  and/or destination and MEMCPY_DI to copy memory under cli)
    @return 1 on success, 0 if exception occur */
u32t   __stdcall hlp_memcpy(void *dst, const void *src, u32t length, u32t flags);

#define MEMCPY_PG0   0x0001         ///< page 0 can be a part of src or dst
#define MEMCPY_DI    0x0002         ///< disable interrupts during copying

/// convert QSINIT error to nearest errno value
int    __stdcall qserr2errno(qserr errv);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_STDLIB
