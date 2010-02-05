/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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

typedef struct _hte_ {
    struct _hte_ *next;	/* next entry in hash table */
    char *name;		/* string to use in hashing */
} hte;

typedef struct {
    Uint size;			/* size of hash table (power of two) */
    unsigned short maxlen;	/* max length of string to be used in hashing */
    bool mem;			/* \0-terminated string or raw memory? */
    hte *table[1];		/* hash table entries */
} hashtab;

extern char		strhashtab[];
extern unsigned short	hashstr		P((char*, unsigned int));
extern unsigned short	hashmem		P((char*, unsigned int));

extern hashtab	       *ht_new		P((unsigned int, unsigned int, int));
extern void		ht_del		P((hashtab*));
extern hte	      **ht_lookup	P((hashtab*, char*, int));

# endif /* H_HASH */
