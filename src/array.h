typedef struct _array_ {
    unsigned short size;		/* number of elements */
    uindex ref;				/* number of references */
    long tag;				/* used in sorting */
    value *elts;			/* elements */
    struct _maphash_ *hashed;		/* hashed mapping elements */
    struct _arrref_ *primary;		/* primary reference */
    struct _array_ *prev, *next;	/* double linked list of all arrays */
} array;

extern void		arr_init	P((int));
extern array	       *arr_new		P((long));
# define arr_ref(a)	((a)->ref++)
extern void		arr_del		P((array*));
extern void		arr_freeall	P((void));
extern int		arr_maxsize	P((void));

extern uindex		arr_put		P((array*));
extern void		arr_clear	P((void));

extern array	       *arr_add		P((array*, array*));
extern array	       *arr_sub		P((array*, array*));
extern array	       *arr_intersect	P((array*, array*));
extern unsigned short	arr_index	P((array*, Int));
extern array	       *arr_range	P((array*, Int, Int));

extern array	       *map_new		P((long));
extern void		map_sort	P((array*));
extern unsigned short	map_size	P((array*));
extern void		map_compact	P((array*));
extern array	       *map_add		P((array*, array*));
extern value	       *map_index	P((array*, value*, value*));
extern array	       *map_indices	P((array*));
extern array	       *map_values	P((array*));
