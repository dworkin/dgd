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
public:
    struct Header {
	union {
	    ChunkAllocator *chunk;	/* chunk handler */
	    Header *list;		/* next in free list */
	};
    };

    virtual ~ChunkAllocator() { }

private:
    virtual Header *alloc() = 0;
    virtual void del(Header *ptr) = 0;

    friend class ChunkAllocated;
};

/*
 * Inherited by classes that are allocated in chunks.
 */
class ChunkAllocated {
public:
    static void *operator new(size_t size, ChunkAllocator &chunk) {
	ChunkAllocator::Header *item;

	/* ask chunk allocator for memory */
	item = chunk.alloc();
	item->chunk = &chunk;
	return item + 1;
    }

    static void operator delete(void *ptr) {
	/* let the chunk allocator reuse this memory */
	ChunkAllocator::Header *item;

	item = (ChunkAllocator::Header *) ptr - 1;
	item->chunk->del(item);
    }
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
    struct Titem : public ChunkAllocator::Header {
	T item;			/* item */
    };
    struct Tchunk {
	Tchunk *prev;		/* previous chunk of items */
	Titem items[CHUNK];	/* array of allocated items */
    };

    /*
     * add new item to chunk
     */
    virtual Header *alloc() {
	if (flist == NULL) {
	    if (chunk == NULL || chunksize == 0) {
		Tchunk *b;

		b = ALLOC(Tchunk, 1);
		b->prev = chunk;
		chunk = b;
		chunksize = CHUNK;
	    }
	    return &chunk->items[--chunksize];
	} else {
	    Titem *item;

	    item = flist;
	    flist = (Titem *) item->list;
	    return item;
	}
    }

    /*
     * remove item from chunk
     */
    virtual void del(Header *ptr) {
	Titem *item;

	item = (Titem *) ptr;
	item->list = flist;
	flist = item;
    }

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
