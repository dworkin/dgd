typedef struct _svalue_ {
    short type;			/* object, number, string, array */
    uindex oindex;		/* index in object table */
    union {
	Int number;		/* number */
	Uint string;		/* string */
	Uint objcnt;		/* object creation count */
	Uint array;		/* array */
    } u;
} svalue;

# define SV_LAYOUT	"sui"

typedef struct _sarray_ {
    Uint index;			/* index in array value table */
    unsigned short size;	/* size of array */
    Uint ref;			/* refcount */
    Uint tag;			/* unique value for each array */
} sarray;

# define SA_LAYOUT	"isii"

typedef struct _sstring_ {
    Uint index;			/* index in string text table */
    ssizet len;			/* length of string */
    Uint ref;			/* refcount */
} sstring;

# define SS_LAYOUT	"iti"


extern void		sd_init			P((struct _mempool_*));

extern control	       *sd_load_control		P((lpcenv*, object*));
extern dataspace       *sd_load_dataspace	P((lpcenv*, object*));

extern sector		sd_get_csize		P((struct _scontrol_*));
extern char	       *sd_get_prog		P((struct _scontrol_*, Uint*));
extern dstrconst       *sd_get_strconsts	P((struct _scontrol_*));
extern char	       *sd_get_ctext		P((struct _scontrol_*, Uint*));
extern dfuncdef        *sd_get_funcdefs		P((struct _scontrol_*));
extern dvardef	       *sd_get_vardefs		P((struct _scontrol_*));
extern char	       *sd_get_funcalls		P((struct _scontrol_*));
extern dsymbol	       *sd_get_symbols		P((struct _scontrol_*));

extern sector		sd_get_dsize		P((struct _sdataspace_*));
extern struct _svalue_ *sd_get_svariables	P((struct _sdataspace_*));
extern struct _sstring_*sd_get_sstrings		P((struct _sdataspace_*));
extern char	       *sd_get_dtext		P((struct _sdataspace_*,
						   Uint*));
extern struct _sarray_ *sd_get_sarrays		P((struct _sdataspace_*));
extern struct _svalue_ *sd_get_selts		P((struct _sdataspace_*));
extern void		sd_load_callouts	P((dataspace*));

extern void		sd_save_control		P((lpcenv*, control*));
extern bool		sd_save_dataspace	P((dataspace*, int, Uint*));
extern void		sd_conv_control		P((unsigned int));
extern void		sd_conv_dataspace	P((object*, Uint*));

extern void		sd_del_scontrol		P((struct _scontrol_*));
extern void		sd_del_sdataspace	P((struct _sdataspace_*));
extern void		sd_free_scontrol	P((struct _scontrol_*));
extern void		sd_free_sdataspace	P((struct _sdataspace_*));
