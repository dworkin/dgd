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

# define INCLUDE_FILE_IO
# include "dgd.h"
# include "hash.h"
# include "swap.h"

static char *swapfile;			/* swap file name */
static int swap;			/* swap file descriptor */
static int dump, dump2;			/* snapshot descriptors */
static char *mem;			/* swap slots in memory */
static Sector *map, *smap;		/* sector map, swap free map */
static Sector mfree, sfree;		/* free sector lists */
static char *cbuf;			/* sector buffer */
static Sector cached;			/* sector currently cached in cbuf */
static Swap::SwapSlot *first, *last;	/* first and last swap slot */
static Swap::SwapSlot *lfree;		/* free swap slot list */
static off_t slotsize;			/* sizeof(SwapSlot) + size of sector */
static unsigned int sectorsize;		/* size of sector */
static unsigned int restoresecsize;	/* size of sector in restore file */
static Sector swapsize, cachesize;	/* # of sectors in swap and cache */
static Sector nsectors;			/* total swap sectors */
static Sector nfree;			/* # free sectors */
static Sector ssectors;			/* sectors actually in swap file */
static Sector sbarrier;			/* swap sector barrier */
static bool swapping;			/* currently using a swapfile? */

/*
 * initialize the swap device
 */
void Swap::init(char *file, unsigned int total, unsigned int secsize)
{
    SwapSlot *h;
    Sector i;

    /* allocate and initialize all tables */
    swapfile = file;
    swapsize = total;
    cachesize = 128;
    sectorsize = secsize;
    slotsize = sizeof(SwapSlot) + secsize;
    mem = ALLOC(char, slotsize * cachesize);
    map = ALLOC(Sector, total);
    smap = ALLOC(Sector, total);
    cbuf = ALLOC(char, secsize);
    cached = SW_UNUSED;

    /* 0 sectors allocated */
    nsectors = 0;
    ssectors = 0;
    sbarrier = 0;
    nfree = 0;

    /* init free sector maps */
    mfree = SW_UNUSED;
    sfree = SW_UNUSED;
    lfree = h = (SwapSlot *) mem;
    for (i = cachesize - 1; i > 0; --i) {
	h->sec = SW_UNUSED;
	h->next = (SwapSlot *) ((char *) h + slotsize);
	h = h->next;
    }
    h->sec = SW_UNUSED;
    h->next = (SwapSlot *) NULL;

    /* no swap slots in use yet */
    first = (SwapSlot *) NULL;
    last = (SwapSlot *) NULL;

    swap = dump = -1;
    swapping = TRUE;
}

/*
 * clean up swapfile
 */
void Swap::finish()
{
    if (swap >= 0) {
	char buf[STRINGSZ];

	P_close(swap);
	P_unlink(path_native(buf, swapfile));
    }
    if (dump >= 0) {
	P_close(dump);
    }
}

/*
 * write possibly large items to the swap
 */
bool Swap::write(int fd, void *buffer, size_t size)
{
    while (size > SWAPCHUNK) {
	if (P_write(fd, (char *) buffer, SWAPCHUNK) != SWAPCHUNK) {
	    return FALSE;
	}
	buffer = (char *) buffer + SWAPCHUNK;
	size -= SWAPCHUNK;
    }
    return (P_write(fd, (char *) buffer, size) == size);
}

/*
 * create the swap file
 */
void Swap::create()
{
    char buf[STRINGSZ], *p;

    memset(cbuf, '\0', sectorsize);
    p = path_native(buf, swapfile);
    P_unlink(p);
    swap = P_open(p, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (swap < 0 || !write(swap, cbuf, sectorsize)) {
	EC->fatal("cannot create swap file \"%s\"", swapfile);
    }
}

/*
 * count the number of sectors required for size bytes + a map
 */
Sector Swap::mapsize(unsigned int size)
{
    Sector i, n;

    /* calculate the number of sectors required */
    n = 0;
    for (;;) {
	i = (size + n * sizeof(Sector) + sectorsize - 1) / sectorsize;
	if (n == i) {
	    return n;
	}
	n = i;
    }
}

/*
 * initialize a new vector of sectors
 */
void Swap::newv(Sector *vec, unsigned int size)
{
    while (mfree != SW_UNUSED) {
	/* reuse a previously deleted sector */
	if (size == 0) {
	    return;
	}
	mfree = map[*vec = mfree];
	map[*vec++] = SW_UNUSED;
	--nfree;
	--size;
    }

    while (size > 0) {
	/* allocate a new sector */
	if (nsectors == swapsize) {
	    EC->fatal("out of sectors");
	}
	map[*vec++ = nsectors++] = SW_UNUSED;
	--size;
    }
}

/*
 * wipe a vector of sectors
 */
void Swap::wipev(Sector *vec, unsigned int size)
{
    Sector sec, i;
    SwapSlot *h;

    vec += size;
    while (size > 0) {
	sec = *--vec;
	i = map[sec];
	if (i < cachesize &&
	    (h=(SwapSlot *) (mem + i * slotsize))->sec == sec) {
	    i = h->swap;
	    h->swap = SW_UNUSED;
	} else {
	    map[sec] = SW_UNUSED;
	}
	if (i != SW_UNUSED && i >= sbarrier) {
	    /*
	     * free sector in swap file
	     */
	    i -= sbarrier;
	    smap[i] = sfree;
	    sfree = i;
	}
	--size;
    }
}

/*
 * delete a vector of swap sectors
 */
void Swap::delv(Sector *vec, unsigned int size)
{
    Sector sec, i;
    SwapSlot *h;

    /*
     * note: sectors must have been wiped before being deleted!
     */
    vec += size;
    while (size > 0) {
	sec = *--vec;
	i = map[sec];
	if (i < cachesize &&
	    (h=(SwapSlot *) (mem + i * slotsize))->sec == sec) {
	    /*
	     * remove the swap slot from the first-last list
	     */
	    if (h != first) {
		h->prev->next = h->next;
	    } else {
		first = h->next;
		if (first != (SwapSlot *) NULL) {
		    first->prev = (SwapSlot *) NULL;
		}
	    }
	    if (h != last) {
		h->next->prev = h->prev;
	    } else {
		last = h->prev;
		if (last != (SwapSlot *) NULL) {
		    last->next = (SwapSlot *) NULL;
		}
	    }
	    /*
	     * put the cache slot in the free cache slot list
	     */
	    h->sec = SW_UNUSED;
	    h->next = lfree;
	    lfree = h;
	}

	/*
	 * put sec in free sector list
	 */
	map[sec] = mfree;
	mfree = sec;
	nfree++;

	--size;
    }
}

/*
 * allocate swapspace for something
 */
Sector Swap::alloc(Uint size, Sector nsectors, Sector **sectors)
{
    Sector n, *s;

    s = *sectors;
    if (nsectors != 0) {
	/* wipe old sectors */
	wipev(s, nsectors);
    }

    n = Swap::mapsize(size);
    if (nsectors > n) {
	/* too many sectors */
	delv(s + n, nsectors - n);
    }

    s = *sectors = REALLOC(*sectors, Sector, nsectors, n);
    if (nsectors < n) {
	/* not enough sectors */
	newv(s + nsectors, n - nsectors);
    }

    return n;
}

/*
 * reserve a swap slot for sector sec. If fill == TRUE, load it
 * from the swap file if appropriate.
 */
Swap::SwapSlot *Swap::load(Sector sec, bool restore, bool fill)
{
    SwapSlot *h;
    Sector load, save;

    load = map[sec];
    if (load >= cachesize ||
	(h=(SwapSlot *) (mem + load * slotsize))->sec != sec) {
	/*
	 * the sector is either unused or in the swap file
	 */
	if (lfree != (SwapSlot *) NULL) {
	    /*
	     * get swap slot from the free swap slot list
	     */
	    h = lfree;
	    lfree = h->next;
	} else {
	    /*
	     * No free slot available, use the last one in the swap slot list
	     * instead.
	     */
	    h = last;
	    last = h->prev;
	    if (last != (SwapSlot *) NULL) {
		last->next = (SwapSlot *) NULL;
	    } else {
		first = (SwapSlot *) NULL;
	    }
	    save = h->swap;
	    if (h->dirty) {
		/*
		 * Dump the sector to swap file
		 */

		if (save == SW_UNUSED || save < sbarrier) {
		    /*
		     * allocate new sector in swap file
		     */
		    if (sfree == SW_UNUSED) {
			if (ssectors == SW_UNUSED) {
			    EC->fatal("out of sectors");
			}
			save = ssectors++;
		    } else {
			save = sfree;
			sfree = smap[save];
			save += sbarrier;
		    }
		}

		if (swap < 0) {
		    create();
		}
		P_lseek(swap, (off_t) (save + 1L) * sectorsize, SEEK_SET);
		if (!write(swap, h + 1, sectorsize)) {
		    EC->fatal("cannot write swap file");
		}
	    }
	    map[h->sec] = save;
	}
	h->sec = sec;
	h->swap = load;
	h->dirty = FALSE;
	/*
	 * The slot has been reserved. Update map.
	 */
	map[sec] = ((intptr_t) h - (intptr_t) mem) / slotsize;

	if (load != SW_UNUSED) {
	    if (restore) {
		/*
		 * load the sector from the snapshot
		 */
		P_lseek(dump, (off_t) (load + 1L) * sectorsize, SEEK_SET);
		if (P_read(dump, (char *) (h + 1), sectorsize) <= 0) {
		    EC->fatal("cannot read snapshot");
		}
	    } else if (fill) {
		/*
		 * load the sector from the swap file
		 */
		P_lseek(swap, (off_t) (load + 1L) * sectorsize, SEEK_SET);
		if (P_read(swap, (char *) (h + 1), sectorsize) <= 0) {
		    EC->fatal("cannot read swap file");
		}
	    }
	} else if (fill) {
	    /* zero-fill new sector */
	    memset(h + 1, '\0', sectorsize);
	}
    } else {
	/*
	 * The sector already had a slot. Remove it from the first-last list.
	 */
	if (h != first) {
	    h->prev->next = h->next;
	} else {
	    first = h->next;
	}
	if (h != last) {
	    h->next->prev = h->prev;
	} else {
	    last = h->prev;
	    if (last != (SwapSlot *) NULL) {
		last->next = (SwapSlot *) NULL;
	    }
	}
    }
    /*
     * put the sector at the head of the first-last list
     */
    h->prev = (SwapSlot *) NULL;
    h->next = first;
    if (first != (SwapSlot *) NULL) {
	first->prev = h;
    } else {
	last = h;	/* last was NULL too */
    }
    first = h;

    return h;
}

/*
 * read bytes from a vector of sectors
 */
void Swap::readv(char *m, Sector *vec, Uint size, Uint idx)
{
    unsigned int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    do {
	len = (size > sectorsize - idx) ? sectorsize - idx : size;
	memcpy(m, (char *) (load(*vec++, FALSE, TRUE) + 1) + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * write bytes to a vector of sectors
 */
void Swap::writev(char *m, Sector *vec, Uint size, Uint idx)
{
    SwapSlot *h;
    unsigned int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    do {
	len = (size > sectorsize - idx) ? sectorsize - idx : size;
	h = load(*vec++, FALSE, (len != sectorsize));
	h->dirty = TRUE;
	memcpy((char *) (h + 1) + idx, m, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * restore bytes from a vector of sectors in snapshot
 */
void Swap::dreadv(char *m, Sector *vec, Uint size, Uint idx)
{
    SwapSlot *h;
    unsigned int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    do {
	len = (size > sectorsize - idx) ? sectorsize - idx : size;
	h = load(*vec++, TRUE, FALSE);
	h->swap = SW_UNUSED;
	memcpy(m, (char *) (h + 1) + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * restore converted bytes from a vector of sectors in snapshot
 */
void Swap::conv(char *m, Sector *vec, Uint size, Uint idx)
{
    unsigned int len;

    vec += idx / restoresecsize;
    idx %= restoresecsize;
    do {
	len = (size > restoresecsize - idx) ? restoresecsize - idx : size;
	if (*vec != cached) {
	    P_lseek(dump, (off_t) (map[*vec] + 1L) * restoresecsize, SEEK_SET);
	    if (P_read(dump, cbuf, restoresecsize) <= 0) {
		EC->fatal("cannot read snapshot");
	    }
	    map[cached = *vec] = SW_UNUSED;
	}
	vec++;
	memcpy(m, cbuf + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * restore bytes from a vector of sectors in secondary snapshot
 */
void Swap::conv2(char *m, Sector *vec, Uint size, Uint idx)
{
    unsigned int len;

    vec += idx / restoresecsize;
    idx %= restoresecsize;
    do {
	len = (size > restoresecsize - idx) ? restoresecsize - idx : size;
	if (*vec != cached) {
	    P_lseek(dump2, (off_t) (map[*vec] + 1L) * restoresecsize,
		    SEEK_SET);
	    if (P_read(dump2, cbuf, restoresecsize) <= 0) {
		EC->fatal("cannot read secondary snapshot");
	    }
	    map[cached = *vec] = SW_UNUSED;
	}
	vec++;
	memcpy(m, cbuf + idx, len);
	idx = 0;
	m += len;
    } while ((size -= len) > 0);
}

/*
 * convert something from the snapshot
 */
Uint Swap::convert(char *m, Sector *vec, const char *layout, Uint n, Uint idx,
		   void (*readv) (char*, Sector*, Uint, Uint))
{
    Uint bufsize;
    char *buf;

    bufsize = (Config::dsize(layout) & 0xff) * n;
    buf = ALLOC(char, bufsize);
    (*readv)(buf, vec, bufsize, idx);
    Config::dconv(m, buf, layout, n);
    FREE(buf);

    return bufsize;
}

/*
 * compress data
 */
Uint Swap::compress(char *data, char *text, Uint size)
{
    char htab[16384];
    unsigned short buf, bufsize, x;
    char *p, *q;
    Uint cspace;

    if (size <= 4 + 1) {
	/* can't get smaller than this */
	return 0;
    }

    /* clear the hash table */
    memset(htab, '\0', sizeof(htab));

    buf = bufsize = 0;
    x = 0;
    p = text;
    q = data;
    *q++ = size >> 24;
    *q++ = size >> 16;
    *q++ = size >> 8;
    *q++ = size;
    cspace = size - 4;

    while (size != 0) {
	if (htab[x] == *p) {
	    buf >>= 1;
	    bufsize += 1;
	} else {
	    htab[x] = *p;
	    buf = (buf >> 9) + 0x0080 + (UCHAR(*p) << 8);
	    bufsize += 9;
	}
	x = ((x << 3) & 0x3fff) ^ HM->hashchar(UCHAR(*p++));

	if (bufsize >= 8) {
	    if (bufsize == 16) {
		if ((Int) (cspace-=2) <= 0) {
		    return 0;	/* out of space */
		}
		*q++ = buf;
		*q++ = buf >> 8;
		bufsize = 0;
	    } else {
		if (--cspace == 0) {
		    return 0;	/* out of space */
		}
		*q++ = buf >> (16 - bufsize);
		bufsize -= 8;
	    }
	}

	--size;
    }
    if (bufsize != 0) {
	if (--cspace == 0) {
	    return 0;	/* compression did not reduce size */
	}
	/* add last incomplete byte */
	*q++ = (buf >> (16 - bufsize)) + (0xff << bufsize);
    }

    return (intptr_t) q - (intptr_t) data;
}

/*
 * read and decompress data from the swap file
 */
char *Swap::decompress(Sector *sectors,
		       void (*readv) (char*, Sector*, Uint, Uint),
		       Uint size, Uint offset, Uint *dsize)
{
    char buffer[8192], htab[16384];
    unsigned short buf, bufsize, x;
    Uint n;
    char *p, *q;

    buf = bufsize = 0;
    x = 0;

    /* clear the hash table */
    memset(htab, '\0', sizeof(htab));

    n = sizeof(buffer);
    if (n > size) {
	n = size;
    }
    (*readv)(p = buffer, sectors, n, offset);
    size -= n;
    offset += n;
    *dsize = (UCHAR(p[0]) << 24) | (UCHAR(p[1]) << 16) | (UCHAR(p[2]) << 8) |
	     UCHAR(p[3]);
    q = ALLOC(char, *dsize);
    p += 4;
    n -= 4;

    for (;;) {
	for (;;) {
	    if (bufsize == 0) {
		if (n == 0) {
		    break;
		}
		--n;
		buf = UCHAR(*p++);
		bufsize = 8;
	    }
	    if (buf & 1) {
		if (n == 0) {
		    break;
		}
		--n;
		buf += UCHAR(*p++) << bufsize;

		*q = htab[x] = buf >> 1;
		buf >>= 9;
	    } else {
		*q = htab[x];
		buf >>= 1;
	    }
	    --bufsize;

	    x = ((x << 3) & 0x3fff) ^ HM->hashchar(UCHAR(*q++));
	}
	if (size == 0) {
	    return q - *dsize;
	}
	n = sizeof(buffer);
	if (n > size) {
	    n = size;
	}
	(*readv)(p = buffer, sectors, n, offset);
	size -= n;
	offset += n;
    }
}

/*
 * return the number of sectors presently in use
 */
Sector Swap::count()
{
    return nsectors - nfree;
}


struct DumpHeader {
    Uint secsize;		/* size of swap sector */
    Sector nsectors;		/* # sectors */
    Sector ssectors;		/* # swap sectors */
    Sector nfree;		/* # free sectors */
    Sector mfree;		/* free sector list */
};

static char dh_layout[] = "idddd";

/*
 * create snapshot
 */
int Swap::save(char *snapshot, bool keep)
{
    SwapSlot *h;
    Sector sec;
    char buffer[STRINGSZ + 4], buf1[STRINGSZ], buf2[STRINGSZ], *p, *q;
    Sector n;

    if (swap < 0) {
	create();
    }

    /* flush the cache and adjust sector map */
    for (h = last; h != (SwapSlot *) NULL; h = h->prev) {
	sec = h->swap;
	if (h->dirty) {
	    /*
	     * Dump the sector to swap file
	     */
	    if (sec == SW_UNUSED || sec < sbarrier) {
		/*
		 * allocate new sector in swap file
		 */
		if (sfree == SW_UNUSED) {
		    if (ssectors == SW_UNUSED) {
			EC->fatal("out of sectors");
		    }
		    sec = ssectors++;
		} else {
		    sec = sfree;
		    sfree = smap[sec];
		    sec += sbarrier;
		}
		h->swap = sec;
	    }
	    P_lseek(swap, (off_t) (sec + 1L) * sectorsize, SEEK_SET);
	    if (!write(swap, h + 1, sectorsize)) {
		EC->fatal("cannot write swap file");
	    }
	}
	map[h->sec] = sec;
    }

    if (dump >= 0 && !keep) {
	P_close(dump);
	dump = -1;
    }
    if (swapping) {
	p = path_native(buf1, snapshot);
	snprintf(buffer, sizeof(buffer), "%s.old", snapshot);
	q = path_native(buf2, buffer);
	P_unlink(q);
	P_rename(p, q);

	/* move to snapshot */
	P_close(swap);
	q = path_native(buf2, swapfile);
	if (P_rename(q, p) < 0) {
	    int old;

	    /*
	     * The rename failed.  Attempt to copy the snapshot instead.
	     * This will take a long, long while, so keep the swapfile and
	     * snapshot on the same file system if at all possible.
	     */
	    old = P_open(q, O_RDWR | O_BINARY, 0);
	    swap = P_open(p, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
	    if (old < 0 || swap < 0) {
		EC->fatal("cannot move swap file");
	    }
	    /* copy initial sector */
	    if (P_read(old, cbuf, sectorsize) <= 0) {
		EC->fatal("cannot read swap file");
	    }
	    if (!write(swap, cbuf, sectorsize)) {
		EC->fatal("cannot write snapshot");
	    }
	    /* copy swap sectors */
	    for (n = ssectors; n > 0; --n) {
		if (P_read(old, cbuf, sectorsize) <= 0) {
		    EC->fatal("cannot read swap file");
		}
		if (!write(swap, cbuf, sectorsize)) {
		    EC->fatal("cannot write snapshot");
		}
	    }
	    P_close(old);
	} else {
	    /*
	     * The rename succeeded; reopen the new snapshot.
	     */
	    swap = P_open(p, O_RDWR | O_BINARY, 0);
	    if (swap < 0) {
		EC->fatal("cannot reopen snapshot");
	    }
	}
    }

    /* write map */
    P_lseek(swap, (off_t) (ssectors + 1L) * sectorsize, SEEK_SET);
    if (!write(swap, map, nsectors * sizeof(Sector))) {
	EC->fatal("cannot write sector map to snapshot");
    }

    /* fix the sector map */
    for (h = last; h != (SwapSlot *) NULL; h = h->prev) {
	map[h->sec] = ((intptr_t) h - (intptr_t) mem) / slotsize;
	h->dirty = FALSE;
    }

    return swap;
}

/*
 * finish snapshot
 */
void Swap::save2(SnapshotInfo *header, int size, bool incr)
{
    static off_t prev;
    off_t sectors;
    Uint offset;
    DumpHeader dh;
    char save[4];

    memset(cbuf, '\0', sectorsize);

    if (!swapping || incr) {
	/* extend */
	sectors = P_lseek(swap, 0, SEEK_CUR);
	offset = sectors % sectorsize;
	sectors /= sectorsize;
	if (offset != 0) {
	    if (!write(swap, cbuf, sectorsize - offset)) {
		EC->fatal("cannot extend swap file");
	    }
	    sectors++;
	}
    }

    if (swapping) {
	P_lseek(swap, 0, SEEK_SET);
	prev = 0;
    }

    /* write header */
    memcpy(cbuf, header, size);
    dh.secsize = sectorsize;
    dh.nsectors = nsectors;
    dh.ssectors = ssectors;
    dh.nfree = nfree;
    dh.mfree = mfree;
    memcpy(cbuf + sectorsize - sizeof(DumpHeader), &dh, sizeof(DumpHeader));
    if (!write(swap, cbuf, sectorsize)) {
	EC->fatal("cannot write snapshot header");
    }

    if (!swapping) {
	/* let the previous header refer to the current one */
	save[0] = sectors >> 24;
	save[1] = sectors >> 16;
	save[2] = sectors >> 8;
	save[3] = sectors;
	P_lseek(swap, prev * sectorsize + size - sizeof(save), SEEK_SET);
	if (!write(swap, save, sizeof(save))) {
	    EC->fatal("cannot write offset");
	}
	prev = sectors;
    }

    if (incr) {
	/* incremental snapshot */
	if (swapping) {
	    --sectors;
	}
	if (sectors > SW_UNUSED) {
	    sectors = SW_UNUSED;
	}
	sbarrier = ssectors = sectors;
	swapping = FALSE;
    } else {
	/* full snapshot */
	dump = swap;
	swap = -1;
	sbarrier = ssectors = 0;
	swapping = TRUE;
	restoresecsize = sectorsize;
    }
    sfree = SW_UNUSED;
    cached = SW_UNUSED;
}

/*
 * restore snapshot
 */
void Swap::restore(int fd, unsigned int secsize)
{
    DumpHeader dh;

    /* restore swap header */
    P_lseek(fd, -(off_t) (Config::dsize(dh_layout) & 0xff), SEEK_CUR);
    Config::dread(fd, (char *) &dh, dh_layout, (Uint) 1);
    if (dh.secsize != secsize) {
	EC->error("Wrong sector size (%d)", dh.secsize);
    }
    if (dh.nsectors > swapsize) {
	EC->error("Too many sectors in restore file (%d)", dh.nsectors);
    }
    restoresecsize = secsize;
    if (secsize > sectorsize) {
	cbuf = REALLOC(cbuf, char, 0, secsize);
    }

    /* seek beyond swap sectors */
    P_lseek(fd, (off_t) (dh.ssectors + 1L) * secsize, SEEK_SET);

    /* restore swap map */
    Config::dread(fd, (char *) map, "d", (Uint) dh.nsectors);
    nsectors = dh.nsectors;
    mfree = dh.mfree;
    nfree = dh.nfree;

    dump = fd;
}

/*
 * restore secondary snapshot
 */
void Swap::restore2(int fd)
{
    dump2 = fd;
}
