# include "config.h"
# include "alloc.h"
# include "error.h"

extern bool call_driver_object	P((char*, int));
extern void swapout		P((void));
extern void dump_state		P((void));
extern void interrupt		P((void));
extern void finish		P((void));
extern int  dgd_main		P((int, char**));
