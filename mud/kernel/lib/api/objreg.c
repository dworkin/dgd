# include <kernel/kernel.h>
# include <kernel/objreg.h>

private object objregd;

/*
 * NAME:	create()
 * DESCRIPTION:	initialize API
 */
static void create()
{
    objregd = find_object(OBJREGD);
}

/*
 * NAME:	first_link()
 * DESCRIPTION:	return first object in linked list
 */
static object first_link(string owner)
{
    return objregd->first_link(owner);
}

/*
 * NAME:	prev_link()
 * DESCRIPTION:	return prev object in linked list
 */
static object prev_link(object obj)
{
    if (!obj) {
	error("Bad argument for prev_link");
    }
    return objregd->prev_link(obj);
}

/*
 * NAME:	next_link()
 * DESCRIPTION:	return next object in linked list
 */
static object next_link(object obj)
{
    if (!obj) {
	error("Bad argument for next_link");
    }
    return objregd->next_link(obj);
}
