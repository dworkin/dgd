# include "ed.h"
# undef error
# include "edcmd.h"
# include <signal.h>

struct _value_ *sp;
struct _frame_ *cframe;

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

    cframe = (struct _frame_ *) buffer;

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
	if (!ec_push((ec_ftn) NULL)) {
	    cb_edit(cb);
	    ec_pop();
	} else {
	    message((char *) NULL);
	}
    }

    while (ec_push((ec_ftn) NULL)) {
	message((char *) NULL);
	cb->flags &= ~(CB_INSERT|CB_CHANGE);
    }

    for (;;) {
	printf((cb->flags & CB_INSERT) ? "*\010" : ":");	/* BS */
	fflush(stdout);
	if (fgets(buffer, sizeof(buffer), stdin) == (char *) NULL) {
	    cb_del(cb);
	    return 1;
	}
	tmp = strchr(buffer, '\n');
	if (tmp != (char *) NULL) {
	    *tmp = '\0';
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
    error(f, a1, a2, a3);
}

void i_set_sp(newsp)
struct _value_ *newsp;
{
}

void interrupt()
{
}
