# include "ed.h"
# include "vars.h"

/*
 * The editor variables are handled here.
 */

/*
 * NAME:	vars->new()
 * DESCRIPTION:	allocate and initialize a variable buffer
 */
vars *va_new()
{
    static vars dflt[] = {
	{ "ignorecase",	"ic",	FALSE },
	{ "shiftwidth",	"sw",	4 },
	{ "window",	"wi",	20 },
    };
    register vars *v;

    v = ALLOC(vars, NUMBER_OF_VARS);
    memcpy(v, dflt, sizeof(dflt));

    return v;
}

/*
 * NAME:	vars->del()
 * DESCRIPTION:	delete a variable buffer
 */
void va_del(v)
vars *v;
{
    FREE(v);
}

/*
 * NAME:	vars->set()
 * DESCRIPTION:	set the value of a variable.
 */
void va_set(v, option)
register vars *v;
register char *option;
{
    register char *val;
    register Int i;

    if (strncmp(option, "no", 2) == 0) {
	option += 2;
	val = "0";
    } else {
	val = strchr(option, '=');
	if (val != (char *) NULL) {
	    *val++ = '\0';
	}
    }

    for (i = NUMBER_OF_VARS; i > 0; --i, v++) {
	if (strcmp(v->name, option) == 0 ||
	  strcmp(v->sname, option) == 0) {
	    if (!val) {
		v->val = 1;
	    } else {
		char *p;

		p = val;
		i = strtoint(&p);
		if (val == p || i < 0) {
		    error("Bad numeric value for option \"%s\"", v->name);
		}
		v->val = i;
	    }
	    return;
	}
    }
    error("No such option");
}

/*
 * NAME:	vars->show()
 * DESCRIPTION:	show all variables
 */
void va_show(v)
register vars *v;
{
    output("%signorecase\011",   ((v++)->val) ? "" : "no");	/* HT */
    output("shiftwidth=%ld\011", (long) (v++)->val);		/* HT */
    output("window=%ld\012",     (long) (v++)->val);		/* LF */
}
