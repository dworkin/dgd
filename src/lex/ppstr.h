typedef struct {
    char *buffer;	/* string buffer */
    int size;		/* size of buffer */
    int len;		/* lenth of string (-1 if illegal) */
} str;

extern void pps_init	P((void));
extern void pps_clear	P((void));
extern str *pps_new	P((char*, int));
extern void pps_del	P((str*));
extern int  pps_scat	P((str*, char*));
extern int  pps_ccat	P((str*, int));
