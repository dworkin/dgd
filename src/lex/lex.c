# include "lex.h"
# undef error
# include <ctype.h>
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"

YYSTYPE yylval;

char *paths[] = { "..", "/usr/include", 0 };

void convert(s)
register char *s;
{
    while (*s) {
	*s = toupper(*s);
	s++;
    }
}

int dgd_main(argc, argv)
int argc;
char *argv[];
{
    register int c;

    pp_init(argv[1], paths, 0);

    while ((c=pp_gettok()) != EOF) {
	switch (c) {
	case STRING_CONST:
	    printf(" \"%s\"", yytext);
	    break;

	case INT_CONST:
	    printf(" %ld", (long) yylval.number);
	    break;

	case IDENTIFIER:
	    printf(" %s", yytext);
	    break;

	default:
	    if (c >= FOR && c <= SWITCH) {
		convert(yytext);
	    }
	    printf(" %s", yytext);
	    break;
	}
    }
    putchar('\n');

    pp_clear();
    return 0;
}

void c_error(s1, s2, s3)
char *s1, *s2, *s3;
{
    fprintf(stderr, "/%s, line %u: ", tk_filename(), tk_line());
    fprintf(stderr, s1, s2, s3);
    fputc('\n', stderr);
}

void error(s1, s2, s3)
char *s1, *s2, *s3;
{
    fprintf(stderr, "/%s, line %u: ", tk_filename(), tk_line());
    fprintf(stderr, s1, s2, s3);
    fputc('\n', stderr);
    exit(1);
}

void fatal(f, a1, a2)
char *f, *a1, *a2;
{
    fprintf(stderr, "Fatal error: ");
    fprintf(stderr, f, a1, a2);
    abort();
}

char *path_include(f, file)
char *f, *file;
{
    return file;
}
