# include "dgd.h"
# include "str.h"
# include "xfloat.h"
# include "node.h"
# include "compile.h"
# include "parser.h"

# define LALLOC(type, size)	ALLOC(comppool, type, size)
# define LALLOCA(type, size)	ALLOCA(comppool, type, size)
# define LREALLOC(mem, type, size1, size2) \
				REALLOC(comppool, mem, type, size1, size2)
# define LFREE(mem)		FREE(comppool, mem)
# define LFREEA(mem)		FREEA(comppool, mem)
# define lexenv			compenv
# define error			c_error
# define warning		c_error

extern struct _mempool_ *comppool;
extern lpcenv *compenv;
