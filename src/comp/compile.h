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

class Compile {
public:
    static void init(char *a, char *d, char *i, char **p, int tc);
    static bool typechecking();
    static bool inherit(char *file, Node *label, int priv);
    static Object *compile(Frame *f, char *file, Object *obj, int nstr,
			   int iflag);
    static int autodriver();
    static String *objecttype(Node *n);
    static void global(unsigned int sclass, Node *type, Node *n);
    static void function(unsigned int sclass, Node *type, Node *n);
    static void funcbody(Node *n);
    static void local(unsigned int sclass, Node *type, Node *n);
    static void startCond();
    static void startCond2();
    static void endCond();
    static void saveCond();
    static void matchCond();
    static bool nil(Node *n);
    static Node *concat(Node *n1, Node *n2);
    static Node *exprStmt(Node *n);
    static Node *ifStmt(Node *n1, Node *n2);
    static Node *endIfStmt(Node *n1, Node *n3);
    static void loop();
    static Node *doStmt(Node *n1, Node *n2);
    static Node *whileStmt(Node *n1, Node *n2);
    static Node *forStmt(Node *n1, Node *n2, Node *n3, Node *n4);
    static void startRlimits();
    static Node *endRlimits(Node *n1, Node *n2, Node *n3);
    static Node *exception(Node *n);
    static void startCatch();
    static void endCatch();
    static Node *doneCatch(Node *n1, Node *n2, bool pop);
    static void startSwitch(Node *n, int typechecked);
    static Node *endSwitch(Node *expr, Node *stmt);
    static Node *caseLabel(Node *n1, Node *n2);
    static Node *defaultLabel();
    static Node *label(Node *n);
    static Node *gotoStmt(Node *n);
    static Node *breakStmt();
    static Node *continueStmt();
    static Node *returnStmt(Node *n, int typechecked);
    static void startCompound();
    static Node *endCompound(Node *n);
    static Node *flookup(Node *n, int typechecked);
    static Node *iflookup(Node *n, Node *label);
    static Node *aggregate(Node *n, unsigned int type);
    static Node *localVar(Node *n);
    static Node *globalVar(Node *n);
    static short vtype(int i);
    static Node *funcall(Node *func, Node *args);
    static Node *arrow(Node *other, Node *func, Node *args);
    static Node *address(Node *func, Node *args, int typechecked);
    static Node *extend(Node *func, Node *args, int typechecked);
    static Node *call(Node *func, Node *args, int typechecked);
    static Node *newObject(Node *o, Node *args);
    static Node *instanceOf(Node *n, Node *prog);
    static Node *checkcall(Node *n, int typechecked);
    static Node *tst(Node *n);
    static Node *_not(Node *n);
    static Node *lvalue(Node *n, const char *oper);
    static Node *assign(Node *n);
    static unsigned short matchType(unsigned int type1, unsigned int type2);
    static void error(const char *format, ...);

private:
    static void clear();
    static void declFunc(unsigned short sclass, Node *type, String *str,
			 Node *formals, bool function);
    static void declVar(unsigned short sclass, Node *type, String *str,
			bool global);
    static void declList(unsigned short sclass, Node *type, Node *list,
			 bool global);
    static Node *block(Node *n, int type, int flags);
    static Node *reloop(Node *n);
    static Node *endloop(Node *n);
    static unsigned int aggrType(unsigned int, unsigned int);
    static bool lvalue(Node *n);
    static Node *funcall(Node *call, Node *args, int funcptr);
    static void lvalAggr(Node **n);
};
