# include "dgd.h"

# define MAGIC_MASK	0xc0000000L	/* magic number mask */
# define SIZE_MASK	0x3fffffffL	/* size mask */

# define SM_MAGIC	0x80000000L	/* static mem */
# define DM_MAGIC	0xc0000000L	/* dynamic mem */

# define UINTSIZE	ALGN(sizeof(Uint), STRUCT_AL)
# define SIZETSIZE	ALGN(sizeof(size_t), STRUCT_AL)

# ifdef DEBUG
# define MOFFSET	ALGN(sizeof(header), STRUCT_AL)
# else
# define MOFFSET	UINTSIZE
# endif

typedef struct _chunk_ {
    Uint size;			/* size of chunk */
    struct _chunk_ *next;	/* next chunk */
} chunk;

# ifdef DEBUG
typedef struct _header_ {
    Uint size;			/* size of chunk */
    struct _header_ *prev;	/* previous in list */
    struct _header_ *next;	/* next in list */
    char *file;			/* file it was allocated from */
    int line;			/* line it was allocated from */
} header;
# endif


static allocinfo mstat;		/* memory statistics */

/*
 * NAME:	newmem()
 * DESCRIPTION:	allocate new memory
 */
static char *newmem(size, list)
size_t size;
char **list;
{
    char *mem;

    if (list != (char **) NULL) {
	size += ALGN(sizeof(char *), STRUCT_AL);
    }
    mem = (char *) malloc(size);
    if (mem == (char *) NULL) {
	fatal("out of memory");
    }
    if (list != (char **) NULL) {
	*((char **) mem) = *list;
	*list = mem;
	mem += ALGN(sizeof(char *), STRUCT_AL);
    }
    return mem;
}

/*
 * static memory manager
 */

# define INIT_MEM	15360		/* initial static memory chunk */
# define SLIMIT		(STRINGSZ + MOFFSET)
# define SSMALL		(MOFFSET + STRINGSZ / 8)
# define SCHUNKS	(STRINGSZ / STRUCT_AL - 1)
# define LCHUNKS	32

typedef struct {
    size_t size;		/* size of chunks in list */
    chunk *list;		/* list of chunks (possibly empty) */
} clist;

static char *slist;			/* list of static chunks */
static chunk *schunk;			/* current chunk */
static size_t schunksz;			/* size of current chunk */
static chunk *schunks[SCHUNKS];		/* lists of small free chunks */
static clist lchunks[LCHUNKS];		/* lists of large free chunks */
static unsigned int nlc;		/* # elements in large chunk list */
static chunk *sflist;			/* list of small unused chunks */
static int slevel;			/* static level */
static bool dmem;			/* any dynamic memory allocated? */

/*
 * NAME:	lchunk()
 * DESCRIPTION:	get the address of a list of large chunks
 */
static chunk **lchunk(size, new)
register size_t size;
bool new;
{
    register unsigned int h, l, m;

    l = m = 0;
    h = nlc;
    while (l < h) {
	m = (l + h) >> 1;
	if (lchunks[m].size == size) {
	    return &lchunks[m].list;	/* found */
	} else if (lchunks[m].size > size) {
	    h = m;			/* search in lower half */
	} else {
	    l = ++m;			/* search in upper half */
	}
    }

    if (!new) {
	return (chunk **) NULL;		/* not found */
    }
    /* make a new list */
    if (nlc == LCHUNKS) {
	fatal("too many different large static chunks");
    }
    for (l = nlc++; l > m; --l) {
	lchunks[l] = lchunks[l - 1];
    }
    lchunks[m].size = size;
    lchunks[m].list = (chunk *) NULL;
    return &lchunks[m].list;
}

/*
 * NAME:	salloc()
 * DESCRIPTION:	allocate static memory
 */
static chunk *salloc(size)
register size_t size;
{
    register chunk *c;

    /* try lists of free chunks */
    if (size >= SLIMIT) {
	register chunk **lc;

	lc = lchunk(size, FALSE);
	if (lc != (chunk **) NULL && *lc != (chunk *) NULL) {
	    /* large chunk */
	    c = *lc;
	    *lc = c->next;
	    return c;
	}
    } else if ((c=schunks[(size - MOFFSET) / STRUCT_AL - 1]) != (chunk *) NULL)
    {
	/* small chunk */
	schunks[(size - MOFFSET) / STRUCT_AL - 1] = c->next;
	return c;
    }

    /* try unused chunk list */
    if (sflist != (chunk *) NULL && sflist->size >= size) {
	if (sflist->size - size <= MOFFSET) {
	    /* remainder is too small to put in free list */
	    c = sflist;
	    sflist = c->next;
	} else {
	    register chunk *n;

	    /* split the chunk in two */
	    c = sflist;
	    n = (chunk *) ((char *) sflist + size);
	    n->size = c->size - size;
	    if (n->size <= SSMALL) {
		/* small chunk */
		n->next = schunks[(n->size - MOFFSET) / STRUCT_AL - 1];
		schunks[(n->size - MOFFSET) / STRUCT_AL - 1] = n;
		sflist = c->next;
	    } else {
		/* large enough chunk */
		n->next = c->next;
		sflist = n;
	    }
	    c->size = size;
	}
	return c;
    }

    /* try current chunk */
    if (schunk == (chunk *) NULL || schunk->size < size) {
	Uint chunksz;

	/*
	 * allocate static memory block
	 */
	if (schunk != (chunk *) NULL) {
	    schunk->next = sflist;
	    sflist = schunk;
	}
	chunksz = (size < INIT_MEM) ? INIT_MEM : size;
	schunk = (chunk *) newmem(chunksz, &slist);
	mstat.smemsize += schunk->size = chunksz;
	if (schunksz != 0 && dmem) {
	    /* fragmentation matters */
	    P_message("*** Ran out of static memory (increase static_chunk)\012"); /* LF */
	}
    }
    if (schunk->size - size <= MOFFSET) {
	/* remainder is too small */
	c = schunk;
	schunk = (chunk *) NULL;
    } else {
	c = schunk;
	schunk = (chunk *) ((char *) schunk + size);
	if ((schunk->size=c->size - size) <= SSMALL) {
	    /* small chunk */
	    schunk->next = schunks[(schunk->size - MOFFSET) / STRUCT_AL -1];
	    schunks[(schunk->size - MOFFSET) / STRUCT_AL - 1] = schunk;
	    schunk = (chunk *) NULL;
	}
	c->size = size;
    }

    if (c->size > SIZE_MASK) {
	fatal("static memory chunk too large");
    }
    return c;
}

/*
 * NAME:	sfree()
 * DESCRIPTION:	free static memory
 */
static void sfree(c)
register chunk *c;
{
    if (c->size < SLIMIT) {
	/* small chunk */
	c->next = schunks[(c->size - MOFFSET) / STRUCT_AL - 1];
	schunks[(c->size - MOFFSET) / STRUCT_AL - 1] = c;
    } else {
	register chunk **lc;

	/* large chunk */
	lc = lchunk(c->size, TRUE);
	c->next = *lc;
	*lc = c;
    }
}

/*
 * NAME:	mstatic()
 * DESCRIPTION:	enter static mode
 */
void m_static()
{
    slevel++;
}

/*
 * NAME:	mdynamic()
 * DESCRIPTION:	reenter dynamic mode
 */
void m_dynamic()
{
    --slevel;
}


/*
 * dynamic memory manager
 */

typedef struct _spnode_ {
    Uint size;			/* size of chunk */
    struct _spnode_ *parent;	/* parent node */
    struct _spnode_ *left;	/* left child node */
    struct _spnode_ *right;	/* right child node */
} spnode;

static size_t dchunksz;		/* dynamic chunk size */
static spnode *dtree;		/* splay tree of large dynamic free chunks */

/*
 * NAME:	insert()
 * DESCRIPTION:	insert a chunk in the splay tree
 */
static void insert(c)
chunk *c;
{
    register spnode *n, *t;
    register spnode *l, *r;
    register Uint size;

    n = dtree;
    dtree = t = (spnode *) c;
    t->parent = (spnode *) NULL;

    if (n == (spnode *) NULL) {
	/* first in splay tree */
	t->left = (spnode *) NULL;
	t->right = (spnode *) NULL;
    } else {
	size = t->size;
	l = r = t;

	for (;;) {
	    if (n->size < size) {
		if ((t=n->right) == (spnode *) NULL) {
		    l->right = n; n->parent = l;
		    r->left = (spnode *) NULL;
		    break;	/* finished */
		}
		if (t->size >= size) {
		    l->right = n; n->parent = l;
		    l = n;
		    n = t;
		} else {
		    /* rotate */
		    if ((n->right=t->left) != (spnode *) NULL) {
			t->left->parent = n;
		    }
		    t->left = n; n->parent = t;
		    l->right = t; t->parent = l;
		    if ((n=t->right) == (spnode *) NULL) {
			r->left = (spnode *) NULL;
			break;	/* finished */
		    }
		    l = t;
		}
	    } else {
		if ((t=n->left) == (spnode *) NULL) {
		    r->left = n; n->parent = r;
		    l->right = (spnode *) NULL;
		    break;	/* finished */
		}
		if (t->size < size) {
		    r->left = n; n->parent = r;
		    r = n;
		    n = t;
		} else {
		    /* rotate */
		    if ((n->left=t->right) != (spnode *) NULL) {
			t->right->parent = n;
		    }
		    t->right = n; n->parent = t;
		    r->left = t; t->parent = r;
		    if ((n=t->left) == (spnode *) NULL) {
			l->right = (spnode *) NULL;
			break;	/* finished */
		    }
		    r = t;
		}
	    }
	}

	/* exchange left and right subtree */
	n = dtree;
	t = n->left;
	n->left = n->right;
	n->right = t;
    }
}

/*
 * NAME:	seek()
 * DESCRIPTION:	find a chunk of the proper size in the splay tree
 */
static chunk *seek(size)
register Uint size;
{
    spnode dummy;
    register spnode *n, *t;
    register spnode *l, *r;

    if ((n=dtree) == (spnode *) NULL) {
	/* empty splay tree */
	return (chunk *) NULL;
    } else {
	l = r = &dummy;

	for (;;) {
	    if (n->size < size) {
		if ((t=n->right) == (spnode *) NULL) {
		    l->right = n; n->parent = l;
		    if (r == &dummy) {
			/* all chunks are too small */
			dtree = dummy.right;
			dtree->parent = (spnode *) NULL;
			return (chunk *) NULL;
		    }
		    if ((r->parent->left=r->right) != (spnode *) NULL) {
			r->right->parent = r->parent;
		    }
		    n = r;
		    break;	/* finished */
		}
		if (t->size >= size) {
		    l->right = n; n->parent = l;
		    l = n;
		    n = t;
		} else {
		    /* rotate */
		    if ((n->right=t->left) != (spnode *) NULL) {
			t->left->parent = n;
		    }
		    t->left = n; n->parent = t;
		    l->right = t; t->parent = l;
		    if ((n=t->right) == (spnode *) NULL) {
			if (r == &dummy) {
			    /* all chunks are too small */
			    dtree = dummy.right;
			    dtree->parent = (spnode *) NULL;
			    return (chunk *) NULL;
			}
			if ((r->parent->left=r->right) != (spnode *) NULL) {
			    r->right->parent = r->parent;
			}
			n = r;
			break;	/* finished */
		    }
		    l = t;
		}
	    } else {
		if ((t=n->left) == (spnode *) NULL) {
		    if ((r->left=n->right) != (spnode *) NULL) {
			n->right->parent = r;
		    }
		    l->right = (spnode *) NULL;
		    break;	/* finished */
		}
		if (t->size < size) {
		    r->left = n; n->parent = r;
		    r = n;
		    n = t;
		} else {
		    /* rotate */
		    if ((n->left=t->right) != (spnode *) NULL) {
			t->right->parent = n;
		    }
		    if (t->left == (spnode *) NULL) {
			r->left = n; n->parent = r;
			l->right = (spnode *) NULL;
			n = t;
			break;	/* finished */
		    }
		    t->right = n; n->parent = t;
		    r->left = t; t->parent = r;
		    r = t;
		    n = t->left;
		}
	    }
	}

	n->parent = (spnode *) NULL;
	if ((n->right=dummy.left) != (spnode *) NULL) {
	    dummy.left->parent = n;
	}
	if ((n->left=dummy.right) != (spnode *) NULL) {
	    dummy.right->parent = n;
	}

	return (chunk *) (dtree = n);
    }
}

/*
 * NAME:	delete()
 * DESCRIPTION:	delete a chunk from the splay tree
 */
static void delete(c)
chunk *c;
{
    register spnode *t, *r;
    register spnode *p, *n;

    n = (spnode *) c;
    p = n->parent;

    if (n->left == (spnode *) NULL) {
	/* there is no left subtree */
	if (p == (spnode *) NULL) {
	    if ((dtree=n->right) != (spnode *) NULL) {
		dtree->parent = (spnode *) NULL;
	    }
	} else if (n == p->left) {
	    if ((p->left=n->right) != (spnode *) NULL) {
		p->left->parent = p;
	    }
	} else if ((p->right=n->right) != (spnode *) NULL) {
	    p->right->parent = p;
	}
    } else {
	t = n->left;

	/* walk to the right in the left subtree */
	while ((r=t->right) != (spnode *) NULL) {
	    if ((t->right=r->left) != (spnode *) NULL) {
		r->left->parent = t;
	    }
	    r->left = t; t->parent = r;
	    t = r;
	}

	if (p == (spnode *) NULL) {
	    dtree = t;
	} else if (n == p->left) {
	    p->left = t;
	} else {
	    p->right = t;
	}
	t->parent = p;
	if ((t->right=n->right) != (spnode *) NULL) {
	    t->right->parent = t;
	}
    }
}

# define DSMALL		64
# define DLIMIT		(DSMALL + MOFFSET)
# define DCHUNKS	(DSMALL / STRUCT_AL - 1)
# define DCHUNKSZ	32768

static char *dlist;		/* list of dynamic memory chunks */
static chunk *dchunks[DCHUNKS];	/* list of free small chunks */
static chunk *dchunk;		/* chunk of small chunks */

/*
 * NAME:	dalloc()
 * DESCRIPTION:	allocate dynamic memory
 */
static chunk *dalloc(size)
register size_t size;
{
    register chunk *c;
    register char *p;
    register size_t sz;

    if (dchunksz == 0) {
	/*
	 * memory manager hasn't been initialized yet
	 */
	c = (chunk *) newmem(size, (char **) NULL);
	c->size = size;
	return c;
    }
    dmem = TRUE;

    if (size < DLIMIT) {
	/*
	 * small chunk
	 */
	if ((c=dchunks[(size - MOFFSET) / STRUCT_AL - 1]) != (chunk *) NULL) {
	    /* small chunk from free list */
	    dchunks[(size - MOFFSET) / STRUCT_AL - 1] = c->next;
	    return c;
	}
	if (dchunk == (chunk *) NULL) {
	    /* get new chunks chunk */
	    dchunk = dalloc(DCHUNKSZ);	/* cannot use alloc() here */
	    p = (char *) dchunk + UINTSIZE;
	    ((chunk *) p)->size = dchunk->size - UINTSIZE - SIZETSIZE;
	    dchunk->size |= DM_MAGIC;
	    dchunk = (chunk *) p;
	}
	sz = dchunk->size - size;
	c = dchunk;
	c->size = size;
	if (sz >= DLIMIT - STRUCT_AL) {
	    /* enough is left for another small chunk */
	    dchunk = (chunk *) ((char *) c + size);
	    dchunk->size = sz;
	} else {
	    /* waste sz bytes of memory */
	    dchunk = (chunk *) NULL;
	}
	return c;
    }

    size += SIZETSIZE;
    c = seek((Uint) size);
    if (c != (chunk *) NULL) {
	/*
	 * remove from free list
	 */
	delete(c);
    } else {
	/*
	 * get new dynamic chunk
	 */
	for (sz = dchunksz; sz < size + SIZETSIZE + UINTSIZE; sz += dchunksz) ;
	p = newmem(sz, &dlist);
	mstat.dmemsize += sz;

	/* no previous chunk */
	*(size_t *) p = 0;
	c = (chunk *) (p + SIZETSIZE);
	/* initialize chunk */
	c->size = sz - SIZETSIZE - UINTSIZE;
	p += c->size;
	*(size_t *) p = c->size;
	/* no following chunk */
	p += SIZETSIZE;
	((chunk *) p)->size = 0;
    }

    if ((sz=c->size - size) >= DLIMIT + SIZETSIZE) {
	/*
	 * split block, put second part in free list
	 */
	c->size = size;
	p = (char *) c + size - SIZETSIZE;
	*(size_t *) p = size;
	p += SIZETSIZE;
	((chunk *) p)->size = sz;
	*((size_t *) (p + sz - SIZETSIZE)) = sz;
	insert((chunk *) p);	/* add to free list */
    }

    if (c->size > SIZE_MASK) {
	fatal("dynamic memory chunk too large");
    }
    return c;
}

/*
 * NAME:	dfree()
 * DESCRIPTION:	free dynamic memory
 */
static void dfree(c)
register chunk *c;
{
    register char *p;

    if (dchunksz == 0) {
	/*
	 * memory manager not yet initialized
	 */
	free((char *) c);
	return;
    }

    if (c->size < DLIMIT) {
	/* small chunk */
	c->next = dchunks[(c->size - MOFFSET) / STRUCT_AL - 1];
	dchunks[(c->size - MOFFSET) / STRUCT_AL - 1] = c;
	return;
    }

    p = (char *) c - SIZETSIZE;
    if (*(size_t *) p != 0) {
	p -= *(size_t *) p - SIZETSIZE;
	if ((((chunk *) p)->size & MAGIC_MASK) == 0) {
	    /*
	     * merge with previous block
	     */
# ifdef DEBUG
	    if (((chunk *) p)->size != *(size_t *) ((char *) c - SIZETSIZE)) {
		fatal("corrupted memory chunk");
	    }
# endif
	    delete((chunk *) p);
	    ((chunk *) p)->size += c->size;
	    c = (chunk *) p;
	    *((size_t *) (p + c->size - SIZETSIZE)) = c->size;
	}
    }
    p = (char*) c + c->size;
    if (((chunk *) p)->size != 0 && (((chunk *) p)->size & MAGIC_MASK) == 0) {
	/*
	 * merge with next block
	 */
# ifdef DEBUG
	if (((chunk *) p)->size !=
			    *(size_t *) (p + ((chunk *) p)->size - SIZETSIZE)) {
	    fatal("corrupted memory chunk");
	}
# endif
	delete((chunk *) p);
	c->size += ((chunk *) p)->size;
	*((size_t *) ((char *) c + c->size - SIZETSIZE)) = c->size;
    }

    insert(c);	/* add to free list */
}


/*
 * NAME:	mem->init()
 * DESCRIPTION:	initialize memory manager
 */
void m_init(ssz, dsz)
size_t ssz, dsz;
{
    schunksz = ALGN(ssz, STRUCT_AL);
    dchunksz = ALGN(dsz, STRUCT_AL);
    if (schunksz != 0) {
	if (schunk != (chunk *) NULL) {
	    schunk->next = sflist;
	    sflist = schunk;
	}
	schunk = (chunk *) newmem(schunksz, &slist);
	mstat.smemsize += schunk->size = schunksz;
    }
    dmem = FALSE;
}


# ifdef DEBUG
static header *hlist;			/* list of all dynamic memory chunks */
# endif

/*
 * NAME:	mem->alloc()
 * DESCRIPTION:	allocate memory
 */
# ifdef DEBUG
char *m_alloc(size, file, line)
register size_t size;
char *file;
int line;
# else
char *m_alloc(size)
register size_t size;
# endif
{
    register chunk *c;

# ifdef DEBUG
    if (size == 0) {
	fatal("m_alloc(0)");
    }
# endif
    size = ALGN(size + MOFFSET, STRUCT_AL);
# ifndef DEBUG
    if (size < ALGN(sizeof(chunk), STRUCT_AL)) {
	size = ALGN(sizeof(chunk), STRUCT_AL);
    }
# endif
    if (slevel > 0) {
	c = salloc(size);
	mstat.smemused += c->size;
	c->size |= SM_MAGIC;
    } else {
	c = dalloc(size);
	mstat.dmemused += c->size;
	c->size |= DM_MAGIC;
# ifdef DEBUG
	((header *) c)->prev = (header *) NULL;
	((header *) c)->next = hlist;
	if (hlist != (header *) NULL) {
	    hlist->prev = (header *) c;
	}
	hlist = (header *) c;
# endif
    }
# ifdef DEBUG
    ((header *) c)->file = file;
    ((header *) c)->line = line;
# endif
    return (char *) c + MOFFSET;
}

/*
 * NAME:	mem->free()
 * DESCRIPTION:	free memory
 */
void m_free(mem)
char *mem;
{
    register chunk *c;

    c = (chunk *) (mem - MOFFSET);
    if ((c->size & MAGIC_MASK) == SM_MAGIC) {
	c->size &= SIZE_MASK;
	mstat.smemused -= c->size;
	sfree(c);
    } else if ((c->size & MAGIC_MASK) == DM_MAGIC) {
	c->size &= SIZE_MASK;
	mstat.dmemused -= c->size;
# ifdef DEBUG
	if (((header *) c)->next != (header *) NULL) {
	    ((header *) c)->next->prev = ((header *) c)->prev;
	}
	if (((header *) c) == hlist) {
	    hlist = ((header *) c)->next;
	} else {
	    ((header *) c)->prev->next = ((header *) c)->next;
	}
# endif
	dfree(c);
    } else {
	fatal("bad pointer in m_free");
    }
}

/*
 * NAME:	mem->realloc()
 * DESCRIPTION:	reallocate memory
 */
# ifdef DEBUG
char *m_realloc(mem, size1, size2, file, line)
char *mem, *file;
register size_t size1, size2;
int line;
# else
char *m_realloc(mem, size1, size2)
char *mem;
register size_t size1, size2;
# endif
{
    register chunk *c1, *c2;

    if (mem == (char *) NULL) {
	if (size2 == 0) {
	    return (char *) NULL;
	}
# ifdef DEBUG
	return m_alloc(size2, file, line);
# else
	return m_alloc(size2);
# endif
    }
    if (size2 == 0) {
	m_free(mem);
	return (char *) NULL;
    }

    size2 = ALGN(size2 + MOFFSET, STRUCT_AL);
# ifndef DEBUG
    if (size2 < ALGN(sizeof(chunk), STRUCT_AL)) {
	size2 = ALGN(sizeof(chunk), STRUCT_AL);
    }
# endif
    c1 = (chunk *) (mem - MOFFSET);
    if ((c1->size & MAGIC_MASK) == SM_MAGIC) {
# ifdef DEBUG
	if (size1 > (c1->size & SIZE_MASK)) {
	    fatal("bad size1 in m_realloc");
	}
# endif
	if ((c1->size & SIZE_MASK) < size2) {
	    c2 = salloc(size2);
	    if (size1 != 0) {
		memcpy((char *) c2 + MOFFSET, mem, size1);
	    }
	    c1->size &= SIZE_MASK;
	    mstat.smemused += c2->size - c1->size;
	    c2->size |= SM_MAGIC;
	    sfree(c1);
	    c1 = c2;
	}
    } else if ((c1->size & MAGIC_MASK) == DM_MAGIC) {
# ifdef DEBUG
	if (size1 > (c1->size & SIZE_MASK)) {
	    fatal("bad size1 in m_realloc");
	}
# endif
	if ((c1->size & SIZE_MASK) < ((size2 < DLIMIT) ?
				       size2 : size2 + SIZETSIZE)) {
	    c2 = dalloc(size2);
	    if (size1 != 0) {
		memcpy((char *) c2 + MOFFSET, mem, size1);
	    }
	    c1->size &= SIZE_MASK;
	    mstat.dmemused += c2->size - c1->size;
	    c2->size |= DM_MAGIC;
# ifdef DEBUG
	    ((header *) c2)->next = ((header *) c1)->next;
	    if (((header *) c1)->next != (header *) NULL) {
		((header *) c2)->next->prev = (header *) c2;
	    }
	    ((header *) c2)->prev = ((header *) c1)->prev;
	    if (((header *) c1) == hlist) {
		hlist = (header *) c2;
	    } else {
		((header *) c2)->prev->next = (header *) c2;
	    }
# endif
	    dfree(c1);
	    c1 = c2;
	}
    } else {
	fatal("bad pointer in m_realloc");
    }
# ifdef DEBUG
    ((header *) c1)->file = file;
    ((header *) c1)->line = line;
# endif
    return (char *) c1 + MOFFSET;
}

/*
 * NAME:	mem->check()
 * DESCRIPTION:	return TRUE if there is enough static memory left, or FALSE
 *		otherwise
 */
bool m_check()
{
    if (schunk == (chunk *) NULL) {
	return FALSE;
    } else {
	return (schunksz == 0 || schunk->size >= schunksz);
    }
}

/*
 * NAME:	mem->purge()
 * DESCRIPTION:	purge dynamic memory
 */
void m_purge()
{
    register char *p;

# ifdef DEBUG
    while (hlist != (header *) NULL) {
	char buf[160];
	register size_t n;

	n = (hlist->size & SIZE_MASK) - MOFFSET;
	if (n >= DLIMIT) {
	    n -= SIZETSIZE;
	}
	sprintf(buf, "FREE(%08lx/%u), %s line %u:\012", /* LF */
		(unsigned long) (hlist + 1), (unsigned int) n, hlist->file,
		hlist->line);
	if (n > 26) {
	    n = 26;
	}
	for (p = (char *) (hlist + 1); n > 0; --n, p++) {
	    if (*p >= ' ') {
		sprintf(buf + strlen(buf), " '%c", *p);
	    } else {
		sprintf(buf + strlen(buf), " %02x", UCHAR(*p));
	    }
	}
	strcat(buf, "\012");	/* LF */
	P_message(buf);
	m_free((char *) (hlist + 1));
    }
# endif

    /* purge dynamic memory */
    while (dlist != (char *) NULL) {
	p = dlist;
	dlist = *(char **) p;
	free(p);
    }
    memset(dchunks, '\0', sizeof(dchunks));
    dchunk = (chunk *) NULL;
    dtree = (spnode *) NULL;
    mstat.dmemsize = mstat.dmemused = 0;
    dmem = FALSE;

    if (schunksz != 0 &&
	(schunk == (chunk *) NULL || schunk->size < schunksz ||
	 (mstat.smemsize - mstat.smemused) * 2 < schunksz * 3)) {
	/* expand static memory */
	if (schunk != (chunk *) NULL) {
	    schunk->next = sflist;
	    sflist = schunk;
	}
	schunk = (chunk *) newmem(schunksz, &slist);
	mstat.smemsize += schunk->size = schunksz;
    }
}

/*
 * NAME:	mem->info()
 * DESCRIPTION:	return informaton about memory usage
 */
allocinfo *m_info()
{
    return &mstat;
}


/*
 * NAME:	mem->finish()
 * DESCRIPTION:	finish up memory manager
 */
void m_finish()
{
    register char *p;

    schunksz = 0;
    dchunksz = 0;

    /* purge dynamic memory */
# ifdef DEBUG
    hlist = (header *) NULL;
# endif
    m_purge();

    /* purge static memory */
    while (slist != (char *) NULL) {
	p = slist;
	slist = *(char **) p;
	free(p);
    }
    memset(schunks, '\0', sizeof(schunks));
    memset(lchunks, '\0', sizeof(lchunks));
    nlc = 0;
    schunk = (chunk *) NULL;
    sflist = (chunk *) NULL;
    slevel = 0;
    mstat.smemsize = mstat.smemused = 0;
}
