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


extern void		sd_init			(struct _mempool_*);

extern control	       *sd_load_control		(lpcenv*, object*);
extern dataspace       *sd_load_dataspace	(lpcenv*, object*);

extern sector		sd_get_csize		(struct _scontrol_*);
extern char	       *sd_get_prog		(struct _scontrol_*, Uint*);
extern dstrconst       *sd_get_strconsts	(struct _scontrol_*);
extern char	       *sd_get_ctext		(struct _scontrol_*, Uint*);
extern dfuncdef        *sd_get_funcdefs		(struct _scontrol_*);
extern dvardef	       *sd_get_vardefs		(struct _scontrol_*);
extern char	       *sd_get_funcalls		(struct _scontrol_*);
extern dsymbol	       *sd_get_symbols		(struct _scontrol_*);

extern sector		sd_get_dsize		(struct _sdataspace_*);
extern struct _svalue_ *sd_get_svariables	(struct _sdataspace_*);
extern struct _sstring_*sd_get_sstrings		(struct _sdataspace_*);
extern char	       *sd_get_dtext		(struct _sdataspace_*,
						 Uint*));
extern struct _sarray_ *sd_get_sarrays		(struct _sdataspace_*);
extern struct _svalue_ *sd_get_selts		(struct _sdataspace_*);
extern void		sd_load_callouts	(dataspace*);

extern void		sd_save_control		(lpcenv*, control*);
extern bool		sd_save_dataspace	(dataspace*, int, Uint*);
extern void		sd_conv_control		(unsigned int);
extern void		sd_conv_dataspace	(object*, Uint*);

extern void		sd_del_scontrol		(struct _scontrol_*);
extern void		sd_del_sdataspace	(struct _sdataspace_*);
extern void		sd_free_scontrol	(struct _scontrol_*);
extern void		sd_free_sdataspace	(struct _sdataspace_*);
