# include "dgd.h"
# include "hash.h"

/*
 * Generic string hash table.
 */

char strhashtab[] = {
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
 * NAME:	hashtab->new()
 * DESCRIPTION:	create a hashtable of size "size", where "maxlen" characters
 *		of each string are significant
 */
hashtab *ht_new(size, maxlen, mem)
register unsigned int size;
unsigned int maxlen;
int mem;
{
    register hashtab *ht;

    ht = (hashtab *) ALLOC(char, sizeof(hashtab) + sizeof(hte*) * (size - 1));
    ht->size = size;
    ht->maxlen = maxlen;
    ht->mem = mem;
    memset(ht->table, '\0', size * sizeof(hte*));

    return ht;
}

/*
 * NAME:	hashtab->del()
 * DESCRIPTION:	delete a hash table
 */
void ht_del(ht)
hashtab *ht;
{
    FREE(ht);
}

/*
 * NAME:	hashstr()
 * DESCRIPTION:	Hash string s, considering at most len characters. Return
 *		an unsigned modulo size.
 *		Based on Peter K. Pearson's article in CACM 33-6, pp 677.
 */
unsigned short hashstr(s, len)
register char *s;
register unsigned int len;
{
    register char h, l;

    h = l = 0;
    while (*s != '\0' && len > 0) {
	h = l;
	l = strhashtab[UCHAR(l ^ *s++)];
	--len;
    }
    return (unsigned short) ((UCHAR(h) << 8) | UCHAR(l));
}

/*
 * NAME:	hashmem()
 * DESCRIPTION:	hash memory
 */
unsigned short hashmem(s, len)
register char *s;
register unsigned int len;
{
    register char h, l;

    h = l = 0;
    while (len > 0) {
	h = l;
	l = strhashtab[UCHAR(l ^ *s++)];
	--len;
    }
    return (unsigned short) ((UCHAR(h) << 8) | UCHAR(l));
}

/*
 * NAME:	hashtab->lookup()
 * DESCRIPTION:	lookup a name in a hashtable, return the address of the entry
 *		or &NULL if none found
 */
hte **ht_lookup(ht, name, move)
hashtab *ht;
register char *name;
int move;
{
    register hte **first, **e, *next;

    if (ht->mem) {
	first = e = &(ht->table[hashmem(name, ht->maxlen) % ht->size]);
	while (*e != (hte *) NULL) {
	    if (memcmp((*e)->name, name, ht->maxlen) == 0) {
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
	first = e = &(ht->table[hashstr(name, ht->maxlen) % ht->size]);
	while (*e != (hte *) NULL) {
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
