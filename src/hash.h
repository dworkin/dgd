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

# ifndef H_HASH
# define H_HASH

extern class Hash *HM;

class Hash {
public:
    struct Entry {
	Entry *next;		/* next entry in hash table */
	const char *name;	/* string to use in hashing */
    };

    class Hashtab : public Allocated {
    public:
	/*
	 * create a new hashtable of size "size", where "maxlen" characters
	 * of each string are significant
	 */
	Hashtab(unsigned int size, unsigned int maxlen, bool mem) :
	    size(size), maxlen(maxlen), mem(mem) {
	    table = ALLOC(Entry*, size);
	    memset(table, '\0', size * sizeof(Entry*));
	}

	/*
	 * delete a hash table
	 */
	virtual ~Hashtab() {
	    FREE(table);
	}

	/*
	 * lookup a name in a hashtable, return the address of the entry
	 * or &NULL if none found
	 */
	Entry **lookup(const char *name, bool move) {
	    Entry **first, **e, *next;

	    if (mem) {
		first = e = &(table[HM->hashmem(name, maxlen) % size]);
		while (*e != (Entry *) NULL) {
		    if (memcmp((*e)->name, name, maxlen) == 0) {
			if (move && e != first) {
			    /* move to first position */
			    next = (*e)->next;
			    (*e)->next = *first;
			    *first = *e;
			    *e = next;
			    return first;
			}
			break;
		    }
		    e = &((*e)->next);
		}
	    } else {
		first = e = &(table[HM->hashstr(name, maxlen) % size]);
		while (*e != (Entry *) NULL) {
		    if (strcmp((*e)->name, name) == 0) {
			if (move && e != first) {
			    /* move to first position */
			    next = (*e)->next;
			    (*e)->next = *first;
			    *first = *e;
			    *e = next;
			    return first;
			}
			break;
		    }
		    e = &((*e)->next);
		}
	    }
	    return e;
	}

	Uint size;		/* size of hash table */
	Entry **table;		/* hash table entries */

    private:
	unsigned short maxlen;	/* max length of string to be used in hashing */
	bool mem;		/* \0-terminated string or raw memory? */
    };

    /*
     * hashtable factory
     */
    virtual Hashtab *create(unsigned int size, unsigned int maxlen, bool mem) {
	return new Hashtab(size, maxlen, mem);
    }

    virtual unsigned char hashchar(unsigned char c) {
	return c;
    }

    /*
     * hash string
     */
    virtual unsigned short hashstr(const char *str, unsigned int len) {
	unsigned char h, l;

	h = l = 0;
	while (*str != '\0' && len > 0) {
	    h = l;
	    l = hashchar(l ^ (unsigned char) *str++);
	    --len;
	}
	return (unsigned short) ((h << 8) | l);
    }

    /*
     * hash memory
     */
    virtual unsigned short hashmem(const char *mem, unsigned int len) {
	unsigned char h, l;

	h = l = 0;
	while (len > 0) {
	    h = l;
	    l = hashchar(l ^ (unsigned char) *mem++);
	    --len;
	}
	return (unsigned short) ((h << 8) | l);
    }
};

class HashImpl : public Hash {
public:
    virtual unsigned char hashchar(unsigned char c);
};

# endif /* H_HASH */
