# include <Files.h>
# include <StandardFile.h>
# include <Errors.h>
# define INCLUDE_FILE_IO
# include "dgd.h"

extern Uint m2utime(long t);

typedef struct {
    short fref;		/* file ref */
    Str255 fname;	/* file name */
} fdtype;

static fdtype fdtab[20];

static long crea;	/* creator */
static long type;	/* file type */

static short vref;	/* volume refNum of current directory */
static long dirid;	/* directory ID */


/*
 * NAME:	fsinit()
 * DESCRIPTION:	initialize file functions
 */
void fsinit(long fcrea, long ftype)
{
    WDPBRec buf;
    Str255 str;

    crea = fcrea;
    type = ftype;
    buf.ioNamePtr = str;
    PBHGetVolSync(&buf);
    vref = buf.ioVRefNum;
    dirid = buf.ioWDDirID;
}


/*
 * NAME:	getpath()
 * DESCRIPTION:	get the full path of a file
 */
char *getpath(char *buf, short vref, unsigned char *fname)
{
    Str255 str;
    CInfoPBRec dir;

    buf += STRINGSZ - 1;
    buf[0] = '\0';
    memcpy(str, fname, fname[0] + 1);

    dir.dirInfo.ioNamePtr = str;
    dir.dirInfo.ioCompletion = NULL;
    dir.dirInfo.ioFDirIndex = 0;
    dir.dirInfo.ioVRefNum = vref;
    dir.dirInfo.ioDrDirID = 0;
    for (;;) {
	PBGetCatInfoSync(&dir);
	memcpy(buf -= str[0], str + 1, str[0]);
	if (dir.dirInfo.ioDrDirID == 2) {
	    return buf;
	}
	*--buf = ':';
	dir.dirInfo.ioFDirIndex = -1;
	dir.dirInfo.ioDrDirID = dir.dirInfo.ioDrParID;
    }
}

/*
 * NAME:	getfile()
 * DESCRIPTION:	get the path of a specific file with a standard dialog
 */
char *getfile(char *buf, long type)
{
    Point where;
    SFTypeList list;
    SFReply reply;

    where.h = 82;
    where.v = 124;
    list[0] = type;
    SFGetFile(where, NULL, NULL, 1, list, NULL, &reply);
    if (reply.good) {
	return getpath(buf, reply.vRefNum, reply.fName);
    } else {
	return NULL;
    }
}


/*
 * NAME:	path_native()
 * DESCRIPTION:	deal with path that's already native
 */
char *path_native(char *to, char *from)
{
    to[0] = '/';	/* mark as native */
    strncpy(to + 1, from, STRINGSZ - 1);
    to[STRINGSZ - 1] = '\0';
    return to;
}

/*
 * NAME:	path_file()
 * DESCRIPTION:	translate a path to a pascal string
 */
static unsigned char *path_file(unsigned char *to, const char *from)
{
    char *p, *q;
    int n;

    p = (char *) to + 1;
    q = (char *) from;
    if (*q == '/') {
	/* native path: copy directly */
	q++;
	while (*q != '\0') {
	    *p++ = *q++;
	}
	to[0] = (unsigned char *) p - to - 1;
	return to;
    }

    n = 0;
    *p++ = ':';
    if (*q == '.' && q[1] == '\0') {
	*p = '\0';
	to[0] = 1;
	return to;
    }
    n++;

    while (n < 255 && *q != '\0') {
	if (*q == '/') {
	    *p = ':';
	} else if (*q == ':') {
	    *p = '/';
	} else {
	    *p = *q;
	}
	p++;
	q++;
	n++;
    }
    to[0] = n;

    return to;
}

/*
 * NAME:	path_unfile()
 * DESCRIPTION:	translate a pascal filename to a path
 */
static char *path_unfile(char *to, const unsigned char *from)
{
    char *p, *q;
    int n;

    for (p = to, q = (char *) from + 1, n = from[0]; n != 0; p++, q++, --n) {
	if (*q == '/') {
	    *p = ':';
	} else {
	    *p = *q;
	}
    }
    *p = '\0';

    return to;
}


static long sdirid;		/* scan directory ID */
static short sdiridx;		/* scan directory index */
static CInfoPBRec sdirbuf;	/* scan dir file info */

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
bool P_opendir(char *path)
{
    CInfoPBRec buf;
    Str255 str;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = path_file(str, path);
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr ||
	(buf.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
	return FALSE;
    }
    sdirid = buf.hFileInfo.ioDirID;
    sdiridx = 1;
    return TRUE;
}

/*
 * NAME:	P->readdir()
 * DESCRIPTION:	read the next filename from the currently open directory
 */
char *P_readdir(void)
{
    Str255 str;
    static char path[34];

    sdirbuf.hFileInfo.ioVRefNum = vref;
    sdirbuf.hFileInfo.ioFDirIndex = sdiridx++;
    sdirbuf.hFileInfo.ioNamePtr = str;
    sdirbuf.hFileInfo.ioDirID = sdirid;
    if (PBGetCatInfoSync(&sdirbuf) != noErr) {
	return NULL;
    }

    return path_unfile(path, str);
}

/*
 * NAME:	P->closedir()
 * DESCRIPTION:	close the currently open directory
 */
void P_closedir(void)
{
    sdiridx = 0;
}


/*
 * NAME:	P->open()
 * DESCRIPTION:	open a file
 */
int P_open(char *path, int flags, int mode)
{
    int fd;
    short fref;

    for (fd = 0; fdtab[fd].fref != 0; fd++) {
	if (fd == sizeof(fdtab) / sizeof(short) - 1) {
	    return -1;
	}
    }

    switch (HOpen(vref, dirid, path_file(fdtab[fd].fname, path), fsRdWrShPerm,
		  &fref)) {
    case noErr:
	if ((flags & O_EXCL) ||
	    ((flags & O_TRUNC) && SetEOF(fref, 0L) != noErr) ||
	    ((flags & O_APPEND) && SetFPos(fref, fsFromLEOF, 0) != noErr)) {
	    FSClose(fref);
	    return -1;
	}
	break;

    case fnfErr:
    case dirNFErr:
	if ((flags & O_CREAT) &&
	    HCreate(vref, dirid, fdtab[fd].fname, crea, type) == noErr &&
	    HOpen(vref, dirid, fdtab[fd].fname, fsRdWrShPerm, &fref) == noErr) {
	    break;
	}
	/* fall through */

    default:
	return -1;
    }

    fdtab[fd].fref = fref;
    return fd;
}

/*
 * NAME:	P->close()
 * DESCRIPTION:	close a file
 */
int P_close(int fd)
{
    FSClose(fdtab[fd].fref);
    fdtab[fd].fref = 0;

    return 0;
}

/*
 * NAME:	P->read()
 * DESCRIPTION:	read from a file
 */
int P_read(int fd, char *buf, int nbytes)
{
    long count;

    count = nbytes;
    switch (FSRead(fdtab[fd].fref, &count, buf)) {
    case noErr:
    case eofErr:
	return (int) count;

    default:
	return -1;
    }
}

/*
 * NAME:	P->write()
 * DESCRIPTION:	write to a file
 */
int P_write(int fd, char *buf, int nbytes)
{
    long count;

    count = nbytes;
    switch (FSWrite(fdtab[fd].fref, &count, buf)) {
    case noErr:
    case dskFulErr:
	return (int) count;

    default:
	return -1;
    }
}

/*
 * NAME:	P->lseek()
 * DESCRIPTION:	seek on a file
 */
long P_lseek(int fd, long offset, int whence)
{
    short mode;

    switch (whence) {
    case SEEK_SET:
	mode = fsFromStart;
	break;

    case SEEK_CUR:
	mode = fsFromMark;
	break;

    case SEEK_END:
	mode = fsFromLEOF;
	break;
    }

    /*
     * note: no seek beyond the end of the file
     */
    if (SetFPos(fdtab[fd].fref, mode, offset) != noErr) {
	return -1;
    }
    if (mode != fsFromStart) {
	GetFPos(fdtab[fd].fref, &offset);
    }
    return offset;
}

/*
 * NAME:	P->stat()
 * DESCRIPTION:	get information about a file
 */
int P_stat(char *path, struct stat *sb)
{
    CInfoPBRec buf;
    Str255 str;

    if (sdiridx != 0) {
	buf = sdirbuf;
    } else {
	buf.hFileInfo.ioVRefNum = vref;
	buf.hFileInfo.ioFDirIndex = 0;
	buf.hFileInfo.ioNamePtr = path_file(str, path);
	buf.hFileInfo.ioDirID = dirid;
	if (PBGetCatInfoSync(&buf) != noErr) {
	    return -1;
	}
    }

    sb->st_mode = (buf.hFileInfo.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.hFileInfo.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.hFileInfo.ioFlMdDat);

    return 0;
}

/*
 * NAME:	P->fstat()
 * DESCRIPTION:	get information about an open file
 */
int P_fstat(int fd, struct stat *sb)
{
    CInfoPBRec buf;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = fdtab[fd].fname;
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr) {
	return -1;
    }

    sb->st_mode = (buf.hFileInfo.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.hFileInfo.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.hFileInfo.ioFlMdDat);

    return 0;
}

/*
 * NAME:	P->unlink()
 * DESCRIPTION:	remove a file (but not a directory)
 */
int P_unlink(char *path)
{
    CInfoPBRec buf;
    Str255 str;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = path_file(str, path);
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr ||
	(buf.hFileInfo.ioFlAttrib & ioDirMask)) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0 : -1;
}

/*
 * NAME:	P->rename()
 * DESCRIPTION:	rename a file
 */
int P_rename(char *from, char *to)
{
    char *p, *q;
    Str255 dir1, dir2, file1, file2;
    CInfoPBRec buf;
    long xdirid;

    p = strrchr(from, '/');
    if (p == NULL) {
	dir1[0] = 1;
	dir1[1] = '.';
	p = from;
    } else {
	*p++ = '\0';
	path_file(dir1, from);
    }
    path_file(file1, p);
    q = strrchr(to, '/');
    if (q == NULL) {
	dir2[0] = 1;
	dir2[1] = '.';
	q = to;
    } else {
	*q++ = '\0';
	path_file(dir2, to);
    }
    path_file(file2, q);

    /* source directory must exist */
    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = dir1;
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr ||
	(buf.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    xdirid = buf.hFileInfo.ioDirID;

    /* source file must exist */
    buf.hFileInfo.ioNamePtr = file1;
    if (PBGetCatInfoSync(&buf) != noErr) {
	return -1;
    }
    if (buf.hFileInfo.ioFlAttrib & ioDirMask) {
	file1[++(file1[0])] = ':';
	file2[++(file2[0])] = ':';
    }

    if (p - from != q - to || memcmp(from, to, p - from) != 0) {
	CMovePBRec move;

	/*
	 * move to different directory
	 */

	/* destination directory must exist */
	buf.hFileInfo.ioNamePtr = dir2;
	buf.hFileInfo.ioDirID = dirid;
	if (PBGetCatInfoSync(&buf) != noErr ||
	    (buf.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
	    return -1;
	}
	move.ioNewDirID = buf.hFileInfo.ioDirID;

	/* destination must not already exist */
	buf.hFileInfo.ioNamePtr = file2;
	if (PBGetCatInfoSync(&buf) == noErr) {
	    return -1;
	}

	/* rename source */
	memcpy(dir1, file1, file1[0] + 1);
	memcpy(file1, "\p:_tmp0000", 6);
	do {
	    static short count;

	    if (count == 9999) {
		count = 0;
	    }
	    sprintf((char *) file1 + 6, "%04d", ++count);
	    buf.hFileInfo.ioNamePtr = file1;
	    buf.hFileInfo.ioDirID = xdirid;
	} while (PBGetCatInfoSync(&buf) == noErr);
	if (dir1[dir1[0]] == ':') {
	    file1[++(file1[0])] = ':';
	}
	if (HRename(vref, xdirid, dir1, file1) != noErr) {
	    return -1;
	}

	/* move source to new directory */
	move.ioNamePtr = file1;
	move.ioVRefNum = vref;
	move.ioNewName = NULL;
	move.ioDirID = xdirid;
	if (PBCatMoveSync(&move) != noErr) {
	    /* back to old name */
	    HRename(vref, xdirid, file1, dir1);
	    return -1;
	}

	xdirid = move.ioNewDirID;
    }

    return (HRename(vref, xdirid, file1, file2) == noErr) ? 0 : -1;
}

/*
 * NAME:	P->access()
 * DESCRIPTION:	check access on a file
 */
int P_access(char *path, int mode)
{
    CInfoPBRec buf;
    Str255 str;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = path_file(str, path);
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr) {
	return -1;
    }

    if (mode == W_OK) {
	return (buf.hFileInfo.ioFlAttrib & 0x01) ? -1 : 0;
    }
    return 0;
}

/*
 * NAME:	P->mkdir()
 * DESCRIPTION:	create a directory
 */
int P_mkdir(char *path, int mode)
{
    Str255 str;
    long newdir;

    if (DirCreate(vref, dirid, path_file(str, path), &newdir) == noErr) {
	return 0;
    } else {
	return -1;
    }
}

/*
 * NAME:	P->rmdir()
 * DESCRIPTION:	remove an empty directory
 */
int P_rmdir(char *path)
{
    CInfoPBRec buf;
    Str255 str;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = path_file(str, path);
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr ||
	(buf.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0  : -1;
}

/*
 * NAME:	P->chdir()
 * DESCRIPTION:	change the current directory
 */
int P_chdir(char *path)
{
    CInfoPBRec buf;
    Str255 str;

    buf.hFileInfo.ioVRefNum = vref;
    buf.hFileInfo.ioFDirIndex = 0;
    buf.hFileInfo.ioNamePtr = path_file(str, path);
    buf.hFileInfo.ioDirID = dirid;
    if (PBGetCatInfoSync(&buf) != noErr ||
	(buf.hFileInfo.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    dirid = buf.hFileInfo.ioDirID;
    return 0;
}
