# include <status.h>

# define DRIVER		"/kernel/sys/driver"
# define AUTO		"/kernel/lib/auto"

/*
 * privilege levels
 *
 * level 0: kernel level
 * level 1: admin level
 */
# define PRIV0()	((int) ::status()[ST_STACKDEPTH] < 0)
# define PRIV1()	((int) ::status()[ST_TICKS] < 0)
