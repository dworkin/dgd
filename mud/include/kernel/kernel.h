# include <config.h>

# define DRIVER		"/kernel/sys/driver"
# define AUTO		"/kernel/lib/auto"

/*
 * privilege levels
 */
# define KERNEL()	(sscanf(previous_program(), "/kernel/%*s") != 0)
# define SYSTEM()	(sscanf(previous_program(), USR + "/System/%*s") != 0)
