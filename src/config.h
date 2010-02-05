/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

/* these may be changed, but sizeof(type) <= sizeof(int) */
typedef unsigned short uindex;
# define UINDEX_MAX	USHRT_MAX

typedef uindex sector;
# define SW_UNUSED	UINDEX_MAX

/* sizeof(ssizet) <= sizeof(uindex) */
typedef unsigned short ssizet;
# define SSIZET_MAX	USHRT_MAX

/* eindex can be anything */
typedef char eindex;
# define EINDEX_MAX	UCHAR_MAX
# define EINDEX(e)	UCHAR(e)


/*
 * Gamedriver configuration.  Hash table sizes should be powers of two.
 */

/* general */
# define BUF_SIZE	FS_BLOCK_SIZE	/* I/O buffer size */
# define MAX_LINE_SIZE	1024	/* max. line size in ed and lex (power of 2) */
# define STRINGSZ	256	/* general (internal) string size */
# define STRMAPHASHSZ	20	/* # characters to hash of map string indices */
# define STRMERGETABSZ	1024	/* general string merge table size */
# define STRMERGEHASHSZ	20	/* # characters in merge strings to hash */
# define ARRMERGETABSZ	1024	/* general array merge table size */
# define OBJHASHSZ	256	/* # characters in object names to hash */
# define COPATCHHTABSZ	64	/* callout patch hash table size */
# define OBJPATCHHTABSZ	128	/* object patch hash table size */
# define CMPLIMIT	2048	/* compress strings if >= CMPLIMIT */
# define SWAPCHUNKSZ	10	/* # objects reconstructed in main loop */

/* comm */
# define INBUF_SIZE	2048	/* telnet input buffer size */
# define OUTBUF_SIZE	8192	/* telnet output buffer size */
# define BINBUF_SIZE	8192	/* binary/UDP input buffer size */
# define UDPHASHSZ	10	/* # characters in UDP challenge to hash */

/* interpreter */
# define MIN_STACK	3	/* minimal stack, # arguments in driver calls */
# define EXTRA_STACK	32	/* extra space in stack frames */
# define MAX_STRLEN	SSIZET_MAX	/* max string length, >= 65535 */
# define INHASHSZ	4096	/* instanceof hashtable size */

/* parser */
# define MAX_AUTOMSZ	6	/* DFA/PDA storage size, in strings */
# define PARSERULTABSZ	256	/* size of parse rule hash table */
# define PARSERULHASHSZ	10	/* # characters in parse rule symbols to hash */

/* editor */
# define NR_EDBUFS	3	/* # buffers in editor cache (>= 3) */
/*# define TMPFILE_SIZE	2097152 */ /* max. editor tmpfile size */

/* lexical scanner */
# define MACTABSZ	1024	/* macro hash table size */
# define MACHASHSZ	10	/* # characters in macros to hash */

/* compiler */
# define YYMAXDEPTH	500	/* parser stack size */
# define MAX_ERRORS	5	/* max. number of errors during compilation */
# define MAX_LOCALS	127	/* max. number of parameters + local vars */
# define OMERGETABSZ	128	/* inherit object merge table size */
# define VFMERGETABSZ	256	/* variable/function merge table sizes */
# define VFMERGEHASHSZ	10	/* # characters in function/variables to hash */
# define NTMPVAL	32	/* # of temporary values for LPC->C code */


extern bool		conf_init	P((char*, char*, sector*));
extern char	       *conf_base_dir	P((void));
extern char	       *conf_driver	P((void));
extern int		conf_typechecking P((void));
extern unsigned short	conf_array_size	P((void));

extern void   conf_dump		P((void));
extern Uint   conf_dsize	P((char*));
extern Uint   conf_dconv	P((char*, char*, char*, Uint));
extern void   conf_dread	P((int, char*, char*, Uint));

extern bool   conf_statusi	P((frame*, Int, value*));
extern array *conf_status	P((frame*));
extern bool   conf_objecti	P((dataspace*, object*, Int, value*));
extern array *conf_object	P((dataspace*, object*));

/* utility functions */
extern Int strtoint		P((char**));
