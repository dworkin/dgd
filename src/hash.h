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

# ifndef H_HASH
# define H_HASH

class Hashtab : public Allocated {
public:
    static Hashtab *create(unsigned int size, unsigned int maxlen, bool mem);

    static unsigned char hashchar(char c) {
	return tab[(unsigned char) c];
    }
    static unsigned short hashstr(const char *str, unsigned int len);
    static unsigned short hashmem(const char *mem, unsigned int len);

    struct Entry : public Allocated {
	Entry *next;		/* next entry in hash table */
	const char *name;	/* string to use in hashing */
    };

    virtual Entry **table() = 0;
    virtual Uint size() = 0;
    virtual Entry **lookup(const char*, bool) = 0;

private:
    static unsigned char tab[256];
};

class HashtabImpl : public Hashtab {
public:
    HashtabImpl(unsigned int size, unsigned int maxlen, bool mem);
    virtual ~HashtabImpl();

    virtual Entry **table() {
	return m_table;
    }

    virtual Uint size() {
	return m_size;
    }

    virtual Entry **lookup(const char *name, bool move);

private:
    Uint m_size;		/* size of hash table (power of two) */
    unsigned short m_maxlen;	/* max length of string to be used in hashing */
    bool m_mem;			/* \0-terminated string or raw memory? */
    Entry **m_table;		/* hash table entries */
};

# endif /* H_HASH */
