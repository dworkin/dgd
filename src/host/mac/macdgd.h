# include <Types.h>
# include <Memory.h>
# include <stddef.h>
# include <setjmp.h>

# define malloc(size)	NewPtr(size)
# define free(ptr)	DisposePtr(ptr)
# define frame		iframe
# define EOF		(-1)

extern int	getevent(void);
extern void	exit(int status);
extern void	abort(void);
extern void	qsort(void *arr, size_t size, size_t sz,
		      int (*cmp)(const void *a, const void *b));

extern void	tminit(void);
extern Uint	m2utime(long t);

# ifdef INCLUDE_CTYPE

# define CTYPE_ALPHA	0x01	/* in alphabet */
# define CTYPE_ALNUM	0x02	/* alphanumeric */
# define CTYPE_UPPER	0x04	/* upper case */
# define CTYPE_LOWER	0x08	/* lower case */
# define CTYPE_DIGIT	0x10	/* digit */
# define CTYPE_XDIGIT	0x20	/* xdigit */

extern char ctype[];

# define isalpha(c)	(ctype[UCHAR(c)] & CTYPE_ALPHA)
# define isalnum(c)	(ctype[UCHAR(c)] & CTYPE_ALNUM)
# define isupper(c)	(ctype[UCHAR(c)] & CTYPE_UPPER)
# define islower(c)	(ctype[UCHAR(c)] & CTYPE_LOWER)
# define isdigit(c)	(ctype[UCHAR(c)] & CTYPE_DIGIT)
# define isxdigit(c)	(ctype[UCHAR(c)] & CTYPE_XDIGIT)

extern int	tolower(int c);
extern int	toupper(int c);

# endif	/* INCLUDE_CTYPE */


# ifdef INCLUDE_FILE_IO

/* open flags */
# define O_RDONLY	0x00
# define O_WRONLY	0x01
# define O_RDWR		0x02
# define O_APPEND	0x04

# define O_CREAT	0x08
# define O_TRUNC	0x10
# define O_EXCL		0x20

# define O_BINARY	0x00

/* lseek flags */
# define SEEK_SET	0
# define SEEK_CUR	1
# define SEEK_END	2

/* stat info */
# define S_IFMT   	0x03
# define S_IFREG	0x01
# define S_IFDIR	0x02

struct stat {
    short st_mode;	/* file type */
    long st_size;	/* size */
    long st_mtime;	/* modification time */
};

# define F_OK		0
# define R_OK		4
# define W_OK		2

extern void	fsinit(long fcrea, long ftype);
extern char	*getpath(char *buf, short vref, unsigned char *fname);
extern char	*getfile(char *buf, long type);

# endif	/* INCLUDE_FILE_IO */


extern int	memcmp(const void *b1, const void *b2, size_t len);
extern void	*memchr(const void *b, int c, size_t len);
extern void	*memset(void *b, int c, size_t len);
extern void	*memcpy(void *dst, const void *src, size_t len);
extern size_t	strlen(const char *s);
extern int	strcmp(const char *p, const char *q);
extern int	strncmp(const char *p, const char *q, size_t len);
extern char	*strchr(const char *p, int c);
extern char	*strrchr(const char *p, int c);
extern char	*strpbrk(const char *p, const char *set);
extern char	*strcpy(char *s, const char *q);
extern char	*strncpy(char *s, const char *q, size_t len);
extern char	*strcat(char *s, const char *q);
extern long	strtol(const char *s, char **end, int base);
extern int	sprintf(char *buf, const char *fmt, ...);
