# include "lex.h"
# include "macro.h"

/*
 * The macro handling routines. These use the hash table.
 */

# define MCHUNKSZ	32

typedef struct _mchunk_ {
    macro m[MCHUNKSZ];		/* macros */
    struct _mchunk_ *next;	/* next in list */
} mchunk;
static mchunk *mlist;		/* list of macro chunks */
static int mchunksz;		/* size of current macro chunk */
static macro *flist;		/* list of free macros */

static hashtab *mt;		/* macro hash table */

/*
 * NAME:	macro->init()
 * DESCRIPTION:	intiialize the macro table
 */
void mc_init()
{
    mt = ht_new(MACTABSZ, MACHASHSZ);
    mlist = (mchunk *) NULL;
    mchunksz = MCHUNKSZ;
    flist = (macro *) NULL;
}

/*
 * NAME:	macro->delete()
 * DESCRIPTION:	delete a macro in the macro table
 */
static void mc_delete(mac)
register macro *mac;
{
    FREE(mac->chain.name);
    if (mac->replace != (char *) NULL) {
	FREE(mac->replace);
    }
}

/*
 * NAME:	macro->clear()
 * DESCRIPTION:	clear the macro table
 */
void mc_clear()
{
    register mchunk *l, *f;

    ht_del(mt, mc_delete);

    for (l = mlist; l != (mchunk *) NULL; ) {
	f = l;
	l = l->next;
	FREE(f);
    }
}

/*
 * NAME:	macro->define()
 * DESCRIPTION:	define a macro
 */
void mc_define(name, replace, narg)
register char *name, *replace;
int narg;
{
    register macro **m;

    m = (macro **) ht_lookup(mt, name);
    if (*m != (macro *) NULL) {
	/* the macro already exists. */
	if ((*m)->replace != (char *) NULL) {
	    if ((*m)->narg != narg || strcmp((*m)->replace, replace) != 0) {
		lexwarning("macro %s redefined", name);
	    }
	    FREE((*m)->replace);
	}
    } else {
	if (flist != (macro *) NULL) {
	    /* get macro from free list */
	    *m = flist;
	    flist = (macro *) flist->chain.next;
	} else {
	    /* allocate new macro */
	    if (mchunksz == MCHUNKSZ) {
		register mchunk *l;

		l = ALLOC(mchunk, 1);
		l->next = mlist;
		mlist = l;
		mchunksz = 0;
	    }
	    *m = &mlist->m[mchunksz++];
	}
	(*m)->chain.next = (hte *) NULL;
	(*m)->chain.name = strcpy(ALLOC(char, strlen(name) + 1), name);
    }
    /* fill in macro */
    if (replace != (char *) NULL) {
	(*m)->replace = strcpy(ALLOC(char, strlen(replace) + 1), replace);
    } else {
	(*m)->replace = (char *) NULL;
    }
    (*m)->narg = narg;
}

/*
 * NAME:	macro->undef()
 * DESCRIPTION:	undefine a macro
 */
void mc_undef(name)
char *name;
{
    register macro **m, *mac;

    m = (macro **) ht_lookup(mt, name);
    if (*m != (macro *) NULL) {
	/* it really exists. */
	FREE((*m)->chain.name);
	if ((*m)->replace != (char *) NULL) {
	    FREE((*m)->replace);
	}
	mac = *m;
	*m = (macro *) mac->chain.next;
	/* put macro in free list */
	mac->chain.next = (hte *) flist;
	flist = mac;
    }
}

/*
 * NAME:	macro->lookup()
 * DESCRIPTION:	lookup a macro definition in the macro table. Return &NULL if
 *		the macro is not found.
 */
macro *mc_lookup(name)
char *name;
{
    return *(macro **) ht_lookup(mt, name);
}
