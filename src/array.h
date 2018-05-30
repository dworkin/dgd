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

class Array : public ChunkAllocated {
public:
    class Backup;			/* array backup chunk */

    Array(unsigned short size);
    ~Array();

    void ref() {
	refCount++;
    }
    void del();
    void freelist();
    Uint put(Uint idx);
    void backup(Backup **ac);
    Array *add(Dataspace *data, Array *a2);
    Array *sub(Dataspace *data, Array *a2);
    Array *intersect(Dataspace *data, Array *a2);
    Array *setadd(Dataspace *data, Array *a2);
    Array *setxadd(Dataspace *data, Array *a2);
    unsigned short index(long l);
    void checkRange(long l1, long l2);
    Array *range(Dataspace *data, long l1, long l2);

    void mapSort();
    void mapRemoveHash();
    void mapCompact(Dataspace *data);
    unsigned short mapSize(Dataspace *data);
    Array *mapAdd(Dataspace *data, Array *m2);
    Array *mapSub(Dataspace *data, Array *a2);
    Array *mapIntersect(Dataspace *data, Array *a2);
    Value *mapIndex(Dataspace *data, Value *val, Value *elt, Value *verify);
    Array *mapRange(Dataspace *data, Value *v1, Value *v2);
    Array *mapIndices(Dataspace *data);
    Array *mapValues(Dataspace *data);

    Array *lwoCopy(Dataspace *data);

    static void init(unsigned int size);
    static Array *alloc(unsigned int size);
    static Array *create(Dataspace *data, long size);
    static Array *createNil(Dataspace *data, long size);
    static void freeall();
    static void merge();
    static void clear();
    static void commit(Backup **ac, Dataplane *plane, bool merge);
    static void discard(Backup **ac);

    static Array *mapCreate(Dataspace *data, long size);

    static Array *lwoCreate(Dataspace *data, Object *obj);

    unsigned short size;		/* number of elements */
    bool hashmod;			/* hashed part contains new elements */
    Uint refCount;			/* number of references */
    Uint tag;				/* used in sorting */
    Uint odcount;			/* last destructed object count */
    Value *elts;			/* elements */
    class MapHash *hashed;		/* hashed mapping elements */
    struct arrref *primary;		/* primary reference */
    Array *prev, *next;			/* per-object linked list */

private:
    void mapDehash(Dataspace *data, bool clean);
};
