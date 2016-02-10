/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2016 DGD Authors (see the commit log for details)
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

struct Hte {
    Hte *next;		/* next entry in hash table */
    const char *name;	/* string to use in hashing */
};

class Hashtab : public Allocated {
public:
    Hashtab(unsigned int size, unsigned int maxlen, bool mem);
    ~Hashtab();

    Hte **lookup(const char*, bool);

    static unsigned char hashchar(char c) {
	return tab[(unsigned char) c];
    }
    static unsigned short hashstr(const char *str, unsigned int len);
    static unsigned short hashmem(const char *mem, unsigned int len);

    Hte **table;		/* hash table entries */
    Uint size;			/* size of hash table (power of two) */

private:
    unsigned short maxlen;	/* max length of string to be used in hashing */
    bool mem;			/* \0-terminated string or raw memory? */
    static unsigned char tab[256];
};

# endif /* H_HASH */
