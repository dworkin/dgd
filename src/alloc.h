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

class Alloc {
public:
    struct Info {
	size_t smemsize;	/* static memory size */
	size_t smemused;	/* static memory used */
	size_t dmemsize;	/* dynamic memory used */
	size_t dmemused;	/* dynamic memory used */
    };

    virtual void init(size_t staticSize, size_t dynamicSize) {
	UNREFERENCED_PARAMETER(staticSize);
	UNREFERENCED_PARAMETER(dynamicSize);
    }
    virtual void finish() { }

# ifdef MEMDEBUG

# define ALLOC(type, size)						      \
			((type *) (MM->alloc(sizeof(type) * (size_t) (size),  \
						__FILE__, __LINE__)))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (MM->realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2), \
					     __FILE__, __LINE__)))
    virtual char *alloc(size_t size, const char *file, int line) {
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	return (char *) std::malloc(size);
    }
    virtual char *realloc(char *mem, size_t size1, size_t size2,
			  const char *file, int line) {
	UNREFERENCED_PARAMETER(size1);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	return (char *) std::realloc(mem, size2);
    }

# else

# define ALLOC(type, size)						      \
			((type *) (MM->alloc(sizeof(type) * (size_t)(size))))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (MM->realloc((char *) (mem),	 	      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2))))
    virtual char *alloc(size_t size) {
	return (char *) std::malloc(size);
    }
    virtual char *realloc(char *mem, size_t size1, size_t size2) {
	UNREFERENCED_PARAMETER(size1);
	return (char *) std::realloc(mem, size2);
    }

# endif

# define FREE(mem)	MM->free((char *) (mem))

    virtual void free(char *mem) {
	std::free(mem);
    }

    virtual void dynamicMode() { }
    virtual void staticMode() { }

    virtual Info *info() {
	return (Info *) NULL;
    }
    virtual bool check() {
	return TRUE;
    }
    virtual void purge() { }
};

class AllocImpl : public Alloc {
public:
    virtual void init(size_t staticSize, size_t dynamicSize);
    virtual void finish();
# ifdef MEMDEBUG
    virtual char *alloc(size_t size, const char *file, int line);
    virtual char *realloc(char *mem, size_t size1, size_t size2,
			  const char *file, int line);
# else
    virtual char *alloc(size_t size);
    virtual char *realloc(char *mem, size_t size1, size_t size2);
# endif
    virtual void free(char *mem);
    virtual void dynamicMode();
    virtual void staticMode();
    virtual Info *info();
    virtual bool check();
    virtual void purge();
};

extern Alloc *MM;

/*
 * inherit from this to use DGD's memory manager
 */
class Allocated {
public:
# ifdef MEMDEBUG
    static void *operator new(size_t size, const char *file, int line) {
	return MM->alloc(size, file, line);
    }
# else
    static void *operator new(size_t size) {
	return MM->alloc(size);
    }
# endif

    static void operator delete(void *ptr) {
	MM->free((char *) ptr);
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

	UNREFERENCED_PARAMETER(size);

	/* ask chunk allocator for memory */
	item = chunk.alloc();
	item->chunk = &chunk;
	return item + 1;
    }

    static void operator delete(void *ptr) {
	ChunkAllocator::Header *item;

	/* let the chunk allocator reuse this memory */
	item = (ChunkAllocator::Header *) ptr - 1;
	item->chunk->del(item);
    }

    static void operator delete(void *ptr, ChunkAllocator &chunk) {
	UNREFERENCED_PARAMETER(chunk);

	/* exception thrown in constructor */
	operator delete(ptr);
    }
};

# ifdef MEMDEBUG
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
		if (b->items[i].chunk == this && !item(&b->items[i].item)) {
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
# ifdef DEBUG
	memset((void *) item, '\xdd', sizeof(Titem));
# endif
	item->list = flist;
	flist = item;
    }

    Tchunk *chunk;		/* chunk of items */
    Titem *flist;		/* list of free items */
    int chunksize;		/* size of chunk */
};
