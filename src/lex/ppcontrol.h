/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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
    static bool init(char *file, char **id, String **strs, int nstr, int level);
    static void clear();
    static bool include(char *file, String **strs, int nstr);
    static char *filename();
    static unsigned short line();
    static int gettok();

    virtual void error(const char *format, ...) {
	va_list args;
	char buf[4 * STRINGSZ];		/* file name + 2 * string + overhead */

	snprintf(buf, sizeof(buf), "%s, %u: ", filename(), line());
	va_start(args, format);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, args);
	va_end(args);
	EC->message("%s\012", buf);     /* LF */
    }

private:
    static int wsgettok();
    static int mcgtok();
    static int wsmcgtok();
    static int expr_get();
    static long eval_expr(int priority);
    static int pptokenz(char *key, unsigned int len);
    static int tokenz(char *key, unsigned int len);
    static void unexpected(int token, const char *wanted,
			   const char *directive);
    static void do_include();
    static int argnum(char **args, int narg, int token);
    static void do_define();
};

extern Preproc *PP;

extern char *yytext;
extern int yyleng;
extern LPCint yynumber;
extern Float yyfloat;
