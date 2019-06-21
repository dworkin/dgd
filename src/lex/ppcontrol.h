/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2019 DGD Authors (see the commit log for details)
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

class PP {
public:
    static bool init(char *file, char **id, String **strs, int nstr, int level);
    static void clear();
    static int gettok();

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
