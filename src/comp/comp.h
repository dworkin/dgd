# include "dgd.h"

# define CALLOC(type, size)	ALLOC(comppool, type, size)
# define CALLOCA(type, size)	ALLOCA(comppool, type, size)
# define CREALLOC(mem, type, size1, size2) \
				REALLOC(comppool, mem, type, size1, size2)
# define CFREE(mem)		FREE(comppool, mem)
# define CFREEA(mem)		FREEA(comppool, mem)

extern struct _mempool_ *comppool;
extern lpcenv *compenv;
