# include "lex.h"
# include "macro.h"
# include "token.h"
# include "special.h"

/*
 * Predefined macro handling.
 */

static char datestr[14];
static char timestr[11];

/*
 * NAME:	special_define()
 * DESCRIPTION:	predefine macros
 */
void special_define()
{
    char buf[26];

    mc_define("__LINE__", (char *) NULL, -1);
    mc_define("__FILE__", (char *) NULL, -1);
    mc_define("__DATE__", (char *) NULL, -1);
    mc_define("__TIME__", (char *) NULL, -1);

    P_ctime(buf, P_time());
    sprintf(datestr, "\"%.6s %.4s\"", buf + 4, buf + 20);
    sprintf(timestr, "\"%.8s\"", buf + 11);
}

/*
 * NAME:	special_replace()
 * DESCRIPTION:	return the expandation of a predefined macro
 */
char *special_replace(name)
register char *name;
{
    static char buf[STRINGSZ + 3];

    if (strcmp(name, "__LINE__") == 0) {
	sprintf(buf, " %u ", tk_line());
	return buf;
    } else if (strcmp(name, "__FILE__") == 0) {
	sprintf(buf, "\"/%s\"", tk_filename());
	return buf;
    } else if (strcmp(name, "__DATE__") == 0) {
	return datestr;
    } else if (strcmp(name, "__TIME__") == 0) {
	return timestr;
    }
    return (char *) NULL;
}
