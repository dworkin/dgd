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
