# define INCLUDE_FILE_IO
# include "dgd.h"
# include "swap.h"

# define BSET(map, bit)		(map[(bit) >> 3] |= (1 << ((bit) & 7)))
# define BCLR(map, bit)		(map[(bit) >> 3] &= ~(1 << ((bit) & 7)))
# define BTST(map, bit)		(map[(bit) >> 3] & (1 << ((bit) & 7)))

typedef struct _header_ {	/* swap slot header */
    struct _header_ *prev;	/* previous in swap slot list */
    struct _header_ *next;	/* next in swap slot list */
    sector sec;			/* the sector that uses this slot */
    sector swap;		/* the swap sector (if any) */
    bool dirty;			/* has the swap slot been written to? */
} header;

static char *swapfile;			/* swap file name */
static int swap = -1;			/* swap file descriptor */
static int dump = -1;			/* dump file descriptor */
static char *mem;			/* swap slots in memory */
static sector *map, *smap;		/* sector map, swap check map */
static sector nfree, sfree;		/* free sector lists */
static char *bmap;			/* sector bitmap */
static char *cbuf;			/* sector buffer */
static header *first, *last;		/* first and last swap slot */
static header *lfree;			/* free swap slot list */
static long slotsize;			/* sizeof(header) + size of sector */
static int sectorsize;			/* size of sector */
static uindex swapsize, cachesize;	/* # of sectors in swap and cache */
static uindex nsectors;			/* total swap sectors */
static uindex ssectors;			/* sectors actually in swap file */
static uindex dsectors, dcursec;	/* dump sectors */

/*
 * NAME:	swap->init()
 * DESCRIPTION:	initialize the swap device
 */
void sw_init(file, total, cache, secsize)
char *file;
register uindex total, cache;
uindex secsize;
{
    register header *h;
    register sector i;

    /* allocate and initialize all tables */
    swapfile = file;
    swapsize = total;
    cachesize = cache;
    sectorsize = secsize;
    slotsize = sizeof(header) + secsize;
    mem = ALLOC(char, slotsize * cache);
    map = ALLOC(sector, total);
    smap = ALLOC(sector, total);
    bmap = ALLOC(char, (total + 7) >> 3);
    cbuf = ALLOC(char, secsize);

    /* 0 sectors allocated */
    nsectors = 0;
    ssectors = 0;

    /* init free sector maps */
    nfree = SW_UNUSED;
    sfree = SW_UNUSED;
    lfree = h = (header *) mem;
    for (i = cache - 1; i > 0; --i) {
	h->sec = SW_UNUSED;
	h->next = (header *) ((char *) h + slotsize);
	h = h->next;
    }
    h->sec = SW_UNUSED;
    h->next = (header *) NULL;

    /* no swap slots in use yet */
    first = (header *) NULL;
    last = (header *) NULL;
}

/*
 * NAME:	swap->finish()
 * DESCRIPTION:	clean up swapfile
 */
void sw_finish()
{
    if (swap >= 0) {
	close(swap);
	unlink(swapfile);
    }
    if (dump >= 0) {
	close(dump);
    }
}

/*
 * NAME:	create()
 * DESCRIPTION:	create the swap file
 */
static void sw_create()
{
    cbuf[0] = 0;
    swap = open(swapfile, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (swap < 0 || write(swap, cbuf, sectorsize) < 0) {
	fatal("cannot create swap file \"%s\"", swapfile);
    }
}

/*
 * NAME:	copy()
 * DESCRIPTION:	copy sectors from dump to swap
 */
static void copy(sec, n)
register sector sec;
register uindex n;
{
    dsectors -= n;

    if (swap < 0) {
	sw_create();
    }

    while (n > 0) {
	while (!BTST(bmap, dcursec)) {
	    dcursec++;
	}
	lseek(dump, (map[dcursec] + 1L) * sectorsize, SEEK_SET);
	if (read(dump, cbuf, sectorsize) <= 0) {
	    fatal("cannot read dump file");
	}
	lseek(swap, (sec + 1L) * sectorsize, SEEK_SET);
	if (write(swap, cbuf, sectorsize) < 0) {
	    fatal("cannot write swap file");
	}

	BCLR(bmap, dcursec);
	map[dcursec++] = sec++;
	--n;
    }
}

/*
 * NAME:	swap->new()
 * DESCRIPTION:	return a newly created (empty) swap sector
 */
sector sw_new()
{
    register sector sec;

    if (nfree != SW_UNUSED) {
	/* reuse a previously deleted sector */
	sec = nfree;
	nfree = map[nfree];
    } else {
	/* allocate a new sector */
	if (nsectors == swapsize) {
	    fatal("out of sectors");
	}
	sec = nsectors++;
    }
    map[sec] = SW_UNUSED;	/* nothing in it yet */

    return sec;
}

/*
 * NAME:	swap->del()
 * DESCRIPTION:	delete a swap sector
 */
void sw_del(sec)
sector sec;
{
    register sector i;
    register header *h;

    i = map[sec];
    if (i < cachesize && (h=(header *) (mem + i * slotsize))->sec == sec) {
	/*
	 * remove the swap slot from the first-last list
	 */
	if (h != first) {
	    h->prev->next = h->next;
	} else {
	    first = h->next;
	    if (first != (header *) NULL) {
		first->prev = (header *) NULL;
	    }
	}
	if (h != last) {
	    h->next->prev = h->prev;
	} else {
	    last = h->prev;
	    if (last != (header *) NULL) {
		last->next = (header *) NULL;
	    }
	}
	/*
	 * put the cache slot in the free cache slot list
	 */
	h->sec = SW_UNUSED;
	h->next = lfree;
	lfree = h;
	/*
	 * free sector in swap file (if any)
	 */
	i = h->swap;
    }
    if (i != SW_UNUSED) {
	if (dsectors > 0) {
	    if (BTST(bmap, sec)) {
		/*
		 * free sector in dump file
		 */
		BCLR(bmap, sec);
		--dsectors;
	    } else {
		/*
		 * replace by sector from dump file
		 */
		copy(i, (uindex) 1);
		if (dsectors == 0) {
		    close(dump);
		    dump = -1;
		}
	    }
	} else {
	    /*
	     * free sector in swap file
	     */
	    smap[i] = sfree;
	    sfree = i;
	}
    }

    /*
     * put sec in free sector list
     */
    map[sec] = nfree;
    nfree = sec;
}

/*
 * NAME:	swap->load()
 * DESCRIPTION:	reserve a swap slot for sector sec. If fill == TRUE, load it
 *		from the swap file if appropriate.
 */
static header *sw_load(sec, fill)
sector sec;
bool fill;
{
    register header *h;
    register sector load, save;

    load = map[sec];
    if (load >= cachesize ||
	(h=(header *) (mem + load * slotsize))->sec != sec) {
	/*
	 * the sector is either unused or in the swap file
	 */
	if (lfree != (header *) NULL) {
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
	    if (last != (header *) NULL) {
		last->next = (header *) NULL;
	    }
	    save = h->swap;
	    if (h->dirty) {
		/*
		 * Dump the sector to swap file
		 */

		if (save == SW_UNUSED) {
		    /*
		     * allocate new sector in swap file
		     */
		    if (sfree == SW_UNUSED) {
			save = ssectors++;
		    } else {
			save = sfree;
			sfree = smap[save];
		    }
		}

		if (swap < 0) {
		    sw_create();
		}
		lseek(swap, (save + 1L) * sectorsize, SEEK_SET);
		if (write(swap, (char *) (h + 1), sectorsize) < 0) {
		    fatal("cannot write swap file");
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
	map[sec] = ((long) h - (long) mem) / slotsize;

	if (load != SW_UNUSED) {
	    if (dsectors > 0 && BTST(bmap, sec)) {
		if (fill) {
		    /*
		     * load the sector from the dump file
		     */
		    lseek(dump, (load + 1L) * sectorsize, SEEK_SET);
		    if (read(dump, (char *) (h + 1), sectorsize) <= 0) {
			fatal("cannot read dump file");
		    }
		}

		BCLR(bmap, sec);
		if (--dsectors == 0) {
		    close(dump);
		    dump = -1;
		}
		h->swap = SW_UNUSED;
		h->dirty = TRUE;
	    } else if (fill) {
		/*
		 * load the sector from the swap file
		 */
		lseek(swap, (load + 1L) * sectorsize, SEEK_SET);
		if (read(swap, (char *) (h + 1), sectorsize) <= 0) {
		    fatal("cannot read swap file");
		}
	    }
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
	    if (last != (header *) NULL) {
		last->next = (header *) NULL;
	    }
	}
    }
    /*
     * put the sector at the head of the first-last list
     */
    h->prev = (header *) NULL;
    h->next = first;
    if (first != (header *) NULL) {
	first->prev = h;
    } else {
	last = h;	/* last was NULL too */
    }
    first = h;

    return h;
}

/*
 * NAME:	swap->readv()
 * DESCRIPTION:	read bytes from a vector of sectors
 */
void sw_readv(m, vec, size, idx)
register char *m;
register sector *vec;
register long size, idx;
{
    register int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    len = (size > sectorsize - idx) ? sectorsize - idx : size;
    memcpy(m, (char *) (sw_load(*vec++, TRUE) + 1) + idx, len);

    while ((size -= len) > 0) {
	m += len;
	len = (size > sectorsize) ? sectorsize : size;
	memcpy(m, (char *) (sw_load(*vec++, TRUE) + 1), len);
    }
}

/*
 * NAME:	swap->writev()
 * DESCRIPTION:	write bytes to a vector of sectors
 */
void sw_writev(m, vec, size, idx)
register char *m;
register sector *vec;
register long size, idx;
{
    register header *h;
    register int len;

    vec += idx / sectorsize;
    idx %= sectorsize;
    len = (size > sectorsize - idx) ? sectorsize - idx : size;
    h = sw_load(*vec++, (len != sectorsize));
    h->dirty = TRUE;
    memcpy((char *) (h + 1) + idx, m, len);

    while ((size -= len) > 0) {
	m += len;
	len = (size > sectorsize) ? sectorsize : size;
	h = sw_load(*vec++, (len != sectorsize));
	h->dirty = TRUE;
	memcpy((char *) (h + 1), m, len);
    }
}

/*
 * NAME:	swap->mapsize()
 * DESCRIPTION:	count the number of sectors required for size bytes + a map
 */
uindex sw_mapsize(size)
long size;
{
    register uindex i, n;

    /* calculate the number of sectors required */
    n = 0;
    for (;;) {
	i = (size + n * sizeof(sector) + sectorsize - 1) / sectorsize;
	if (n == i) {
	    return n;
	}
	n = i;
    }
}

/*
 * NAME:	swap->count()
 * DESCRIPTION:	return the number of sectors presently in use (or in free list)
 */
uindex sw_count()
{
    return nsectors;
}


typedef struct {
    uindex sectorsize;		/* size of swap sector */
    uindex nsectors;		/* # sectors */
    uindex ssectors;		/* # swap sectors */
    uindex nfree, sfree;	/* free sector lists */
} dump_header;

# define CHUNKSZ	32

/*
 * NAME:	swap->copy()
 * DESCRIPTION:	copy sectors from dumpfile to swapfile
 */
void sw_copy()
{
    register uindex n;

    if (dsectors > 0) {
	n = CHUNKSZ;
	if (n > dsectors) {
	    n = dsectors;
	}
	copy(ssectors, n);
	ssectors += n;
	if (dsectors == 0) {
	    close(dump);
	    dump = -1;
	}
    }
}

/*
 * NAME:	swap->dump()
 * DESCRIPTION:	dump swap file
 */
int sw_dump(dumpfile)
char *dumpfile;
{
    register header *h;
    register sector sec;
    char buffer[STRINGSZ + 4];
    uindex n;
    dump_header dh;

    if (dsectors > 0) {
	/* copy remaining dump sectors */
	n = dsectors;
	copy(ssectors, n);
	ssectors += n;
	close(dump);
    }
    sprintf(buffer, "%s.old", dumpfile);
    unlink(buffer);
    rename(dumpfile, buffer);
    if (swap < 0) {
	sw_create();
    }

    /* flush the cache */
    for (h = last; h != (header *) NULL; h = h->prev) {
	sec = h->swap;
	if (h->dirty) {
	    /*
	     * Dump the sector to swap file
	     */
	    if (sec == SW_UNUSED) {
		/*
		 * allocate new sector in swap file
		 */
		if (sfree == SW_UNUSED) {
		    sec = ssectors++;
		} else {
		    sec = sfree;
		    sfree = smap[sec];
		}
		h->swap = sec;
	    }
	    lseek(swap, (sec + 1L) * sectorsize, SEEK_SET);
	    if (write(swap, (char *) (h + 1), sectorsize) < 0) {
		fatal("cannot write swap file");
	    }
	}
    }

    /* move to dumpfile */
    close(swap);
    swap = -1;
    if (rename(swapfile, dumpfile) < 0) {
	fatal("cannot move swap file");
    }
    dump = open(dumpfile, O_RDWR | O_BINARY);
    if (dump < 0) {
	fatal("cannot reopen dump file");
    }

    /* write header */
    dh.sectorsize = sectorsize;
    dh.nsectors = nsectors;
    dh.ssectors = ssectors;
    dh.nfree = nfree;
    dh.sfree = sfree;
    lseek(dump, sectorsize - (long) sizeof(dump_header), SEEK_SET);
    if (write(dump, &dh, sizeof(dump_header)) < 0) {
	fatal("cannot write swap header to dump file");
    }

    /* adjust the sector map */
    for (h = last; h != (header *) NULL; h = h->prev) {
	map[h->sec] = h->swap;
    }

    /* write maps */
    lseek(dump, (ssectors + 1L) * sectorsize, SEEK_SET);
    if (write(dump, map, nsectors * sizeof(sector)) < 0 ||
	write(dump, smap, ssectors * sizeof(sector)) < 0) {
	fatal("cannot write swap maps to dump file");
    }

    /* create bitmap */
    dsectors = nsectors;
    dcursec = 0;
    memset(bmap, -1, (dsectors + 7) >> 3);
    for (sec = nfree; sec != SW_UNUSED; sec = map[sec]) {
	BCLR(bmap, sec);
	--dsectors;
    }

    /* fix the sector map and bitmap */
    for (h = last; h != (header *) NULL; h = h->prev) {
	map[h->sec] = ((long) h - (long) mem) / slotsize;
	BCLR(bmap, h->sec);
	h->swap = SW_UNUSED;
	h->dirty = TRUE;
	--dsectors;
    }

    ssectors = 0;
    sfree = SW_UNUSED;
    return dump;
}

/*
 * NAME:	swap->restore()
 * DESCRIPTION:	restore swap file
 */
void sw_restore(fd, secsize)
int fd, secsize;
{
    register uindex n, size;
    dump_header dh;
    char *buffer;

    /* restore swap header */
    lseek(fd, secsize - (long) sizeof(dump_header), SEEK_SET);
    if (read(fd, &dh, sizeof(dump_header)) != sizeof(dump_header) ||
	dh.sectorsize != sectorsize || dh.nsectors > swapsize) {
	fatal("bad swap header in restore file");
    }
    nsectors = dh.nsectors;
    ssectors = dh.ssectors;
    nfree = dh.nfree;
    sfree = dh.sfree;

    /* create swap file */
    sw_create();

    /* restore swapfile */
    buffer = ALLOCA(char, CHUNKSZ * sectorsize);
    for (n = ssectors; n > 0; n -= size) {
	size = (n >= CHUNKSZ) ? CHUNKSZ : n;
	if (read(fd, buffer, size * sectorsize) != size * sectorsize) {
	    fatal("cannot restore swap sectors");
	}
	if (write(swap, buffer, size * sectorsize) < 0) {
	    fatal("cannot write to swap file");
	}
    }
    AFREE(buffer);

    /* restore swap maps */
    if (read(fd, map, nsectors * sizeof(sector)) != nsectors * sizeof(sector) ||
	read(fd, smap, ssectors * sizeof(sector)) != ssectors * sizeof(sector))
    {
	fatal("cannot restore swap maps");
    }
}
