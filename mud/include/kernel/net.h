# ifdef SYS_NETWORKING
#  define LIB_PORT	"/kernel/lib/port"
#  define PORT_OBJECT	"/kernel/obj/port"
# else
#  error	networking capabilities required
# endif
