# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "csupport.h"

# define TAG(t)	extern precomp t;
# include "list"
# undef TAG

precomp *precompiled[] = {
# define TAG(t)	&t,
# include "list"
# undef TAG
    (precomp *) NULL	/* terminator */
};
