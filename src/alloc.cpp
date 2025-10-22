/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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

# include "dgd.h"

# define SIZE_SHIFT	(8 * (sizeof(size_t) - 1))
# define MAGIC_MASK	((size_t) 0xc0 << SIZE_SHIFT)	/* magic number mask */
# define SIZE_MASK	(~MAGIC_MASK)			/* size mask */

# define SM_MAGIC	((size_t) 0x80 << SIZE_SHIFT)	/* static mem */
# define DM_MAGIC	((size_t) 0xc0 << SIZE_SHIFT)	/* dynamic mem */

# define SIZETSIZE	ALGN(sizeof(size_t), STRUCT_AL)


class MemChunk {
public:
    /*
     * allocate a chunk (low-level)
     */
    static MemChunk *alloc(size_t size, MemChunk **list) {
	MemChunk *mem;

	if (list != (MemChunk **) NULL) {
	    size += ALGN(sizeof(MemChunk *), STRUCT_AL);
	}
	mem = (MemChunk *) std::malloc(size);
	if (mem == (MemChunk *) NULL) {
	    EC->fatal("out of memory");
	}
	if (list != (MemChunk **) NULL) {
	    *((MemChunk **) mem) = *list;
	    *list = mem;
	    mem = (MemChunk *) ((char *) mem +
				ALGN(sizeof(MemChunk *), STRUCT_AL));
	}
	return mem;
    }

    /*
     * free a chunk (low-level)
     */
    static MemChunk *free(MemChunk *mem) {
	MemChunk *next;

	next = *(MemChunk **) mem;
	std::free(mem);
	return next;
    }

    size_t size;			/* size of chunk */
    union {
	MemChunk *next;			/* next chunk */
	class SplayNode *parent;	/* parent node in splay tree */
    };
};

# ifdef DEBUG
/*
 * debug extension of chunk
 */
class MemHeader : public MemChunk {
public:
    MemHeader *prev;		/* previous in list */
# ifdef MEMDEBUG
    const char *file;		/* file it was allocated from */
    int line;			/* line it was allocated from */
# endif
};

# define MOFFSET	ALGN(sizeof(MemHeader), STRUCT_AL)
# else
# define MOFFSET	SIZETSIZE
# endif


/*
 * static memory manager
 */

# define INIT_MEM	15360		/* initial static memory chunk */
# define SLIMIT		(STRINGSZ + MOFFSET)
# define SSMALL		(MOFFSET + STRINGSZ / 8)
# define SCHUNKS	(STRINGSZ / STRUCT_AL - 1)
# define LCHUNKS	32

class ChunkList {
public:
    static void finish() {
	memset(lists, '\0', sizeof(lists));
	nLists = 0;
    }

    static MemChunk **largeList(size_t size, bool add) {
	unsigned int h, l, m;

	l = m = 0;
	h = nLists;
	while (l < h) {
	    m = (l + h) >> 1;
	    if (lists[m].size == size) {
		return &lists[m].list;		/* found */
	    } else if (lists[m].size > size) {
		h = m;				/* search in lower half */
	    } else {
		l = ++m;			/* search in upper half */
	    }
	}

	if (!add) {
	    return (MemChunk **) NULL;		/* not found */
	}
	/* make a new list */
	if (nLists == LCHUNKS) {
	    EC->fatal("too many different large static chunks");
	}
	for (l = nLists++; l > m; --l) {
	    lists[l] = lists[l - 1];
	}
	lists[m].size = size;
	lists[m].list = (MemChunk *) NULL;
	return &lists[m].list;
    }

    size_t size;			/* size of chunks in list */
    MemChunk *list;			/* list of chunks (possibly empty) */

private:
    static ChunkList lists[LCHUNKS];
    static unsigned int nLists;		/* # elements in large chunk list */
};

ChunkList ChunkList::lists[LCHUNKS];
unsigned int ChunkList::nLists;


class StaticMem {
public:
    static void init(size_t size) {
	schunksz = ALGN(size, STRUCT_AL);
	if (schunksz != 0) {
	    if (schunk != (MemChunk *) NULL) {
		schunk->next = sflist;
		sflist = schunk;
	    }
	    schunk = MemChunk::alloc(schunksz, &slist);
	    memSize += schunk->size = schunksz;
	}
	dmem = FALSE;
    }

    static void finish() {
	ChunkList::finish();

	while (slist != (MemChunk *) NULL) {
	    slist = MemChunk::free(slist);
	}
	schunksz = 0;
	memset(schunks, '\0', sizeof(schunks));
	schunk = (MemChunk *) NULL;
	sflist = (MemChunk *) NULL;
	memSize = memUsed = 0;
    }

    /*
     * allocate static memory
     */
    static MemChunk *alloc(size_t size) {
	MemChunk *c;

	/* try lists of free chunks */
	if (size >= SLIMIT) {
	    MemChunk **lc;

	    lc = ChunkList::largeList(size, FALSE);
	    if (lc != (MemChunk **) NULL && *lc != (MemChunk *) NULL) {
		/* large chunk */
		c = *lc;
		*lc = c->next;
		return c;
	    }
	} else if ((c=schunks[(size - MOFFSET) / STRUCT_AL - 1]) !=
							    (MemChunk *) NULL) {
	    /* small chunk */
	    schunks[(size - MOFFSET) / STRUCT_AL - 1] = c->next;
	    return c;
	}

	/* try unused chunk list */
	if (sflist != (MemChunk *) NULL && sflist->size >= size) {
	    if (sflist->size - size <= MOFFSET) {
		/* remainder is too small to put in free list */
		c = sflist;
		sflist = c->next;
	    } else {
		MemChunk *n;

		/* split the chunk in two */
		c = sflist;
		n = (MemChunk *) ((char *) sflist + size);
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

	if (size > ((schunksz != 0) ? schunksz : INIT_MEM)) {
	    /*
	     * allocate directly
	     */
	    c = MemChunk::alloc(size, &slist);
	    memSize += c->size = size;
	} else {
	    if (schunk == (MemChunk *) NULL || schunk->size < size) {
		size_t chunksz;

		/*
		 * allocate static memory block
		 */
		if (schunk != (MemChunk *) NULL) {
		    schunk->next = sflist;
		    sflist = schunk;
		}
		chunksz = (schunksz != 0) ? schunksz : INIT_MEM;
		schunk = MemChunk::alloc(chunksz, &slist);
		memSize += schunk->size = chunksz;
		if (schunksz != 0 && dmem) {
		    /* fragmentation matters */
		    EC->message("*** Ran out of static memory (increase static_chunk)\012"); /* LF */
		}
	    }
	    if (schunk->size - size <= MOFFSET) {
		/* remainder is too small */
		c = schunk;
		schunk = (MemChunk *) NULL;
	    } else {
		c = schunk;
		schunk = (MemChunk *) ((char *) schunk + size);
		if ((schunk->size=c->size - size) <= SSMALL) {
		    /* small chunk */
		    schunk->next = schunks[(schunk->size - MOFFSET) / STRUCT_AL -1];
		    schunks[(schunk->size - MOFFSET) / STRUCT_AL - 1] = schunk;
		    schunk = (MemChunk *) NULL;
		}
		c->size = size;
	    }
	}

	if (c->size > SIZE_MASK) {
	    EC->fatal("static memory chunk too large");
	}
	return c;
    }

    /*
     * free static memory
     */
    static void free(MemChunk *c) {
# ifdef DEBUG
	memset(c + 1, '\xdd', c->size - sizeof(MemChunk));
# endif
	if (c->size < SLIMIT) {
	    /* small chunk */
	    c->next = schunks[(c->size - MOFFSET) / STRUCT_AL - 1];
	    schunks[(c->size - MOFFSET) / STRUCT_AL - 1] = c;
	} else {
	    MemChunk **lc;

	    /* large chunk */
	    lc = ChunkList::largeList(c->size, TRUE);
	    c->next = *lc;
	    *lc = c;
	}
    }

    static bool check() {
	if (schunk == (MemChunk *) NULL) {
	    return FALSE;
	} else {
	    return (schunksz == 0 || schunk->size >= schunksz);
	}
    }

    static void expand() {
	dmem = FALSE;

	if (schunksz != 0 &&
	    (schunk == (MemChunk *) NULL || schunk->size < schunksz ||
	     (memSize - memUsed) * 2 < schunksz * 3)) {
	    /* expand static memory */
	    if (schunk != (MemChunk *) NULL) {
		schunk->next = sflist;
		sflist = schunk;
	    }
	    schunk = MemChunk::alloc(schunksz, &slist);
	    memSize += schunk->size = schunksz;
	}
    }

    static bool dmem;			/* any dynamic memory allocated? */
    static size_t memSize;		/* static memory allocated */
    static size_t memUsed;		/* static memory used */

private:
    static MemChunk *slist;		/* list of static chunks */
    static MemChunk *schunk;		/* current chunk */
    static size_t schunksz;		/* size of current chunk */
    static MemChunk *schunks[SCHUNKS];	/* lists of small free chunks */
    static MemChunk *sflist;		/* list of small unused chunks */
};

bool StaticMem::dmem;
size_t StaticMem::memSize;
size_t StaticMem::memUsed;
MemChunk *StaticMem::slist;
MemChunk *StaticMem::schunk;
size_t StaticMem::schunksz;
MemChunk *StaticMem::schunks[SCHUNKS];
MemChunk *StaticMem::sflist;


/*
 * dynamic memory manager
 */

class SplayNode : public MemChunk {
public:
    /*
     * insert a node in a splay tree
     */
    static void insert(SplayNode *&tree, SplayNode *t) {
	SplayNode *n, *l, *r;
	size_t size;

	n = tree;
	tree = t;
	t->parent = (SplayNode *) NULL;

	if (n == (SplayNode *) NULL) {
	    /* first in splay tree */
	    t->left = (SplayNode *) NULL;
	    t->right = (SplayNode *) NULL;
	} else {
	    size = t->size;
	    l = r = t;

	    for (;;) {
		if (n->size < size) {
		    if ((t=n->right) == (SplayNode *) NULL) {
			l->right = n; n->parent = l;
			r->left = (SplayNode *) NULL;
			break;	/* finished */
		    }
		    if (t->size >= size) {
			l->right = n; n->parent = l;
			l = n;
			n = t;
		    } else {
			/* rotate */
			if ((n->right=t->left) != (SplayNode *) NULL) {
			    t->left->parent = n;
			}
			t->left = n; n->parent = t;
			l->right = t; t->parent = l;
			if ((n=t->right) == (SplayNode *) NULL) {
			    r->left = (SplayNode *) NULL;
			    break;	/* finished */
			}
			l = t;
		    }
		} else {
		    if ((t=n->left) == (SplayNode *) NULL) {
			r->left = n; n->parent = r;
			l->right = (SplayNode *) NULL;
			break;	/* finished */
		    }
		    if (t->size < size) {
			r->left = n; n->parent = r;
			r = n;
			n = t;
		    } else {
			/* rotate */
			if ((n->left=t->right) != (SplayNode *) NULL) {
			    t->right->parent = n;
			}
			t->right = n; n->parent = t;
			r->left = t; t->parent = r;
			if ((n=t->left) == (SplayNode *) NULL) {
			    l->right = (SplayNode *) NULL;
			    break;	/* finished */
			}
			r = t;
		    }
		}
	    }

	    /* exchange left and right subtree */
	    n = tree;
	    t = n->left;
	    n->left = n->right;
	    n->right = t;
	}
    }

    /*
     * find a node of the proper size in the splay tree
     */
    static SplayNode *seek(SplayNode *&tree, size_t size) {
	SplayNode dummy;
	SplayNode *n, *t, *l, *r;

	n = tree;
	if (n == (SplayNode *) NULL) {
	    /* empty splay tree */
	    return (SplayNode *) NULL;
	}

	l = r = &dummy;

	for (;;) {
	    if (n->size < size) {
		if ((t=n->right) == (SplayNode *) NULL) {
		    l->right = n; n->parent = l;
		    if (r == &dummy) {
			/* all chunks are too small */
			tree = dummy.right;
			tree->parent = (SplayNode *) NULL;
			return (SplayNode *) NULL;
		    }
		    if ((r->parent->left=r->right) != (SplayNode *) NULL) {
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
		    if ((n->right=t->left) != (SplayNode *) NULL) {
			t->left->parent = n;
		    }
		    t->left = n; n->parent = t;
		    l->right = t; t->parent = l;
		    if ((n=t->right) == (SplayNode *) NULL) {
			if (r == &dummy) {
			    /* all chunks are too small */
			    tree = dummy.right;
			    tree->parent = (SplayNode *) NULL;
			    return (SplayNode *) NULL;
			}
			if ((r->parent->left=r->right) != (SplayNode *) NULL) {
			    r->right->parent = r->parent;
			}
			n = r;
			break;	/* finished */
		    }
		    l = t;
		}
	    } else {
		if ((t=n->left) == (SplayNode *) NULL) {
		    if ((r->left=n->right) != (SplayNode *) NULL) {
			n->right->parent = r;
		    }
		    l->right = (SplayNode *) NULL;
		    break;	/* finished */
		}
		if (t->size < size) {
		    r->left = n; n->parent = r;
		    r = n;
		    n = t;
		} else {
		    /* rotate */
		    if ((n->left=t->right) != (SplayNode *) NULL) {
			t->right->parent = n;
		    }
		    if (t->left == (SplayNode *) NULL) {
			r->left = n; n->parent = r;
			l->right = (SplayNode *) NULL;
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

	n->parent = (SplayNode *) NULL;
	if ((n->right=dummy.left) != (SplayNode *) NULL) {
	    dummy.left->parent = n;
	}
	if ((n->left=dummy.right) != (SplayNode *) NULL) {
	    dummy.right->parent = n;
	}

	return tree = n;
    }

    /*
     * delete a node from the splay tree
     */
    static void del(SplayNode *&tree, SplayNode *n) {
	SplayNode *t, *r, *p;

	p = n->parent;

	if (n->left == (SplayNode *) NULL) {
	    /* there is no left subtree */
	    if (p == (SplayNode *) NULL) {
		if ((tree=n->right) != (SplayNode *) NULL) {
		    tree->parent = (SplayNode *) NULL;
		}
	    } else if (n == p->left) {
		if ((p->left=n->right) != (SplayNode *) NULL) {
		    p->left->parent = p;
		}
	    } else if ((p->right=n->right) != (SplayNode *) NULL) {
		p->right->parent = p;
	    }
	} else {
	    t = n->left;

	    /* walk to the right in the left subtree */
	    while ((r=t->right) != (SplayNode *) NULL) {
		if ((t->right=r->left) != (SplayNode *) NULL) {
		    r->left->parent = t;
		}
		r->left = t; t->parent = r;
		t = r;
	    }

	    if (p == (SplayNode *) NULL) {
		tree = t;
	    } else if (n == p->left) {
		p->left = t;
	    } else {
		p->right = t;
	    }
	    t->parent = p;
	    if ((t->right=n->right) != (SplayNode *) NULL) {
		t->right->parent = t;
	    }
	}
    }

    SplayNode *left;		/* left child node */
    SplayNode *right;		/* right child node */
};


# define DSMALL		64
# define DLIMIT		(DSMALL + MOFFSET)
# define DCHUNKS	(DSMALL / STRUCT_AL - 1)
# define DCHUNKSZ	32768

class DynamicMem {
public:
    static void init(size_t size) {
	dchunksz = ALGN(size, STRUCT_AL);
    }

    static void purge() {
	/* purge dynamic memory */
	while (dlist != (MemChunk *) NULL) {
	    dlist = MemChunk::free(dlist);
	}
	memset(dchunks, '\0', sizeof(dchunks));
	dchunk = (MemChunk *) NULL;
	dtree = (SplayNode *) NULL;
	memSize = memUsed = 0;
    }

    static void finish() {
	dchunksz = 0;
    }

    /*
     * allocate dynamic memory
     */
    static MemChunk *alloc(size_t size) {
	MemChunk *c;
	char *p;
	size_t sz;

	if (dchunksz == 0) {
	    /*
	     * memory manager hasn't been initialized yet
	     */
	    c = MemChunk::alloc(size, (MemChunk **) NULL);
	    c->size = size;
	    return c;
	}
	StaticMem::dmem = TRUE;

	if (size < DLIMIT) {
	    /*
	     * small chunk
	     */
	    if ((c=dchunks[(size - MOFFSET) / STRUCT_AL - 1]) !=
							    (MemChunk *) NULL) {
		/* small chunk from free list */
		dchunks[(size - MOFFSET) / STRUCT_AL - 1] = c->next;
		return c;
	    }
	    if (dchunk == (MemChunk *) NULL) {
		/* get new chunks chunk */
		dchunk = alloc(DCHUNKSZ);
		p = (char *) dchunk + SIZETSIZE;
		((MemChunk *) p)->size = dchunk->size - SIZETSIZE - SIZETSIZE;
		dchunk->size |= DM_MAGIC;
		dchunk = (MemChunk *) p;
	    }
	    sz = dchunk->size - size;
	    c = dchunk;
	    c->size = size;
	    if (sz >= DLIMIT - STRUCT_AL) {
		/* enough is left for another small chunk */
		dchunk = (MemChunk *) ((char *) c + size);
		dchunk->size = sz;
	    } else {
		/* waste sz bytes of memory */
		dchunk = (MemChunk *) NULL;
	    }
	    return c;
	}

	size += SIZETSIZE;
	c = SplayNode::seek(dtree, size);
	if (c != (MemChunk *) NULL) {
	    /*
	     * remove from free list
	     */
	    SplayNode::del(dtree, (SplayNode *) c);
	} else {
	    /*
	     * get new dynamic chunk
	     */
	    for (sz = dchunksz; sz < size + SIZETSIZE + SIZETSIZE;
		 sz += dchunksz) ;
	    p = (char *) MemChunk::alloc(sz, &dlist);
	    memSize += sz;

	    /* no previous chunk */
	    *(size_t *) p = 0;
	    c = (MemChunk *) (p + SIZETSIZE);
	    /* initialize chunk */
	    c->size = sz - SIZETSIZE - SIZETSIZE;
	    p += c->size;
	    *(size_t *) p = c->size;
	    /* no following chunk */
	    p += SIZETSIZE;
	    ((MemChunk *) p)->size = 0;
	}

	if ((sz=c->size - size) >= DLIMIT + SIZETSIZE) {
	    /*
	     * split block, put second part in free list
	     */
	    c->size = size;
	    p = (char *) c + size - SIZETSIZE;
	    *(size_t *) p = size;
	    p += SIZETSIZE;
	    ((MemChunk *) p)->size = sz;
	    *((size_t *) (p + sz - SIZETSIZE)) = sz;
	    SplayNode::insert(dtree, (SplayNode *) p);	/* add to free list */
	}

	if (c->size > SIZE_MASK) {
	    EC->fatal("dynamic memory chunk too large");
	}
	return c;
    }

    /*
     * free dynamic memory
     */
    static void free(MemChunk *c) {
	char *p;

	if (dchunksz == 0) {
	    /*
	     * memory manager not yet initialized
	     */
	    MemChunk::free(c);
	    return;
	}

# ifdef DEBUG
	memset(c + 1, '\xdd', c->size - sizeof(MemChunk) - SIZETSIZE);
# endif
	if (c->size < DLIMIT) {
	    /* small chunk */
	    c->next = dchunks[(c->size - MOFFSET) / STRUCT_AL - 1];
	    dchunks[(c->size - MOFFSET) / STRUCT_AL - 1] = c;
	    return;
	}

	p = (char *) c - SIZETSIZE;
	if (*(size_t *) p != 0) {
	    p -= *(size_t *) p - SIZETSIZE;
	    if ((((MemChunk *) p)->size & MAGIC_MASK) == 0) {
		/*
		 * merge with previous block
		 */
# ifdef DEBUG
		if (((MemChunk *) p)->size !=
					*(size_t *) ((char *) c - SIZETSIZE)) {
		    EC->fatal("corrupted memory chunk");
		}
# endif
		SplayNode::del(dtree, (SplayNode *) p);
		((MemChunk *) p)->size += c->size;
		c = (MemChunk *) p;
		*((size_t *) (p + c->size - SIZETSIZE)) = c->size;
	    }
	}
	p = (char*) c + c->size;
	if (((MemChunk *) p)->size != 0 &&
	    (((MemChunk *) p)->size & MAGIC_MASK) == 0) {
	    /*
	     * merge with next block
	     */
# ifdef DEBUG
	    if (((MemChunk *) p)->size !=
			*(size_t *) (p + ((MemChunk *) p)->size - SIZETSIZE)) {
		EC->fatal("corrupted memory chunk");
	    }
# endif
	    SplayNode::del(dtree, (SplayNode *) p);
	    c->size += ((MemChunk *) p)->size;
	    *((size_t *) ((char *) c + c->size - SIZETSIZE)) = c->size;
	}

	SplayNode::insert(dtree, (SplayNode *) c);	/* add to free list */
    }

    static SplayNode *dtree;	/* splay tree of large dynamic free chunks */
    static MemChunk *dlist;		/* list of dynamic memory chunks */
    static MemChunk *dchunks[DCHUNKS];	/* list of free small chunks */
    static MemChunk *dchunk;		/* chunk of small chunks */
    static size_t dchunksz;		/* dynamic chunk size */
    static size_t memSize;		/* dynamic memory size */
    static size_t memUsed;		/* dynamic memory used */
};

SplayNode *DynamicMem::dtree;		/* large dynamic free chunks */
MemChunk *DynamicMem::dlist;		/* list of dynamic memory chunks */
MemChunk *DynamicMem::dchunks[DCHUNKS];	/* list of free small chunks */
MemChunk *DynamicMem::dchunk;		/* chunk of small chunks */
size_t DynamicMem::dchunksz;		/* dynamic chunk size */
size_t DynamicMem::memSize;		/* dynamic memory size */
size_t DynamicMem::memUsed;		/* dynamic memory used */


# ifdef DEBUG
static MemHeader *hlist;	/* list of all dynamic memory chunks */
# endif

static AllocImpl MMI;
Alloc *MM = &MMI;

static int sLevel;

/*
 * initialize memory manager
 */
void AllocImpl::init(size_t ssz, size_t dsz)
{
    StaticMem::init(ssz);
    DynamicMem::init(dsz);
}

/*
 * enter static mode
 */
void AllocImpl::staticMode()
{
    sLevel++;
}

/*
 * reenter dynamic mode
 */
void AllocImpl::dynamicMode()
{
    --sLevel;
}

/*
 * allocate memory
 */
# ifdef MEMDEBUG
char *AllocImpl::alloc(size_t size, const char *file, int line)
# else
char *AllocImpl::alloc(size_t size)
# endif
{
    MemChunk *c;

# ifdef DEBUG
    if (size == 0) {
	EC->fatal("alloc(0)");
    }
# endif
    size = ALGN(size + MOFFSET, STRUCT_AL);
# ifndef DEBUG
    if (size < ALGN(sizeof(MemChunk), STRUCT_AL)) {
	size = ALGN(sizeof(MemChunk), STRUCT_AL);
    }
# endif
    if (size > SIZE_MASK) {
	EC->fatal("size too big in alloc");
    }
    if (sLevel > 0) {
	c = StaticMem::alloc(size);
	StaticMem::memUsed += c->size;
	c->size |= SM_MAGIC;
    } else {
	c = DynamicMem::alloc(size);
	DynamicMem::memUsed += c->size;
	c->size |= DM_MAGIC;
# ifdef DEBUG
	((MemHeader *) c)->prev = (MemHeader *) NULL;
	c->next = hlist;
	if (hlist != (MemHeader *) NULL) {
	    hlist->prev = (MemHeader *) c;
	}
	hlist = (MemHeader *) c;
# endif
    }
# ifdef MEMDEBUG
    ((MemHeader *) c)->file = file;
    ((MemHeader *) c)->line = line;
# endif
    return (char *) c + MOFFSET;
}

/*
 * free memory
 */
void AllocImpl::free(char *mem)
{
    MemChunk *c;

    c = (MemChunk *) (mem - MOFFSET);
    if ((c->size & MAGIC_MASK) == SM_MAGIC) {
	c->size &= SIZE_MASK;
	StaticMem::memUsed -= c->size;
	StaticMem::free(c);
    } else if ((c->size & MAGIC_MASK) == DM_MAGIC) {
	c->size &= SIZE_MASK;
	DynamicMem::memUsed -= c->size;
# ifdef DEBUG
	if (c->next != (MemChunk *) NULL) {
	    ((MemHeader *) c->next)->prev = ((MemHeader *) c)->prev;
	}
	if ((MemHeader *) c == hlist) {
	    hlist = (MemHeader *) c->next;
	} else {
	    ((MemHeader *) c)->prev->next = c->next;
	}
# endif
	DynamicMem::free(c);
    } else {
	EC->fatal("bad pointer in free");
    }
}

/*
 * reallocate memory
 */
# ifdef MEMDEBUG
char *AllocImpl::realloc(char *mem, size_t size1, size_t size2,
			 const char *file, int line)
# else
char *AllocImpl::realloc(char *mem, size_t size1, size_t size2)
# endif
{
    MemChunk *c1, *c2;

    if (mem == (char *) NULL) {
	if (size2 == 0) {
	    return (char *) NULL;
	}
# ifdef MEMDEBUG
	return alloc(size2, file, line);
# else
	return alloc(size2);
# endif
    }
    if (size2 == 0) {
	free(mem);
	return (char *) NULL;
    }

    size2 = ALGN(size2 + MOFFSET, STRUCT_AL);
# ifndef DEBUG
    if (size2 < ALGN(sizeof(MemChunk), STRUCT_AL)) {
	size2 = ALGN(sizeof(MemChunk), STRUCT_AL);
    }
# endif
    c1 = (MemChunk *) (mem - MOFFSET);
    if ((c1->size & MAGIC_MASK) == SM_MAGIC) {
# ifdef DEBUG
	if (size1 > (c1->size & SIZE_MASK)) {
	    EC->fatal("bad size1 in m_realloc");
	}
# endif
	if ((c1->size & SIZE_MASK) < size2) {
	    c2 = StaticMem::alloc(size2);
	    if (size1 != 0) {
		memcpy((char *) c2 + MOFFSET, mem, size1);
	    }
	    c1->size &= SIZE_MASK;
	    StaticMem::memUsed += c2->size - c1->size;
	    c2->size |= SM_MAGIC;
	    StaticMem::free(c1);
	    c1 = c2;
	}
    } else if ((c1->size & MAGIC_MASK) == DM_MAGIC) {
# ifdef DEBUG
	if (size1 > (c1->size & SIZE_MASK)) {
	    EC->fatal("bad size1 in m_realloc");
	}
# endif
	if ((c1->size & SIZE_MASK) < ((size2 < DLIMIT) ?
				       size2 : size2 + SIZETSIZE)) {
	    c2 = DynamicMem::alloc(size2);
	    if (size1 != 0) {
		memcpy((char *) c2 + MOFFSET, mem, size1);
	    }
	    c1->size &= SIZE_MASK;
	    DynamicMem::memUsed += c2->size - c1->size;
	    c2->size |= DM_MAGIC;
# ifdef DEBUG
	    c2->next = c1->next;
	    if (c1->next != (MemChunk *) NULL) {
		((MemHeader *) c2->next)->prev = (MemHeader *) c2;
	    }
	    ((MemHeader *) c2)->prev = ((MemHeader *) c1)->prev;
	    if ((MemHeader *) c1 == hlist) {
		hlist = (MemHeader *) c2;
	    } else {
		((MemHeader *) c2)->prev->next = c2;
	    }
# endif
	    DynamicMem::free(c1);
	    c1 = c2;
	}
    } else {
	EC->fatal("bad pointer in m_realloc");
    }
# ifdef MEMDEBUG
    ((MemHeader *) c1)->file = file;
    ((MemHeader *) c1)->line = line;
# endif
    return (char *) c1 + MOFFSET;
}

/*
 * return TRUE if there is enough static memory left, or FALSE otherwise
 */
bool AllocImpl::check()
{
    return StaticMem::check();
}

/*
 * purge dynamic memory
 */
void AllocImpl::purge()
{
# ifdef DEBUG
    char *p;

    while (hlist != (MemHeader *) NULL) {
	char buf[160];
	size_t n, len;

	n = (hlist->size & SIZE_MASK) - MOFFSET;
	if (n >= DLIMIT) {
	    n -= SIZETSIZE;
	}
# ifdef MEMDEBUG
	snprintf(buf, sizeof(buf), "FREE(%08lx/%u), %s line %u:\012", /* LF */
		 (unsigned long) (hlist + 1), (unsigned int) n, hlist->file,
		 hlist->line);
# else
	snprintf(buf, sizeof(buf), "FREE(%08lx/%u):\012", /* LF */
		 (unsigned long) (hlist + 1), (unsigned int) n);
# endif
	if (n > 26) {
	    n = 26;
	}
	for (p = (char *) (hlist + 1); n > 0; --n, p++) {
	    len = strlen(buf);
	    if (*p >= ' ') {
		snprintf(buf + len, sizeof(buf) - len, " '%c", *p);
	    } else {
		snprintf(buf + len, sizeof(buf) - len, " %02x", UCHAR(*p));
	    }
	}
	EC->message("%s\012", buf);	/* LF */
	free((char *) (hlist + 1));
    }
# endif

    DynamicMem::purge();
    StaticMem::expand();
}

/*
 * return information about memory usage
 */
Alloc::Info *AllocImpl::info()
{
    static Info mstat;

    mstat.smemsize = StaticMem::memSize;
    mstat.smemused = StaticMem::memUsed;
    mstat.dmemsize = DynamicMem::memSize;
    mstat.dmemused = DynamicMem::memUsed;
    return &mstat;
}

/*
 * finish up memory manager
 */
void AllocImpl::finish()
{
    /* purge dynamic memory */
# ifdef DEBUG
    hlist = (MemHeader *) NULL;
# endif
    purge();

    StaticMem::finish();
    DynamicMem::finish();
}
