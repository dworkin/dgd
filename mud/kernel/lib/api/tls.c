# include <kernel/kernel.h>

private object driver;		/* driver object has direct access to TLS */

/*
 * NAME:	create()
 * DESCRIPTION:	initialize API
 */
static void create()
{
    driver = find_object(DRIVER);
}

/*
 * NAME:	get_tlvar()
 * DESCRIPTION:	get value of TLS variable
 */
static mixed get_tlvar(int index)
{
    if (index < 0) {
	error("Array index out of range");
    }
    return driver->get_tlvar(index);
}

/*
 * NAME:	set_tlvar()
 * DESCRIPTION:	set value of TLS variable
 */
static void set_tlvar(int index, mixed value)
{
    if (index < 0) {
	error("Array index out of range");
    }
    driver->set_tlvar(index, value);
}

/*
 * NAME:	set_tls_size()
 * DESCRIPTION:	set size of TLS storage
 */
static void set_tls_size(int size)
{
    if (size < 0) {
	error("TLS size must be >= 0");
    }
    driver->set_tls_size(size);
}
