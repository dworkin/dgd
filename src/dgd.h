# include "host.h"

typedef struct _string_ string;
typedef struct _array_ array;
typedef struct _object_ object;
typedef struct _value_ value;
typedef struct _control_ control;
typedef struct _dataspace_ dataspace;
typedef struct _frame_ frame;

# include "config.h"
# include "alloc.h"
# include "error.h"

extern bool call_driver_object	P((frame*, char*, int));
extern void swapout		P((void));
extern void dump_state		P((void));
extern void interrupt		P((void));
extern void finish		P((void));
extern void endthread		P((void));
extern void errhandler		P((frame*, Int));
extern int  dgd_main		P((int, char**));
