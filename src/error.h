# define ec_push()	setjmp(*_ec_push_())

extern jmp_buf *_ec_push_	P((void));
extern jmp_buf *ec_pop		P((void));

extern char *errormesg		P((void));
extern void  errorlog		P((char*));

extern void  message		();
extern void  warning		();
extern void  error		();
extern void  fatal		();
