# define INCLUDE_FILE_IO
# include "dgd.h"
# include "swap.h"

typedef struct _header_ {	/* swap slot header */
    struct _header_ *prev;	/* previous in swap slot list */
    struct _header_ *next;	/* next in swap slot list */
    sector sec;			/* the sector that uses this slot */
    sector swap;		/* the swap sector (if any) */
    bool dirty;			/* has the swap slot been written to? */
} header;

static char *mem;			/* swap slots in memory */
static sector *map, *smap;		/* sector map, swap check map */
static sector mfree, sfree;		/* free sector lists */
static header *first, *last;		/* first and last swap slot */
static header *lfree;			/* free swap slot list */
static long slotsize;			/* sizeof(header) + size of sector */
static int sectorsize;			/* size of sector */
static uindex swapsize, cachesize;	/* # of sectors in swap and cache */
static uindex nsectors;			/* total swap sectors */
static uindex ssectors;			/* sectors actually in swap file */
static int swap;			/* swap file descriptor */

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
    swapsize = total;
    cachesize = cache;
    sectorsize = secsize;
    slotsize = sizeof(header) + secsize;
    mem = ALLOC(char, slotsize * cache);
    map = ALLOC(sector, total);
    smap = ALLOC(sector, total);

    /* create swap file */
    if (cache < total) {
	swap = open(file, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (swap < 0) {
	    fatal("cannot create swap file \"%s\"", file);
	}
    }

    /* 0 sectors allocated */
    nsectors = 0;
    ssectors = 0;

    /* init free sector maps */
    mfree = SW_UNUSED;
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
 * NAME:	swap->new()
 * DESCRIPTION:	return a newly created (empty) swap sector
 */
sector sw_new()
{
    register sector sec;

    if (mfree != SW_UNUSED) {
	/* reuse a previously deleted sector */
	sec = mfree;
	mfree = map[mfree];
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
    if (i < cachesize && (h = (header *) (mem + i * slotsize))->sec == sec) {
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
	if (h->swap != SW_UNUSED) {
	    smap[h->swap] = sfree;
	    sfree = h->swap;
	}
    } else if (i != SW_UNUSED) {
	/*
	 * free sector in swap file
	 */
	smap[i] = sfree;
	sfree = i;
    }

    /*
     * put sec in free sector list
     */
    map[sec] = mfree;
    mfree = sec;
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
	(h = (header *) (mem + load * slotsize))->sec != sec) {
	/*
	 * the sector is either unused or in the swap file
	 */
	if (lfree != (header *) NULL) {
	    /*
	     * get a swap slot from the free swap slot list
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
		lseek(swap, save * (long) sectorsize, SEEK_SET);
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

	if (load != SW_UNUSED && fill) {
	    /*
	     * load the sector from the swap file
	     */
	    lseek(swap, load * (long) sectorsize, SEEK_SET);
	    if (read(swap, (char *) (h + 1), sectorsize) < 0) {
		fatal("cannot read swap file");
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

# if 0
void sw_dump()
{
    register header *l;
    register sector s;

    printf("in cache:");
    for (l = first; l != (header *) NULL; l = l->next) {
	printf(" (%d %d %d)", l->sec, map[l->sec], l->swap);
    }
    putchar('\n');

    printf("sectors in free list:");
    for (s = mfree; s != SW_UNUSED; s = map[s]) {
	printf(" %2u", s);
    }
    putchar('\n');

    printf("map:");
    for (s = 0; s < nsectors; s++) {
	printf(" %2u", map[s]);
    }
    putchar('\n');

    printf("free slots in swap file:");
    for (s = sfree; s != SW_UNUSED; s = smap[s]) {
	printf(" %2u", s);
    }
    putchar('\n');
}
# endif
