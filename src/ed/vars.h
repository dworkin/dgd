typedef struct {
    char *name;		/* long variable name */
    char *sname;	/* short variable name */
    Int val;		/* value */
} vars;

# define IGNORECASE(v)	(v[0].val)
# define SHIFTWIDTH(v)	(v[1].val)
# define WINDOW(v)	(v[2].val)

# define NUMBER_OF_VARS	3

extern vars *va_new  P((void));
extern void  va_del  P((vars*));
extern void  va_set  P((vars*, char*));
extern void  va_show P((vars*));
