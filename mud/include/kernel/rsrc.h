# define RSRCD		"/kernel/sys/rsrcd"
# define RSRCOBJ	("/kernel" + CLONABLE_SUBDIR + "rsrc")
# define API_RSRC	("/kernel" + INHERITABLE_SUBDIR + "api/rsrc")

# define RSRC_USAGE	0	/* resource usage */
# define RSRC_MAX	1	/* resource limit */
# define RSRC_DECAYTIME	2	/* time of last decay... */
# define RSRC_INDEXED	2	/* ...or, indexed usages */
# define RSRC_DECAY	3	/* decay percentage */
# define RSRC_PERIOD	4	/* decay period */

# define GRSRC_MAX	0	/* global resource limit */
# define GRSRC_DECAY	1	/* global decay percentage */
# define GRSRC_PERIOD	2	/* global decay period */

# define LIM_NEXT	0	/* next limits frame */
# define LIM_OWNER	1	/* owner of this frame */
# define LIM_MAXSTACK	2	/* max stack in frame */
# define LIM_MAXTICKS	3	/* max ticks in frame */
# define LIM_TICKS	4	/* current ticks in frame */

# define LIM_MAX_STACK	0	/* max stack */
# define LIM_MAX_TICKS	1	/* max ticks */
# define LIM_MAX_TIME	2	/* tick usage decay time */
