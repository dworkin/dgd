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

# include "dgd.h"
# include "hash.h"

/*
 * Generic string hash table.
 */

unsigned char Hashtab::tab[256] = {
    u'\001', u'\127', u'\061', u'\014', u'\260', u'\262', u'\146', u'\246',
    u'\171', u'\301', u'\006', u'\124', u'\371', u'\346', u'\054', u'\243',
    u'\016', u'\305', u'\325', u'\265', u'\241', u'\125', u'\332', u'\120',
    u'\100', u'\357', u'\030', u'\342', u'\354', u'\216', u'\046', u'\310',
    u'\156', u'\261', u'\150', u'\147', u'\215', u'\375', u'\377', u'\062',
    u'\115', u'\145', u'\121', u'\022', u'\055', u'\140', u'\037', u'\336',
    u'\031', u'\153', u'\276', u'\106', u'\126', u'\355', u'\360', u'\042',
    u'\110', u'\362', u'\024', u'\326', u'\364', u'\343', u'\225', u'\353',
    u'\141', u'\352', u'\071', u'\026', u'\074', u'\372', u'\122', u'\257',
    u'\320', u'\005', u'\177', u'\307', u'\157', u'\076', u'\207', u'\370',
    u'\256', u'\251', u'\323', u'\072', u'\102', u'\232', u'\152', u'\303',
    u'\365', u'\253', u'\021', u'\273', u'\266', u'\263', u'\000', u'\363',
    u'\204', u'\070', u'\224', u'\113', u'\200', u'\205', u'\236', u'\144',
    u'\202', u'\176', u'\133', u'\015', u'\231', u'\366', u'\330', u'\333',
    u'\167', u'\104', u'\337', u'\116', u'\123', u'\130', u'\311', u'\143',
    u'\172', u'\013', u'\134', u'\040', u'\210', u'\162', u'\064', u'\012',
    u'\212', u'\036', u'\060', u'\267', u'\234', u'\043', u'\075', u'\032',
    u'\217', u'\112', u'\373', u'\136', u'\201', u'\242', u'\077', u'\230',
    u'\252', u'\007', u'\163', u'\247', u'\361', u'\316', u'\003', u'\226',
    u'\067', u'\073', u'\227', u'\334', u'\132', u'\065', u'\027', u'\203',
    u'\175', u'\255', u'\017', u'\356', u'\117', u'\137', u'\131', u'\020',
    u'\151', u'\211', u'\341', u'\340', u'\331', u'\240', u'\045', u'\173',
    u'\166', u'\111', u'\002', u'\235', u'\056', u'\164', u'\011', u'\221',
    u'\206', u'\344', u'\317', u'\324', u'\312', u'\327', u'\105', u'\345',
    u'\033', u'\274', u'\103', u'\174', u'\250', u'\374', u'\052', u'\004',
    u'\035', u'\154', u'\025', u'\367', u'\023', u'\315', u'\047', u'\313',
    u'\351', u'\050', u'\272', u'\223', u'\306', u'\300', u'\233', u'\041',
    u'\244', u'\277', u'\142', u'\314', u'\245', u'\264', u'\165', u'\114',
    u'\214', u'\044', u'\322', u'\254', u'\051', u'\066', u'\237', u'\010',
    u'\271', u'\350', u'\161', u'\304', u'\347', u'\057', u'\222', u'\170',
    u'\063', u'\101', u'\034', u'\220', u'\376', u'\335', u'\135', u'\275',
    u'\302', u'\213', u'\160', u'\053', u'\107', u'\155', u'\270', u'\321',
};

/*
 * NAME:	Hashtab::create()
 * DESCRIPTION:	hashtable factory
 */
Hashtab *Hashtab::create(unsigned int size, unsigned int maxlen, bool mem)
{
    return new HashtabImpl(size, maxlen, mem);
}

/*
 * NAME:	Hashtab::hashstr()
 * DESCRIPTION:	Hash string s, considering at most len characters. Return
 *		an unsigned modulo size.
 *		Based on Peter K. Pearson's article in CACM 33-6, pp 677.
 */
unsigned short Hashtab::hashstr(const char *str, unsigned int len)
{
    unsigned char h, l;

    h = l = 0;
    while (*str != '\0' && len > 0) {
	h = l;
	l = tab[l ^ (unsigned char) *str++];
	--len;
    }
    return (unsigned short) ((h << 8) | l);
}

/*
 * NAME:	Hashtab::hashmem()
 * DESCRIPTION:	hash memory
 */
unsigned short Hashtab::hashmem(const char *mem, unsigned int len)
{
    unsigned char h, l;

    h = l = 0;
    while (len > 0) {
	h = l;
	l = tab[l ^ (unsigned char) *mem++];
	--len;
    }
    return (unsigned short) ((h << 8) | l);
}


/*
 * NAME:	HashtabImpl()
 * DESCRIPTION:	create a new hashtable of size "size", where "maxlen" characters
 *		of each string are significant
 */
HashtabImpl::HashtabImpl(unsigned int size, unsigned int maxlen, bool mem)
{
    m_size = size;
    m_maxlen = maxlen;
    m_mem = mem;
    m_table = ALLOC(Entry*, size);
    memset(m_table, '\0', size * sizeof(Entry*));
}

/*
 * NAME:	~HashtabImpl()
 * DESCRIPTION:	delete a hash table
 */
HashtabImpl::~HashtabImpl()
{
    FREE(m_table);
}

/*
 * NAME:	HashtabImpl::lookup()
 * DESCRIPTION:	lookup a name in a hashtable, return the address of the entry
 *		or &NULL if none found
 */
Hashtab::Entry **HashtabImpl::lookup(const char *name, bool move)
{
    Entry **first, **e, *next;

    if (m_mem) {
	first = e = &(m_table[hashmem(name, m_maxlen) % m_size]);
	while (*e != (Entry *) NULL) {
	    if (memcmp((*e)->name, name, m_maxlen) == 0) {
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
	first = e = &(m_table[hashstr(name, m_maxlen) % m_size]);
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
