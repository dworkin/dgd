typedef struct _array_ {
    unsigned short size;		/* number of elements */
    uindex ref;				/* number of references */
    long tag;				/* used in sorting */
    struct _arrref_ *primary;		/* primary reference */
    struct _array_ *prev, *next;	/* double linked list of all arrays */
    value *elts;			/* elements in array */
} array;

extern void	arr_init	P((void));
extern array   *arr_new		P((long));
# define arr_ref(a)	((a)->ref++)
extern void	arr_del		P((array*));
extern void	arr_freeall	P((void));

extern uindex	arr_put		P((array*));
extern void	arr_clear	P((void));

extern array   *arr_add		P((array*, array*));
extern array   *arr_sub		P((array*, array*));
extern array   *arr_intersect	P((array*, array*));
extern int	arr_index	P((array*, Int));
extern array   *arr_range	P((array*, Int, Int));

extern array   *map_new		P((long));
extern void	map_sort	P((array*));
extern array   *map_make	P((array*, array*));
extern array   *map_add		P((array*, array*));
extern int	map_index	P((array*, value*, bool));
extern void	map_compact	P((array*));
extern array   *map_indices	P((array*));
extern array   *map_values	P((array*));
