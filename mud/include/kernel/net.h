# ifdef SYS_NETWORKING
#  define LIB_PORT	"/kernel/lib/port"
#  define PORT_OBJECT	"/kernel/obj/port"
#  define PORT_UDP	"/kernel/obj/udp"
# else
#  error	networking capabilities required
# endif
