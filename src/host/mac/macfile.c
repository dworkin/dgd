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
 * NAME:	path_file()
 * DESCRIPTION:	convert a path to host format
 */
char *path_file(char *path)
{
    static char buf[STRINGSZ];
    char *p;

    if (path == (char *) NULL || strlen(path) >= STRINGSZ) {
	return (char *) NULL;
    }
    buf[0] = ':';
    if (path[0] == '.' && path[1] == '\0') {
	buf[1] = '\0';
	return buf;
    }
    strncpy(buf + 1, path, STRINGSZ - 1);
    buf[STRINGSZ - 1] = '\0';

    for (p = buf + 1; *p != '\0'; p++) {
	if (*p == '/') {
	    *p = ':';
	} else if (*p == ':') {
	    *p = '/';
	}
    }

    return buf;
}

/*
 * NAME:	path_unfile()
 * DESCRIPTION:	convert a path from host format
 */
char *path_unfile(char *path)
{
    static char buf[STRINGSZ];
    char *p;

    /* must start with : */
    if (path[1] == '\0') {
	buf[0] = '.';
	buf[1] = '\0';
	return buf;
    }
    strncpy(buf, path + 1, STRINGSZ - 1);
    buf[STRINGSZ - 1] = '\0';

    for (p = buf; *p != '\0'; p++) {
	if (*p == '/') {
	    *p = ':';
	} else if (*p == ':') {
	    *p = '/';
	}
    }

    return buf;
}


/*
 * NAME:	getpath()
 * DESCRIPTION:	get the full path of a file
 */
char *getpath(char *buf, short vref, unsigned char *fname)
{
    Str255 str;
    DirInfo dir;
    VolumeParam vol;

    buf += STRINGSZ - 1;
    buf[0] = '\0';
    memcpy(str, fname, fname[0] + 1);
    memcpy(buf -= fname[0], fname + 1, fname[0]);

    dir.ioNamePtr = str;
    dir.ioCompletion = NULL;
    dir.ioFDirIndex = 0;
    dir.ioVRefNum = vref;
    dir.ioDrDirID = 0;
    for (;;) {
	PBGetCatInfoSync((CInfoPBPtr) &dir);
	memcpy(buf -= str[0], str + 1, str[0]);
	if (dir.ioDrDirID == 2) {
	    return buf;
	}
	dir.ioFDirIndex = -1;
	dir.ioDrDirID = dir.ioDrParID;
	*--buf = ':';
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
 * NAME:	filename()
 * DESCRIPTION:	translate a path to a pascal string
 */
static unsigned char *filename(unsigned char *to, const char *from)
{
    int n;

    n = strlen(from);
    to[0] = n;
    memcpy(to + 1, from, n);

    return to;
}

/*
 * NAME:	pathname()
 * DESCRIPTION:	translate a pascal string to a path
 */
static char *pathname(char *to, const unsigned char *from)
{
    memcpy(to, from, from[0] + 1);
    to[0] = ':';
    to[from[0] + 1] = '\0';

    return to;
}


static long sdirid;		/* scan directory ID */
static short sdiridx;		/* scan directory index */
static HFileInfo sdirbuf;	/* scan dir file info */

/*
 * NAME:	P->opendir()
 * DESCRIPTION:	open a directory
 */
bool P_opendir(char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = filename(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return FALSE;
    }
    sdirid = buf.ioDirID;
    sdiridx = 1;
    return TRUE;
}

/*
 * NAME:	P->readdir()
 * DESCRIPTION:	read the next filename from the currently open directory
 */
char *P_readdir(void)
{
    HFileInfo buf;
    Str255 str;
    static char path[34];

    sdirbuf.ioVRefNum = vref;
    sdirbuf.ioFDirIndex = sdiridx++;
    sdirbuf.ioNamePtr = str;
    sdirbuf.ioDirID = sdirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &sdirbuf) != noErr) {
	return NULL;
    }

    return pathname(path, str);
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
 * NAME:	open()
 * DESCRIPTION:	open a file
 */
int open(const char *path, int flags, int mode)
{
    int fd;
    short fref;

    for (fd = 0; fdtab[fd].fref != 0; fd++) {
	if (fd == sizeof(fdtab) / sizeof(short) - 1) {
	    return -1;
	}
    }

    switch (HOpen(vref, dirid, filename(fdtab[fd].fname, path), fsRdWrShPerm,
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
 * NAME:	close()
 * DESCRIPTION:	close a file
 */
int close(int fd)
{
    FSClose(fdtab[fd].fref);
    fdtab[fd].fref = 0;

    return 0;
}

/*
 * NAME:	read()
 * DESCRIPTION:	read from a file
 */
int read(int fd, void *buf, int nbytes)
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
 * NAME:	write()
 * DESCRIPTION:	write to a file
 */
int write(int fd, const void *buf, int nbytes)
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
 * NAME:	lseek()
 * DESCRIPTION:	seek on a file
 */
long lseek(int fd, long offset, int whence)
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
 * NAME:	stat()
 * DESCRIPTION:	get information about a file
 */
int stat(const char *path, struct stat *sb)
{
    HFileInfo buf;
    Str255 str;

    if (sdiridx != 0) {
	buf = sdirbuf;
    } else {
	buf.ioVRefNum = vref;
	buf.ioFDirIndex = 0;
	buf.ioNamePtr = filename(str, path);
	buf.ioDirID = dirid;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	    return -1;
	}
    }

    sb->st_mode = (buf.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.ioFlMdDat);

    return 0;
}

/*
 * NAME:	fstat()
 * DESCRIPTION:	get information about an open file
 */
int fstat(int fd, struct stat *sb)
{
    HFileInfo buf;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = fdtab[fd].fname;
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }

    sb->st_mode = (buf.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
    sb->st_size = buf.ioFlLgLen;
    sb->st_mtime = (long) m2utime(buf.ioFlMdDat);

    return 0;
}

/*
 * NAME:	unlink()
 * DESCRIPTION:	remove a file (but not a directory)
 */
int unlink(const char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = filename(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask)) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0 : -1;
}

/*
 * NAME:	rename()
 * DESCRIPTION:	rename a file
 */
int rename(const char *from, const char *to)
{
    char *p, *q;
    Str255 dir1, dir2, file1, file2;
    HFileInfo buf;
    long xdirid;

    p = strrchr(from, ':');
    q = strrchr(to, ':');
    if (p == NULL || q == NULL) {
	return -1;
    }
    memcpy(dir1 + 1, from, dir1[0] = p - from);
    if (dir1[0] == 0) {
	dir1[++(dir1[0])] = ':';
    }
    filename(file1, p);
    memcpy(dir2 + 1, to, dir2[0] = q - to);
    if (dir2[0] == 0) {
	dir2[++(dir2[0])] = ':';
    }
    filename(file2, q);

    /* source directory must exist */
    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = dir1;
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    xdirid = buf.ioDirID;

    /* source file must exist */
    buf.ioNamePtr = file1;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }
    if (buf.ioFlAttrib & ioDirMask) {
	file1[++(file1[0])] = ':';
	file2[++(file2[0])] = ':';
    }

    if (p - from != q - to || memcmp(from, to, p - from) != 0) {
	CMovePBRec move;

	/*
	 * move to different directory
	 */

	/* destination directory must exist */
	buf.ioNamePtr = dir2;
	buf.ioDirID = dirid;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	    (buf.ioFlAttrib & ioDirMask) == 0) {
	    return -1;
	}
	move.ioNewDirID = buf.ioDirID;

	/* destination must not already exist */
	buf.ioNamePtr = file2;
	if (PBGetCatInfoSync((CInfoPBPtr) &buf) == noErr) {
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
	    buf.ioNamePtr = file1;
	    buf.ioDirID = xdirid;
	} while (PBGetCatInfoSync((CInfoPBPtr) &buf) == noErr);
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
 * NAME:	access()
 * DESCRIPTION:	check access on a file
 */
int access(const char *path, int mode)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = filename(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr) {
	return -1;
    }

    if (mode == W_OK) {
	return (buf.ioFlAttrib & 0x01) ? -1 : 0;
    }
    return 0;
}

/*
 * NAME:	mkdir()
 * DESCRIPTION:	create a directory
 */
int mkdir(const char *path, int mode)
{
    Str255 str;
    long newdir;

    if (DirCreate(vref, dirid, filename(str, path), &newdir) == noErr) {
	return 0;
    } else {
	return -1;
    }
}

/*
 * NAME:	rmdir()
 * DESCRIPTION:	remove an empty directory
 */
int rmdir(const char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = filename(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    return (HDelete(vref, dirid, str) == noErr) ? 0  : -1;
}

/*
 * NAME:	chdir()
 * DESCRIPTION:	change the current directory
 */
int chdir(const char *path)
{
    HFileInfo buf;
    Str255 str;

    buf.ioVRefNum = vref;
    buf.ioFDirIndex = 0;
    buf.ioNamePtr = filename(str, path);
    buf.ioDirID = dirid;
    if (PBGetCatInfoSync((CInfoPBPtr) &buf) != noErr ||
	(buf.ioFlAttrib & ioDirMask) == 0) {
	return -1;
    }
    dirid = buf.ioDirID;
    return 0;
}
