# include "ed.h"
# undef error
# include <signal.h>
# include "edcmd.h"

cmdbuf *cb;

void intr()
{
    cb_del(cb);
    exit(2);
}

int dgd_main(argc, argv)
int argc;
char *argv[];
{
    char buffer[2048];
    char tmpfname[100], *tmp;

    signal(SIGHUP, intr);
    signal(SIGINT, intr);
    signal(SIGQUIT, intr);

    tmp = getenv("TMPDIR");
    if (tmp == (char *) NULL) {
	tmp = "/tmp";
    }
    sprintf(tmpfname, "%s/Ed0%05d", tmp, getpid());
    cb = cb_new(tmpfname);

    if (argc > 1) {

	cb->cmd = argv[1];
	if (!ec_push()) {
	    cb_edit(cb);
	    ec_pop();
	} else {
	    warning((char *) NULL);
	}
    }

    while (ec_push()) {
	warning((char *) NULL);
	cb->flags &= ~(CB_INSERT|CB_CHANGE);
    }

    for (;;) {
	printf((cb->flags & CB_INSERT) ? "*\010" : ":");	/* BS */
	if (gets(buffer) == (char *) NULL) {
	    cb_del(cb);
	    return 1;
	}
	if (!cb_command(cb, buffer)) {
	    cb_del(cb);
	    return 0;
	}
    }
}

void comm_finish()
{
}

char *path_ed_read(file) char *file; { return file; }

char *path_ed_write(file) char *file; { return file; }

void output(f, a1, a2, a3, a4)
char *f, *a1, *a2, *a3, *a4;
{
    printf(f, a1, a2, a3, a4);
}

void ed_error(f, a1, a2, a3)
char *f, *a1, *a2, *a3;
{
    error(f, a1,a2, a3);
}
