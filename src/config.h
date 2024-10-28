/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * These may be changed, but sizeof(type) <= sizeof(int)
 *
 * UINDEX limits the number of objects
 * SECTOR limits the number of swap sectors (the size of a snapshot)
 * CINDEX limits the number of callouts
 * EINDEX limits the number of connected users
 * SSIZET limits the length of a string (best kept at 16 bits)
 *
 * default: 64K objects, 64K swap sectors, 255 users, max string length 64K
 */
# ifndef UINDEX_TYPE
# define UINDEX_TYPE	unsigned short
# define UINDEX_MAX	USHRT_MAX
# endif
# ifndef SECTOR_TYPE
# define SECTOR_TYPE	UINDEX_TYPE
# define SECTOR_MAX	UINDEX_MAX
# endif
# ifndef CINDEX_TYPE
# define CINDEX_TYPE	UINDEX_TYPE
# define CINDEX_MAX	UINDEX_MAX
# endif
# ifndef EINDEX_TYPE
# define EINDEX_TYPE	unsigned char
# define EINDEX_MAX	UCHAR_MAX
# endif
# ifndef SSIZET_TYPE
# define SSIZET_TYPE	unsigned short
# define SSIZET_MAX	USHRT_MAX
# endif

typedef UINDEX_TYPE uindex;
typedef SECTOR_TYPE Sector;
# define SW_UNUSED	SECTOR_MAX
typedef CINDEX_TYPE cindex;
typedef EINDEX_TYPE eindex;
# define EINDEX(e)	((eindex) e)
typedef SSIZET_TYPE ssizet;

# ifdef LARGENUM

# if UINDEX_MAX == USHRT_MAX
# error LARGENUM requires uindex of 4 bytes or more
# endif

typedef int64_t LPCint;
typedef uint64_t LPCuint;

# define LPCINT_MIN	0x8000000000000000LL
# define LPCINT_MAX	0x7fffffffffffffffLL
# define LPCINT_BITS	64
# define LPCINT_BUFFER	22
# define LPCUINT_MAX	0xffffffffffffffffLL

# else

typedef Int LPCint;
typedef Uint LPCuint;

# define LPCINT_MIN	0x80000000L
# define LPCINT_MAX	0x7fffffffL
# define LPCINT_BITS	32
# define LPCINT_BUFFER	12
# define LPCUINT_MAX	0xffffffffL

# endif

typedef unsigned short kfindex;
# define KFTAB_SIZE	1024
# define KFCRYPT_SIZE	128


/*
 * Gamedriver configuration.
 */

/* general */
# define BUF_SIZE	FS_BLOCK_SIZE	/* I/O buffer size */
# define MAX_LINE_SIZE	4096	/* max. line size in ed and lex (power of 2) */
# define STRINGSZ	256	/* general (internal) string size */
# define STRMAPHASHSZ	20	/* # characters to hash of map string indices */
# define STRMERGETABSZ	1024	/* general string merge table size */
# define STRMERGEHASHSZ	20	/* # characters in merge strings to hash */
# define ARRMERGETABSZ	1031	/* general array merge table size */
# define OBJHASHSZ	256	/* # characters in object names to hash */
# define COPATCHHTABSZ	1031	/* callout patch hash table size */
# define OBJPATCHHTABSZ	257	/* object patch hash table size */
# define CMPLIMIT	2048	/* compress strings if >= CMPLIMIT */
# define SWAPCHUNKSZ	10	/* # objects reconstructed in main loop */

/* comm */
# define INBUF_SIZE	2048	/* telnet input buffer size */
# define OUTBUF_SIZE	8192	/* telnet output buffer size */
# define BINBUF_SIZE	8192	/* binary/UDP input buffer size */
# define UDPHASHSZ	10	/* # characters in UDP challenge to hash */

/* swap */
# define SWAPCHUNK	(128 * 1024 * 1024)

/* interpreter */
# define MIN_STACK	5	/* minimal stack, # arguments in driver calls */
# define EXTRA_STACK	32	/* extra space in stack frames */
# define MAX_STRLEN	SSIZET_MAX	/* max string length, >= 65535 */
# define INHASHSZ	4099	/* instanceof hashtable size */

/* parser */
# define MAX_AUTOMSZ	6	/* DFA/PDA storage size, in strings */
# define PARSERULTABSZ	256	/* size of parse rule hash table */
# define PARSERULHASHSZ	10	/* # characters in parse rule symbols to hash */

/* editor */
# define NR_EDBUFS	3	/* # buffers in editor cache (>= 3) */
/*# define TMPFILE_SIZE	2097152 */ /* max. editor tmpfile size */

/* lexical scanner */
# define INCLUDEDEPTH	64	/* maximum include depth */
# define MACTABSZ	16384	/* macro hash table size */
# define MACHASHSZ	10	/* # characters in macros to hash */

/* compiler */
# define YYMAXDEPTH	500	/* parser stack size */
# define MAX_ERRORS	5	/* max. number of errors during compilation */
# define MAX_LOCALS	127	/* max. number of parameters + local vars */
# define OMERGETABSZ	512	/* inherit object merge table size */
# define VFMERGETABSZ	2048	/* variable/function merge table sizes */
# define VFMERGEHASHSZ	10	/* # characters in function/variables to hash */
# define NTMPVAL	32	/* # of temporary values for LPC->C code */

/* builtin type prefix */
# define BIPREFIX	"builtin/"
# define BIPREFIXLEN	8


class SnapshotInfo {
public:
    char valid;
    char version;
    char model;
    char typecheck;
    char secsize[2];
    char s[2];			/* short */
    char i[4];			/* Int */
    char l[8];			/* int64_t */
    char utsize;		/* sizeof(uindex) + sizeof(ssizet) */
    char desize;		/* sizeof(sector) + sizeof(eindex) */
    char psize;			/* sizeof(char*), upper nibble reserved */
    char calign;		/* align(char), upper nibble reserved */
    char snalsz;		/* align(short) + sizeof(LPCint)  */
    char ilalign;		/* align(Int) + align(int64_t) */
    char pfalsz;		/* align(char*) + sizeof(cindex) */
    char zalign;		/* align(struct) */
    char start[4];
    char elapsed[4];
    char zero1;			/* reserved (0) */
    char zero2;			/* reserved (0) */
    char zero3;			/* reserved (0) */
    char zero4;			/* reserved (0) */
    char dflags;		/* flags */
    char vmversion;		/* VM version */
    char vstr[18];
    char offset[4];

    unsigned int restore(int fd);
};

class Config {
public:
    static void modFinish(bool wait);
    static bool init(char *configfile, char *snapshot, char *snapshot2,
		     Sector *fragment);
    static char *baseDir();
    static char	*driver();
    static char	**hotbootExec();
    static int typechecking();
    static unsigned short arraySize();
    static bool attach(int port);

    static void dump(bool incr, bool boot);
    static Uint dsize(const char *layout);
    static Uint dconv(char *buf, char *rbuf, const char *layout, Uint n);
    static void dread(int fd, char *buf, const char *layout, Uint n);

    static bool statusi(Frame *f, LPCint idx, Value *v);
    static Array *status(Frame *f);
    static bool objecti(Dataspace *data, Object *obj, LPCint idx, Value *v);
    static Array *object(Dataspace *data, Object *obj);

    const char *name;	/* name of the option */
    short type;		/* option type */
    bool resolv;	/* TRUE if path name must be resolved */
    bool set;		/* TRUE if option is set */
    Uint low, high;	/* lower and higher bound, for numeric values */
    union {
	long num;	/* numeric value */
	char *str;	/* string value */
    };

private:
    static void dumpinit();
    static bool restore(int fd, int fd2);
    static void err(const char *err);
    static bool config();
    static void fdlist();
    static bool open(char *file);
    static void puts(const char *str);
    static bool close();
    static bool includes();
    static void putval(Value *v, size_t n);
};

/* utility functions */
extern LPCint strtoint(char**);
