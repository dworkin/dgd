/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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

class Array : public ChunkAllocated {
public:
    class Backup;			/* array backup chunk */

    Array(unsigned short size);
    Array() {
	prev = next = this;		/* alist sentinel */
    }
    virtual ~Array() { }

    void ref() {
	refCount++;
    }
    void del();
    void freelist();
    virtual bool trim();
    virtual void canonicalize();
    Uint put(Uint idx);
    void backup(Backup **ac);
    virtual Array *add(Dataspace *data, Array *a2);
    virtual Array *sub(Dataspace *data, Array *a2);
    virtual Array *intersect(Dataspace *data, Array *a2);
    Array *setAdd(Dataspace *data, Array *a2);
    Array *setXAdd(Dataspace *data, Array *a2);
    unsigned short index(LPCint l);
    void checkRange(LPCint l1, LPCint l2);
    Array *range(Dataspace *data, LPCint l1, LPCint l2);

    static void init(unsigned int size);
    static Array *alloc(unsigned short size);
    static Array *create(Dataspace *data, LPCint size);
    static Array *createNil(Dataspace *data, LPCint size);
    static void freeall();
    static void merge();
    static void clear();
    static void commit(Backup **ac, Dataplane *plane, bool merge);
    static void discard(Backup **ac);

    unsigned short size;		/* number of elements */
    Uint refCount;			/* number of references */
    Uint tag;				/* used in sorting */
    Uint objDestrCount;			/* last destructed object count */
    Value *elts;			/* elements */
    struct ArrRef *primary;		/* primary reference */
    Array *prev, *next;			/* per-object linked list */

protected:
    virtual void deepDelete();
    virtual void shallowDelete();

    friend class ArrBak;
};

class Mapping : public Array {
public:
    Mapping(unsigned short size);
    virtual ~Mapping() { }

    void sort();
    virtual bool trim();
    virtual void canonicalize();
    unsigned short msize(Dataspace *data);
    virtual Array *add(Dataspace *data, Array *a2);
    virtual Array *sub(Dataspace *data, Array *a2);
    virtual Array *intersect(Dataspace *data, Array *a2);
    Value *index(Dataspace *data, Value *val, Value *elt, Value *verify);
    Mapping *range(Dataspace *data, Value *v1, Value *v2);
    Array *indices(Dataspace *data);
    Array *values(Dataspace *data);

    static Mapping *alloc(unsigned short size);
    static Mapping *create(Dataspace *data, LPCint size);

protected:
    virtual void deepDelete();
    virtual void shallowDelete();

private:
    void dehash(Dataspace *data, bool clean);
    void compact(Dataspace *data);

    bool hashmod;			/* hashed part contains new elements */
    class MapHash *hashed;		/* hashed mapping elements */

    friend class MapHash;
};

class LWO : public Array {
public:
    LWO(unsigned short size) : Array(size) { }
    virtual ~LWO() { }

    LWO *copy(Dataspace *data);

    static LWO *alloc(unsigned short size);
    static LWO *create(Dataspace *data, Object *obj);
};
