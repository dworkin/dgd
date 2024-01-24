# include "lex.h"
# include "ppcontrol.h"
# include "path.h"
# include "hash.h"
# include <time.h>
# include <sys/time.h>
# include <errno.h>


static Alloc LMM;
Alloc *MM = &LMM;

static ErrorContext LEC;
ErrorContext *EC = &LEC;

static Path LPM;
Path *PM = &LPM;

static Hash LHM;
Hash *HM = &LHM;

static Preproc LPP;
Preproc *PP = &LPP;

const char *paths[] = { "/usr/include", (char *) NULL };
YYSTYPE yylval;
static double flt;

bool Float::atof(char **buf, Float *f)
{
    errno = 0;
    flt = strtod(*buf, buf);
    return (errno == 0);
}

Uint P_time()
{
    return (Uint) time((time_t *) NULL);
}

char *P_ctime(char *buf, Uint time)
{
    time_t t;

    t = time;
    return ctime_r(&t, buf);
}

int main(int argc, char *argv[])
{
    int c;

    Preproc::init(argv[1], (char **) paths, (char *) NULL, 0, 0);
    while ((c=Preproc::gettok()) != EOF) {
	switch (c) {
	case STRING_CONST:
	    printf(" \"%s\"", yytext);
	    break;

	case INT_CONST:
	    printf(" %lld", (long long) yylval.number);
	    break;

	case FLOAT_CONST:
	    printf(" %lg", flt);
	    break;

	default:
	    printf(" %s", yytext);
	    break;
	}
    }
    printf("\n");

    return 0;
}
