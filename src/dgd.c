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
    char *p;
    int narg;
    extern value *ilvp;

    chdir("lpc");
    str_init();
    arr_init(3000);
    o_init(100);
    kf_init();
    c_init("auto", "driver", "std.h", paths, FALSE);
    i_init(100, 10, 15, 1000000L);

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
	printf("Before: sp %X, ilvp %X\n", sp, ilvp);
	if ((p=strchr(buffer, ' ')) != (char *) NULL) {
	    *p++ = 0;
	    if ((*p >= '0' && *p <= '9') || *p == '-') {
		--sp;
		sp->type = T_NUMBER;
		sscanf(p, "%ld", &sp->u.number);
	    } else {
		--sp;
		sp->type = T_STRING;
		str_ref(sp->u.string = str_new(p, (long) strlen(p)));
	    }
	    narg = 1;
	} else {
	    narg = 0;
	}
	if (!i_apply(obj, buffer, FALSE, narg)) {
	    printf("No such function.\n");
	} else {
	    i_pop(1);
	}
	printf("After: sp %X, ilvp %X\n", sp, ilvp);
    }
}

void output(f, a1, a2, a3, a4)
char *f, *a1, *a2, *a3, *a4;
{
    printf(f, a1, a2, a3, a4);
}
