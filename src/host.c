# include "dgd.h"


# ifdef NEED_LMALLOC
# include "host/int2/lmalloc.c"
# endif

# ifdef UNIX_PATH_RESOLVE
# include "host/unix/path.c"
# endif
