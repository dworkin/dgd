# include "dgd.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
# include "comp.h"

main(argc, argv)
int argc;
char *argv[];
{
    char buffer[100];
    static char *paths[] = { "/usr/include", 0 };
    object *obj;

    chdir("lpc");
    str_init();
    arr_init();
    o_init(100);
    kf_init();
    c_init("auto", "driver", "std.h", paths, FALSE);
    i_init(100, 10, 15, 10000L);

    if (ec_push()) {
	warning((char *) NULL);
	return 1;
    }
    obj = c_compile(argv[1]);
    ec_pop();

    while (ec_push()) {
	warning((char *) NULL);
	i_dump_trace(stderr);
	i_clear();
    }

    for (;;) {
	printf("> ");
	gets(buffer);
	if (!i_apply(obj, buffer, FALSE, 0)) {
	    printf("No such function.\n");
	} else {
	    printf("Ok.\n");
	    i_pop(1);
	}
    }
}
