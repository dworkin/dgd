/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 2024 DGD Authors (see the commit log for details)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

static Hash LHM;
Hash *HM = &LHM;

static Preproc LPP;
Preproc *PP = &LPP;

class LexPath : public Path {
public:
    virtual char *include(char *buf, char *from, char *file) {
	if (!PP->include(Path::from(buf, from, file), (char *) NULL, 0)) {
	    return (char *) NULL;
	}
	return buf;
    }
};

static LexPath LPM;
Path *PM = &LPM;

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
    char filename[1024], *nfilename;
    int line, nline, c;

    if (argc != 2) {
	fprintf(stderr, "Usage: %s file\n", argv[0]);
	return 2;
    }

    filename[0] = '\0';
    line = 0;
    Preproc::init(argv[1], (char **) paths, (char *) NULL, 0, 0);
    while ((c=Preproc::gettok()) != EOF) {
	nfilename = Preproc::filename() + 1;
	nline = Preproc::line();
	if (strcmp(filename, nfilename) != 0) {
	    if (filename[0] != '\0') {
		printf("\n");
	    }
	    strcpy(filename, nfilename);
	    line = nline;
	    printf("#line %d \"%s\"\n", line, filename);
	} else if (nline != line) {
	    if (nline > line && nline - line <= 5) {
		while (line < nline) {
		    printf("\n");
		    line++;
		}
	    } else {
		line = nline;
		printf("\n#line %d\n", line);
	    }
	}
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
