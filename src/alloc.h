/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2018 DGD Authors (see the commit log for details)
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
extern void  m_dynamic	();
extern void  m_static	();
extern bool  m_check	();
extern void  m_purge	();
extern void  m_finish	();

/*
 * inherit from this to use DGD's memory manager
 */
class Allocated {
public:
# ifdef DEBUG
    static void *operator new(size_t size, const char *file, int line) {
	return m_alloc(size, file, line);
    }
# else
    static void *operator new(size_t size) {
	return m_alloc(size);
    }
# endif

    static void operator delete(void *ptr) {
	m_free((char *) ptr);
    }
};

class ChunkAllocator {
private:
    virtual void *alloc() = 0;
    virtual void del(void *ptr) = 0;

    friend class ChunkAllocated;
};

/*
 * inherited by classes that are allocated in chunks
 */
class ChunkAllocated {
public:
    static void *operator new(size_t size, ChunkAllocator &chunk) {
	ChunkAllocated *item;

	/* ask chunk allocator for memory */
	item = (ChunkAllocated *) chunk.alloc();
	item->setChunk(&chunk);
	return item;
    }

    static void operator delete(void *ptr) {
	/* let the chunk allocator reuse this memory */
	((ChunkAllocated *) ptr)->chunk()->del(ptr);
    }

private:
    /*
     * save chunk allocator in instance (pre-constructor)
     */
    void setChunk(ChunkAllocator *c) {
	allocator = c;
    }

    /*
     * get chunk allocator from instance (post-destructor)
     */
    ChunkAllocator *chunk() {
	return allocator;
    }

    ChunkAllocator *allocator;	/* chunk allocator for this instance */
};

# ifdef DEBUG
# define _N_		new
# define new_F_		new
# define _N__F_(b, c)	new
# define _B_(a, b, c)	a ## _F_(b, c)
# define _A_(a, b, c)	_B_(a, b, c)
# define new		_A_(_N_, __FILE__, __LINE__)
# define chunknew(c)	_N_ (c)
# else
# define chunknew(c)	new (c)
# endif

template <class T, int CHUNK> class Chunk :
				    public Allocated, public ChunkAllocator {
public:
    Chunk() {
	chunk = (Tchunk *) NULL;
	flist = (Titem *) NULL;
	chunksize = 0;
    }

    virtual ~Chunk() {
	clean();
    }


    virtual bool item(T *item) {
	UNREFERENCED_PARAMETER(item);
	return TRUE;
    }

    /*
     * visit items in chunks
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
     * clean up item chunks
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
    /*
     * add new item to chunk
     */
    virtual void *alloc() {
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
     * remove item from chunk
     */
    virtual void del(void *ptr) {
	Titem *item;

	item = (Titem *) ptr;
	item->list = flist;
	flist = item;
    }

    struct Titem {
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

extern allocinfo *m_info ();
