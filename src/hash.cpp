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
    '\001', '\127', '\061', '\014', '\260', '\262', '\146', '\246',
    '\171', '\301', '\006', '\124', '\371', '\346', '\054', '\243',
    '\016', '\305', '\325', '\265', '\241', '\125', '\332', '\120',
    '\100', '\357', '\030', '\342', '\354', '\216', '\046', '\310',
    '\156', '\261', '\150', '\147', '\215', '\375', '\377', '\062',
    '\115', '\145', '\121', '\022', '\055', '\140', '\037', '\336',
    '\031', '\153', '\276', '\106', '\126', '\355', '\360', '\042',
    '\110', '\362', '\024', '\326', '\364', '\343', '\225', '\353',
    '\141', '\352', '\071', '\026', '\074', '\372', '\122', '\257',
    '\320', '\005', '\177', '\307', '\157', '\076', '\207', '\370',
    '\256', '\251', '\323', '\072', '\102', '\232', '\152', '\303',
    '\365', '\253', '\021', '\273', '\266', '\263', '\000', '\363',
    '\204', '\070', '\224', '\113', '\200', '\205', '\236', '\144',
    '\202', '\176', '\133', '\015', '\231', '\366', '\330', '\333',
    '\167', '\104', '\337', '\116', '\123', '\130', '\311', '\143',
    '\172', '\013', '\134', '\040', '\210', '\162', '\064', '\012',
    '\212', '\036', '\060', '\267', '\234', '\043', '\075', '\032',
    '\217', '\112', '\373', '\136', '\201', '\242', '\077', '\230',
    '\252', '\007', '\163', '\247', '\361', '\316', '\003', '\226',
    '\067', '\073', '\227', '\334', '\132', '\065', '\027', '\203',
    '\175', '\255', '\017', '\356', '\117', '\137', '\131', '\020',
    '\151', '\211', '\341', '\340', '\331', '\240', '\045', '\173',
    '\166', '\111', '\002', '\235', '\056', '\164', '\011', '\221',
    '\206', '\344', '\317', '\324', '\312', '\327', '\105', '\345',
    '\033', '\274', '\103', '\174', '\250', '\374', '\052', '\004',
    '\035', '\154', '\025', '\367', '\023', '\315', '\047', '\313',
    '\351', '\050', '\272', '\223', '\306', '\300', '\233', '\041',
    '\244', '\277', '\142', '\314', '\245', '\264', '\165', '\114',
    '\214', '\044', '\322', '\254', '\051', '\066', '\237', '\010',
    '\271', '\350', '\161', '\304', '\347', '\057', '\222', '\170',
    '\063', '\101', '\034', '\220', '\376', '\335', '\135', '\275',
    '\302', '\213', '\160', '\053', '\107', '\155', '\270', '\321',
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
