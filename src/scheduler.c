# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "data.h"
# include "interpret.h"


static lpcenv *lpc;

lpcenv *sch_new_env()
{
    register lpcenv *e;

    e = SALLOC(lpcenv, 1);
    e->mp = m_new_pool();
    e->ee = ec_new_env();
    e->ae = arr_new_env();
    e->oe = o_new_env();
    e->de = d_new_env();
    e->ie = i_new_env(e);
    e->this_user = OBJ_NONE;

    return e;
}

void sch_init()
{
    lpc = sch_new_env();
}

lpcenv *sch_env()
{
    return lpc;
}
