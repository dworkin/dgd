/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

# ifdef DEBUG

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size),    \
					   __FILE__, __LINE__)))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2), \
					     __FILE__, __LINE__)))
extern char *m_alloc	(size_t, const char*, int);
extern char *m_realloc	(char*, size_t, size_t, const char*, int);

# else

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size))))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2))))
extern char *m_alloc	(size_t);
extern char *m_realloc	(char*, size_t, size_t);

# endif

# define FREE(mem)	m_free((char *) (mem))

extern void  m_init	(size_t, size_t);
extern void  m_free	(char*);
extern void  m_dynamic	(void);
extern void  m_static	(void);
extern bool  m_check	(void);
extern void  m_purge	(void);
extern void  m_finish	(void);

class Allocated {
public:
    /*
     * NAME:		new()
     * DESCRIPTION:	override new for class Allocated
     */
    static void *operator new(std::size_t size) {
	return ALLOC(char, size);
    }

    /*
     * NAME:		delete()
     * DESCRIPTION:	override delete for class Allocated
     */
    static void operator delete(void *ptr) {
	FREE(ptr);
    }
};

template <class T, int CHUNK> class Chunk : public Allocated {
public:
    /*
     * NAME:		Chunk()
     * DESCRIPTION:	constructor
     */
    Chunk() {
	chunk = (Tchunk *) NULL;
	flist = (Titem *) NULL;
	chunksize = 0;
    }

    virtual ~Chunk() {
	clean();
    }

    /*
     * NAME:		alloc()
     * DESCRIPTION:	add new item to chunk
     */
    T *alloc() {
	if (flist == NULL) {
	    if (chunk == NULL || chunksize == 0) {
		Tchunk *b;

		b = ALLOC(Tchunk, 1);
		b->prev = chunk;
		chunk = b;
		chunksize = CHUNK;
	    }
	    return &chunk->items[--chunksize].item;
	} else {
	    Titem *item;

	    item = flist;
	    flist = item->list;
	    return &item->item;
	}
    }

    /*
     * NAME:		del()
     * DESCRITION:	remove item from chunk
     */
    void del(T *ptr) {
	Titem *item;

	item = (Titem *) ptr;
	item->list = flist;
	flist = item;
    }

    virtual bool item(T *item) {
	return TRUE;
    }

    /*
     * NAME:		items()
     * DESCRIPTION:	visit items in chunks
     */
    void items() {
	Tchunk *b;
	int i;

	i = chunksize;
	for (b = chunk; b != NULL; b = b->prev) {
	    while (i < CHUNK) {
		if (!item(&b->items[i].item)) {
		    return;
		}
		i++;
	    }
	    i = 0;
	}
    }

    /*
     * NAME:		clean()
     * DESCRIPTION:	clean up item chunks
     */
    void clean() {
	while (chunk != NULL) {
	    Tchunk *prev;

	    prev = chunk->prev;
	    FREE(chunk);
	    chunk = prev;
	}
	flist = NULL;
	chunksize = 0;
    }

private:
    union Titem {
	T item;			/* item */
	Titem *list;		/* next in the free list */
    };
    struct Tchunk {
	Titem items[CHUNK];	/* array of allocated items */
	Tchunk *prev;		/* previous chunk of items */
    };

    Tchunk *chunk;		/* chunk of items */
    Titem *flist;		/* list of free items */
    int chunksize;		/* size of chunk */
};

struct allocinfo {
    size_t smemsize;	/* static memory size */
    size_t smemused;	/* static memory used */
    size_t dmemsize;	/* dynamic memory used */
    size_t dmemused;	/* dynamic memory used */
};

extern allocinfo *m_info (void);
