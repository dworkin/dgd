# include <status.h>

# define DRIVER		"/kernel/sys/driver"
# define AUTO		"/kernel/lib/auto"

/*
 * privilege levels
 *
 * level 0: kernel level
 * level 1: System level
 */
# define KERNEL()	(sscanf(previous_program(), "/kernel/%*s") != 0)
# define SYSTEM()	(sscanf(previous_program(), "/kernel/%*s") != \
				 sscanf(previous_program(), "/usr/System/%*s"))
