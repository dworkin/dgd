/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

class Preproc {
public:
    bool init(char *file, char **id, char *buffer, unsigned int buflen,
	      int level);
    void clear();
    bool include(char *file, char *buffer, unsigned int buflen);
    void push(char *buffer, unsigned int buflen);
    char *filename();
    unsigned short line();
    int gettok();

    virtual void error(const char *format, ...) {
	va_list args;
	char buf[4 * STRINGSZ];		/* file name + 2 * string + overhead */

	snprintf(buf, sizeof(buf), "\"%s\", %u: ", filename() + 1, line());
	va_start(args, format);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, args);
	va_end(args);
	fprintf(stderr, "%s\n", buf);
    }

private:
    int wsgettok();
    int mcgtok();
    int wsmcgtok();
    int expr_get();
    long eval_expr(int priority);
    int pptokenz(char *key, unsigned int len);
    int tokenz(char *key, unsigned int len);
    void unexpected(int token, const char *wanted, const char *directive);
    void do_include();
    int argnum(char **args, int narg, int token);
    void do_define();
};

extern Preproc *PP;

extern char *yytext;
extern int yyleng;
extern LPCint yynumber;
extern Float yyfloat;
