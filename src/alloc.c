# include "dgd.h"

# ifndef STRUCT_AL
# define STRUCT_AL	2		/* default memory alignment */
# endif

# define MAGIC_MASK	0xff000000L	/* magic number mask */
# define SIZE_MASK	0x00ffffffL	/* size mask */

# define SM_MAGIC	0xc5000000L	/* static mem */
# define DM_MAGIC	0xc6000000L	/* dynamic mem */

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
    int line;			/* line it was allocated from */
    char *file;			/* file it was allocated from */
    struct _header_ *prev;	/* previous in list */
    struct _header_ *next;	/* next in list */
} header;
# endif


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

# define INIT_MEM	15360	/* initial static memory chunk */
# define SLIMIT		(STRINGSZ + MOFFSET)
# define SSMALL		(MOFFSET + STRINGSZ / 8)
# define SCHUNKS	(STRINGSZ / STRUCT_AL - 1)
# define LCHUNKS	32

typedef struct {
    size_t size;		/* size of chunks in list */
    chunk *list;		/* list of chunks (possibly empty) */
} clist;

static char *slist;		/* list of static chunks */
static chunk *schunk;		/* current chunk */
static size_t schunksz;		/* size of current chunk */
static chunk *schunks[SCHUNKS];	/* lists of small free chunks */
static clist lchunks[LCHUNKS];	/* lists of large free chunks */
static unsigned int nlc;	/* # elements in large chunk list */
static chunk *sflist;		/* list of small unused chunks */
static Uint smemsize;		/* static memory size */
static Uint smemused;		/* static memory used */

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
    if (schunk == (chunk *) NULL ||
	(schunk->size < size && (schunksz != 0 || size <= INIT_MEM))) {
	/*
	 * allocate default static memory block
	 */
	if (schunk != (chunk *) NULL) {
	    schunk->next = sflist;
	    sflist = schunk;
	}
	schunk = (chunk *) newmem(INIT_MEM, &slist);
	smemsize += schunk->size = INIT_MEM;
	if (schunksz != 0) {
	    /* fragmentation matters */
	    message("*** Ran out of static memory (increase static_chunk)\012");
	}
    }
    if (schunk->size >= size) {
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
	return c;
    }

    /* allocate static memory directly */
    c = (chunk *) newmem(size, &slist);
    smemsize += c->size = size;
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
 * dynamic memory manager
 */

typedef struct _spnode_ {
    Uint size;			/* size of chunk */
    struct _spnode_ *parent;	/* parent node */
    struct _spnode_ *left;	/* left child node */
    struct _spnode_ *right;	/* right child node */
} spnode;

/*
 * NAME:	insert()
 * DESCRIPTION:	insert a chunk in the splay tree
 */
static spnode *insert(dtree, c)
spnode *dtree;
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

    return dtree;
}

/*
 * NAME:	seek()
 * DESCRIPTION:	find a chunk of the proper size in the splay tree
 */
static chunk *seek(dtree, size)
spnode **dtree;
register Uint size;
{
    spnode dummy;
    register spnode *n, *t;
    register spnode *l, *r;

    if ((n=*dtree) == (spnode *) NULL) {
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
			*dtree = dummy.right;
			(*dtree)->parent = (spnode *) NULL;
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
			    *dtree = dummy.right;
			    (*dtree)->parent = (spnode *) NULL;
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

	return (chunk *) (*dtree = n);
    }
}

/*
 * NAME:	delete()
 * DESCRIPTION:	delete a chunk from the splay tree
 */
static spnode *delete(dtree, c)
spnode *dtree;
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

    return dtree;
}

# define DSMALL		64
# define DLIMIT		(DSMALL + MOFFSET)
# define DCHUNKS	(DSMALL / STRUCT_AL - 1)
# define DCHUNKSZ	32768

typedef struct _mempool_ {
    struct _mempool_ *next;	/* next in linked list */
    chunk *dchunks[DCHUNKS];	/* list of free small chunks */
    chunk *dchunk;		/* chunk of small chunks */
    spnode *dtree;		/* splay tree of large dynamic free chunks */
# ifdef DEBUG
    header *hlist;		/* list of all dynamic memory chunks */
# endif
    Uint memsize;		/* memory allocated */
    Uint memused;		/* memory used */
} mempool;

static char *dlist;		/* list of dynamic memory chunks */
static size_t dchunksz;		/* dynamic chunk size */
static mempool *plist;		/* list of memory pools */

/*
 * NAME:	dalloc()
 * DESCRIPTION:	allocate dynamic memory
 */
static chunk *dalloc(pool, size)
register mempool *pool;
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

    if (size < DLIMIT) {
	/*
	 * small chunk
	 */
	c = pool->dchunks[(size - MOFFSET) / STRUCT_AL - 1];
	if (c != (chunk *) NULL) {
	    /* small chunk from free list */
	    pool->dchunks[(size - MOFFSET) / STRUCT_AL - 1] = c->next;
	    return c;
	}
	if (pool->dchunk == (chunk *) NULL) {
	    /* get new chunks chunk */
	    pool->dchunk = dalloc(pool, DCHUNKSZ); /* cannot use alloc() here */
	    p = (char *) pool->dchunk + UINTSIZE;
	    ((chunk *) p)->size = pool->dchunk->size - UINTSIZE - SIZETSIZE;
	    pool->dchunk->size |= DM_MAGIC;
	    pool->dchunk = (chunk *) p;
	}
	sz = pool->dchunk->size - size;
	c = pool->dchunk;
	c->size = size;
	if (sz >= DLIMIT - STRUCT_AL) {
	    /* enough is left for another small chunk */
	    pool->dchunk = (chunk *) ((char *) c + size);
	    pool->dchunk->size = sz;
	} else {
	    /* waste sz bytes of memory */
	    pool->dchunk = (chunk *) NULL;
	}
	return c;
    }

    size += SIZETSIZE;
    c = seek(&pool->dtree, (Uint) size);
    if (c != (chunk *) NULL) {
	/*
	 * remove from free list
	 */
	pool->dtree = delete(pool->dtree, c);
    } else {
	/*
	 * get new dynamic chunk
	 */
	for (sz = dchunksz; sz < size + SIZETSIZE + UINTSIZE; sz += dchunksz) ;
	p = newmem(sz, &dlist);
	pool->memsize += sz;

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
	pool->dtree = insert(pool->dtree, (chunk *) p);	/* add to free list */
    }
    return c;
}

/*
 * NAME:	dfree()
 * DESCRIPTION:	free dynamic memory
 */
static void dfree(pool, c)
register mempool *pool;
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
	c->next = pool->dchunks[(c->size - MOFFSET) / STRUCT_AL - 1];
	pool->dchunks[(c->size - MOFFSET) / STRUCT_AL - 1] = c;
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
	    pool->dtree = delete(pool->dtree, (chunk *) p);
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
	pool->dtree = delete(pool->dtree, (chunk *) p);
	c->size += ((chunk *) p)->size;
	*((size_t *) ((char *) c + c->size - SIZETSIZE)) = c->size;
    }

    pool->dtree = insert(pool->dtree, c);	/* add to free list */
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
	smemsize += schunk->size = schunksz;
    }
}

/*
 * NAME:	mem->new_pool()
 * DESCRIPTION:	allocate a new memory pool
 */
mempool *m_new_pool()
{
    register mempool *pool;

    pool = SALLOC(mempool, 1);
    memset(pool, '\0', sizeof(mempool));
    pool->next = plist;
    plist = pool;

    return pool;
}

/*
 * NAME:	mem->alloc()
 * DESCRIPTION:	allocate memory
 */
# ifdef DEBUG
char *m_alloc(pool, size, file, line)
mempool *pool;
register size_t size;
char *file;
int line;
# else
char *m_alloc(pool, size)
mempool *pool;
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
    if (pool == (mempool *) NULL) {
	c = salloc(size);
	smemused += c->size;
	c->size |= SM_MAGIC;
    } else {
	c = dalloc(pool, size);
	pool->memused += c->size;
	c->size |= DM_MAGIC;
# ifdef DEBUG
	((header *) c)->prev = (header *) NULL;
	((header *) c)->next = pool->hlist;
	if (pool->hlist != (header *) NULL) {
	    pool->hlist->prev = (header *) c;
	}
	pool->hlist = (header *) c;
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
void m_free(pool, mem)
mempool *pool;
char *mem;
{
    register chunk *c;

    c = (chunk *) (mem - MOFFSET);
    if ((c->size & MAGIC_MASK) == SM_MAGIC) {
# ifdef DEBUG
	if (pool != (mempool *) NULL) {
	    fatal("static memory freed in memory pool");
	}
# endif
	c->size &= SIZE_MASK;
	smemused -= c->size;
	sfree(c);
    } else if ((c->size & MAGIC_MASK) == DM_MAGIC) {
# ifdef DEBUG
	if (pool == (mempool *) NULL) {
	    fatal("dynamic memory freed without memory pool");
	}
# endif
	c->size &= SIZE_MASK;
	pool->memused -= c->size;
# ifdef DEBUG
	if (((header *) c)->next != (header *) NULL) {
	    ((header *) c)->next->prev = ((header *) c)->prev;
	}
	if (((header *) c) == pool->hlist) {
	    pool->hlist = ((header *) c)->next;
	} else {
	    ((header *) c)->prev->next = ((header *) c)->next;
	}
# endif
	dfree(pool, c);
    } else {
	fatal("bad pointer in m_free");
    }
}

/*
 * NAME:	mem->realloc()
 * DESCRIPTION:	reallocate memory
 */
# ifdef DEBUG
char *m_realloc(pool, mem, size1, size2, file, line)
mempool *pool;
char *mem, *file;
register size_t size1, size2;
int line;
# else
char *m_realloc(pool, mem, size1, size2)
mempool *pool;
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
	return m_alloc(pool, size2, file, line);
# else
	return m_alloc(pool, size2);
# endif
    }
    if (size2 == 0) {
	m_free(pool, mem);
	return (char *) NULL;
    }

    size2 = ALGN(size2 + MOFFSET, STRUCT_AL);
# ifndef DEBUG
    if (size2 < ALGN(sizeof(chunk), STRUCT_AL)) {
	size2 = ALGN(sizeof(chunk), STRUCT_AL);
    }
# endif
    c1 = (chunk *) (mem - MOFFSET);
    if ((c1->size & MAGIC_MASK) == DM_MAGIC) {
# ifdef DEBUG
	if (pool == (mempool *) NULL) {
	    fatal("dynamic memory reallocated without memory pool");
	}
	if (size1 > (c1->size & SIZE_MASK)) {
	    fatal("bad size1 in m_realloc");
	}
# endif
	if ((c1->size & SIZE_MASK) < ((size2 < DLIMIT) ?
				       size2 : size2 + SIZETSIZE)) {
	    c2 = dalloc(pool, size2);
	    if (size1 != 0) {
		memcpy((char *) c2 + MOFFSET, mem, size1);
	    }
	    c1->size &= SIZE_MASK;
	    pool->memused += c2->size - c1->size;
	    c2->size |= DM_MAGIC;
# ifdef DEBUG
	    ((header *) c2)->next = ((header *) c1)->next;
	    if (((header *) c1)->next != (header *) NULL) {
		((header *) c2)->next->prev = (header *) c2;
	    }
	    ((header *) c2)->prev = ((header *) c1)->prev;
	    if (((header *) c1) == pool->hlist) {
		pool->hlist = (header *) c2;
	    } else {
		((header *) c2)->prev->next = (header *) c2;
	    }
# endif
	    dfree(pool, c1);
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
    register mempool *pool;

    for (pool = plist; pool != (mempool *) NULL; pool = pool->next) {
# ifdef DEBUG
	while (pool->hlist != (header *) NULL) {
	    char buf[160];
	    register size_t n;

	    n = (pool->hlist->size & SIZE_MASK) - MOFFSET;
	    if (n >= DLIMIT) {
		n -= SIZETSIZE;
	    }
	    sprintf(buf, "FREE(%08lx/%u), %s line %u:\012", /* LF */
		    (unsigned long) (pool->hlist + 1), n, pool->hlist->file,
		    pool->hlist->line);
	    if (n > 26) {
		n = 26;
	    }
	    for (p = (char *) (pool->hlist + 1); n > 0; --n, p++) {
		if (*p >= ' ') {
		    sprintf(buf + strlen(buf), " '%c", *p);
		} else {
		    sprintf(buf + strlen(buf), " %02x", UCHAR(*p));
		}
	    }
	    message("%s\012", buf);	/* LF */
	    m_free(pool, (char *) (pool->hlist + 1));
	}
# endif
	memset(pool->dchunks, '\0', sizeof(pool->dchunks));
	pool->dchunk = (chunk *) NULL;
	pool->dtree = (spnode *) NULL;
	pool->memsize = pool->memused = 0;
    }

    /* purge dynamic memory */
    while (dlist != (char *) NULL) {
	p = dlist;
	dlist = *(char **) p;
	free(p);
    }

    if (schunksz != 0 &&
	(schunk == (chunk *) NULL || schunk->size < schunksz ||
	 (smemsize - smemused) * 2 < schunksz * 3)) {
	/* expand static memory */
	if (schunk != (chunk *) NULL) {
	    schunk->next = sflist;
	    sflist = schunk;
	}
	schunk = (chunk *) newmem(schunksz, &slist);
	smemsize += schunk->size = schunksz;
    }
}

/*
 * NAME:	mem->info()
 * DESCRIPTION:	return informaton about memory usage
 */
void m_info(sms, smu, dms, dmu)
Uint *sms, *smu, *dms, *dmu;
{
    register mempool *pool;

    *sms = smemsize;
    *smu = smemused;
    *dms = 0;
    *dmu = 0;
    for (pool = plist; pool != (mempool *) NULL; pool = pool->next) {
	*dms += pool->memsize;
	*dmu += pool->memused;
    }
}


/*
 * NAME:	mem->finish()
 * DESCRIPTION:	finish up memory manager
 */
void m_finish()
{
    register char *p;

    /* purge dynamic memory */
    m_purge();
    plist = (mempool *) NULL;

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
    smemsize = smemused = 0;

    schunksz = 0;
    dchunksz = 0;
}
