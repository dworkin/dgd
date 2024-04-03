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

/*
 * LPC grammar, handles construction of parse trees and type checking.
 * Currently there is one shift/reduce conflict, on else.
 * The node->mod field is used to store the type of an expression. (!)
 */

%{

# define INCLUDE_CTYPE
# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "control.h"
# include "data.h"
# include "interpret.h"
# include "ppcontrol.h"
# include "table.h"
# include "node.h"
# include "compile.h"

# define yylex		PP->gettok
# define yyerror	Compile::error
# define register	/* nothing */

int nerrors;			/* number of errors encountered so far */
static int ndeclarations;	/* number of declarations */
static int nstatements;		/* number of statements in current function */
static bool typechecking;	/* does the current function have it? */

class YYParser {
public:
    static void _void(Node *n);
    static Node *prefix(int op, Node *n, const char *name);
    static Node *postfix(int op, Node *n, const char *name);
    static Node *cast(Node *n, Node *type);
    static Node *idx(Node *n1, Node *n2);
    static Node *range(Node *n1, Node *n2, Node *n3);
    static Node *bini(int op, Node *n1, Node *n2, const char *name);
    static Node *bina(int op, Node *n1, Node *n2, const char *name);
    static Node *mult(int op, Node *n1, Node *n2, const char *name);
    static Node *mdiv(int op, Node *n1, Node *n2, const char *name);
    static Node *mod(int op, Node *n1, Node *n2, const char *name);
    static Node *add(int op, Node *n1, Node *n2, const char *name);
    static Node *sub(int op, Node *n1, Node *n2, const char *name);
    static Node *umin(Node *n);
    static Node *lshift(int op, Node *n1, Node *n2, const char *name);
    static Node *rshift(int op, Node *n1, Node *n2, const char *name);
    static Node *rel(int op, Node *n1, Node *n2, const char *name);
    static Node *eq(Node *n1, Node *n2);
    static Node *_and(int op, Node *n1, Node *n2, const char *name);
    static Node *_xor(int op, Node *n1, Node *n2, const char *name);
    static Node *_or(int op, Node *n1, Node *n2, const char *name);
    static Node *land(Node *n1, Node *n2);
    static Node *lor(Node *n1, Node *n2);
    static Node *quest(Node *n1, Node *n2, Node *n3);
    static Node *assign(Node *n1, Node *n2);
    static Node *comma(Node *n1, Node *n2);

private:
    static bool unary(Node *n, const char *name);
};

%}


/*
 * Keywords. The order is determined in PP->tokenz() in the lexical scanner.
 */
%token NOMASK BREAK DO MAPPING ELSE CASE OBJECT DEFAULT FLOAT CONTINUE STATIC
       INT FOR IF OPERATOR INHERIT RLIMITS GOTO FUNCTION RETURN MIXED WHILE
       STRING TRY PRIVATE VOID NEW CATCH ATOMIC NIL SWITCH VARARGS

/*
 * composite tokens
 */
%token LARROW RARROW PLUS_PLUS MIN_MIN LSHIFT RSHIFT LE GE EQ NE LAND LOR
       PLUS_EQ MIN_EQ MULT_EQ DIV_EQ MOD_EQ LSHIFT_EQ RSHIFT_EQ AND_EQ
       XOR_EQ OR_EQ COLON_COLON DOT_DOT ELLIPSIS STRING_CONST IDENTIFIER

%union {
    LPCint number;		/* lex input */
    Float real;			/* lex input */
    unsigned short type;	/* internal */
    class Node *node;		/* internal */
}

/*
 * token types
 */
%token <number> INT_CONST
%token <real> FLOAT_CONST

/*
 * lexical scanner tokens
 */
%token MARK HASH HASH_HASH INCL_CONST NR_TOKENS

/*
 * rule types
 */
%type <type> class_specifier_list class_specifier_list2 class_specifier
	     opt_private non_private star_list
%type <node> opt_inherit_label string composite_string formals_declaration
	     formal_declaration_list varargs_formal_declaration string_exp
	     formal_declaration type_specifier data_dcltr operator
	     function_name function_dcltr dcltr list_dcltr dcltr_or_stmt_list
	     dcltr_or_stmt if_stmt stmt nocase_stmt compound_stmt
	     opt_caught_stmt case_list case function_call primary_p1_exp
	     primary_p2_exp postfix_exp prefix_exp cast_exp mult_oper_exp
	     add_oper_exp shift_oper_exp rel_oper_exp equ_oper_exp
	     bitand_oper_exp bitxor_oper_exp bitor_oper_exp and_oper_exp
	     or_oper_exp cond_exp exp list_exp opt_list_exp f_list_exp
	     f_opt_list_exp arg_list opt_arg_list opt_arg_list_comma assoc_exp
	     assoc_arg_list opt_assoc_arg_list_comma ident exception

%%

program
	:	{
		  nerrors = 0;
		  ndeclarations = 0;
		}
	  top_level_declarations
		{
		  if (nerrors > 0) {
		      YYABORT;
		  }
		}
	;

top_level_declarations
	: /* empty */
	| top_level_declarations top_level_declaration
		{
		  if (nerrors > 0) {
		      YYABORT;
		  }
		}
	;

top_level_declaration
	: opt_private INHERIT opt_inherit_label opt_object composite_string ';'
		{
		  if (ndeclarations > 0) {
		      Compile::error("inherit must precede all declarations");
		  } else if (nerrors > 0 ||
			     !Compile::inherit($5->l.string->text, $3, $1 != 0))
		  {
		      /*
		       * The object to be inherited may have been compiled;
		       * abort this compilation and possibly restart later.
		       */
		      YYABORT;
		  }
		}
	| data_declaration
		{ ndeclarations++; }
	| function_declaration
		{ ndeclarations++; }
	;

opt_inherit_label
	: /* empty */
		{ $$ = (Node *) NULL; }
	| ident
	;

ident
	: IDENTIFIER
		{ $$ = Node::createStr(String::create(yytext, yyleng)); }
	;

exception
	: ident
	| ELLIPSIS
		{ $$ = (Node *) NULL; }
	;

composite_string
	: string_exp
	| composite_string '+' string_exp
		{ $$ = Node::createStr($1->l.string->add($3->l.string)); }
	;

string_exp
	: string
	| '(' composite_string ')'
		{ $$ = $2; }
	;

string
	: STRING_CONST
		{ $$ = Node::createStr(String::create(yytext, yyleng)); }
	;

data_declaration
	: class_specifier_list type_specifier list_dcltr ';'
		{ Compile::global($1, $2, $3); }
	;

function_declaration
	: class_specifier_list type_specifier function_dcltr
		{
		  typechecking = TRUE;
		  Compile::function($1, $2, $3);
		}
	  compound_stmt
		{
		  if (nerrors == 0) {
		      Compile::funcbody($5);
		  }
		}
	| class_specifier_list function_name '(' formals_declaration ')'
		{
		  typechecking = Compile::typechecking();
		  Compile::function($1, Node::createType((typechecking) ?
							  T_VOID : T_NIL,
							 (String *) NULL),
			     Node::createBin(N_FUNC, 0, $2, $4));
		}
	  compound_stmt
		{
		  if (nerrors == 0) {
		      Compile::funcbody($7);
		  }
		}
	;

local_data_declaration
	: class_specifier_list type_specifier list_dcltr ';'
		{ Compile::local($1, $2, $3); }
	;

formals_declaration
	: /* empty */
		{ $$ = (Node *) NULL; }
	| VOID	{ $$ = (Node *) NULL; }
	| formal_declaration_list
	| formal_declaration_list ELLIPSIS
		{
		  $$ = $1;
		  $$->flags |= F_ELLIPSIS;
		}
	;

formal_declaration_list
	: varargs_formal_declaration
	| formal_declaration_list ',' varargs_formal_declaration
		{ $$ = Node::createBin(N_PAIR, 0, $1, $3); }
	;

varargs_formal_declaration
	: VARARGS formal_declaration
		{
		  $$ = $2;
		  $$->flags |= F_VARARGS;
		}
	| formal_declaration
	;

formal_declaration
	: type_specifier data_dcltr
		{
		  $$ = $2;
		  $$->mod |= $1->mod;
		  $$->sclass = $1->sclass;
		  if ($$->sclass != (String *) NULL) {
		      $$->sclass->ref();
		  }
		}
	| ident {
		  $$ = $1;
		  $$->mod = T_NIL;	/* only if typechecking, though */
		}
	;

class_specifier_list
	: opt_private
	| non_private
	| class_specifier class_specifier_list2
		{ $$ = $1 | $2; }
	;

class_specifier_list2
	: class_specifier
	| class_specifier_list2 class_specifier
		{ $$ = $1 | $2; }
	;

class_specifier
	: PRIVATE
		{ $$ = C_STATIC | C_PRIVATE; }
	| non_private
	;

opt_private
	: /* empty */
		{ $$ = 0; }
	| PRIVATE
		{ $$ = C_STATIC | C_PRIVATE; }
	;

non_private
	: STATIC
		{ $$ = C_STATIC; }
	| ATOMIC
		{ $$ = C_ATOMIC; }
	| NOMASK
		{ $$ = C_NOMASK; }
	| VARARGS
		{ $$ = C_VARARGS; }
	;

type_specifier
	: INT	{ $$ = Node::createType(T_INT, (String *) NULL); }
	| FLOAT	{ $$ = Node::createType(T_FLOAT, (String *) NULL); }
	| STRING
		{ $$ = Node::createType(T_STRING, (String *) NULL); }
	| OBJECT
		{ $$ = Node::createType(T_OBJECT, (String *) NULL); }
	| OBJECT composite_string
		{ $$ = Node::createType(T_CLASS, Compile::objecttype($2)); }
	| MAPPING
		{ $$ = Node::createType(T_MAPPING, (String *) NULL); }
	| FUNCTION
		{
		  $$ = Node::createStr(String::create("/" BIPREFIX "function",
						      BIPREFIXLEN + 9));
		  $$ = Node::createType(T_CLASS, Compile::objecttype($$));
		}
	| MIXED	{ $$ = Node::createType(T_MIXED, (String *) NULL); }
	| VOID	{ $$ = Node::createType(T_VOID, (String *) NULL); }
	;

opt_object
	: /* empty */
	| OBJECT
	;

star_list
	: /* empty */
		{ $$ = 0; }
	| star_list '*'
		{
		  $$ = $1 + 1;
		  if ($$ == 1 << (8 - REFSHIFT)) {
		      Compile::error("too deep indirection");
		  }
		}
	;

data_dcltr
	: star_list ident
		{
		  $$ = $2;
		  $$->mod = ($1 << REFSHIFT) & T_REF;
		}
	;

operator
	: OPERATOR '+'
		{ $$ = Node::createOp("+"); }
	| OPERATOR '-'
		{ $$ = Node::createOp("-"); }
	| OPERATOR '*'
		{ $$ = Node::createOp("*"); }
	| OPERATOR '/'
		{ $$ = Node::createOp("/"); }
	| OPERATOR '%'
		{ $$ = Node::createOp("%"); }
	| OPERATOR '&'
		{ $$ = Node::createOp("&"); }
	| OPERATOR '^'
		{ $$ = Node::createOp("^"); }
	| OPERATOR '|'
		{ $$ = Node::createOp("|"); }
	| OPERATOR '<'
		{ $$ = Node::createOp("<"); }
	| OPERATOR '>'
		{ $$ = Node::createOp(">"); }
	| OPERATOR LE
		{ $$ = Node::createOp("<="); }
	| OPERATOR GE
		{ $$ = Node::createOp(">="); }
	| OPERATOR LSHIFT
		{ $$ = Node::createOp("<<"); }
	| OPERATOR RSHIFT
		{ $$ = Node::createOp(">>"); }
	| OPERATOR '~'
		{ $$ = Node::createOp("~"); }
	| OPERATOR PLUS_PLUS
		{ $$ = Node::createOp("++"); }
	| OPERATOR MIN_MIN
		{ $$ = Node::createOp("--"); }
	| OPERATOR '[' ']'
		{ $$ = Node::createOp("[]"); }
	| OPERATOR '[' ']' '='
		{ $$ = Node::createOp("[]="); }
	| OPERATOR '[' DOT_DOT ']'
		{ $$ = Node::createOp("[..]"); }
	;

function_name
	: ident
	| operator
	;

function_dcltr
	: star_list function_name '(' formals_declaration ')'
		{
		  $$ = Node::createBin(N_FUNC, ($1 << REFSHIFT) & T_REF, $2,
				       $4);
		}
	;

dcltr
	: data_dcltr
	| function_dcltr
	;

list_dcltr
	: dcltr
	| list_dcltr ',' dcltr
		{ $$ = Node::createBin(N_PAIR, 0, $1, $3); }
	;

dcltr_or_stmt_list
	: /* empty */
		{ $$ = (Node *) NULL; }
	| dcltr_or_stmt_list dcltr_or_stmt
		{ $$ = Compile::concat($1, $2); }
	;

dcltr_or_stmt
	: local_data_declaration
		{
		  if (nstatements > 0) {
		      Compile::error("declaration after statement");
		  }
		  $$ = (Node *) NULL;
		}
	| stmt	{
		  nstatements++;
		  $$ = $1;
		}
	| error ';'
		{
		  if (nerrors >= MAX_ERRORS) {
		      YYABORT;
		  }
		  $$ = (Node *) NULL;
		}
	;

if_stmt
	: IF '(' f_list_exp ')'
		{ Compile::startCond(); }
	  stmt	{ $$ = Compile::ifStmt($3, $6); }
	;

stmt
	: nocase_stmt
	| case_list nocase_stmt
		{
		  if ($1 != NULL) {
		      $$ = $1;
		      $$->r.right->l.left = $2;
		  } else {
		      $$ = $2;
		  }
		}
	;

nocase_stmt
	: list_exp ';'
		{ $$ = Compile::exprStmt($1); }
	| compound_stmt
	| if_stmt
		{
		  Compile::endCond();
		  $$ = Compile::endIfStmt($1, (Node *) NULL);
		}
/* will cause shift/reduce conflict */
	| if_stmt ELSE
		{ Compile::startCond2(); }
	  stmt
		{
		  if ($1 != (Node *) NULL && $4 != (Node *) NULL) {
		      if (!(($1->flags | $4->flags) & F_END)) {
			  Compile::matchCond();
			  Compile::saveCond();
		      } else if (!($4->flags & F_END)) {
			  Compile::saveCond();
			  Compile::saveCond();
		      } else {
			  Compile::endCond();
			  if (!($1->flags & F_END)) {
			      Compile::saveCond();
			  } else {
			      Compile::endCond();
			  }
		      }
		  } else {
		      Compile::endCond();
		      Compile::endCond();
		  }
		  $$ = Compile::endIfStmt($1, $4);
		}
	| DO	{
		  Compile::loop();
		  Compile::startCond();
		}
	  stmt WHILE '('
		{
		  Compile::endCond();
		  Compile::startCond();
		}
	  f_list_exp ')' ';'
		{
		  Compile::endCond();
		  $$ = Compile::doStmt($7, $3);
		}
	| WHILE '(' f_list_exp ')'
		{
		  Compile::loop();
		  Compile::startCond();
		}
	  stmt	{
		  Compile::endCond();
		  $$ = Compile::whileStmt($3, $6);
		}
	| FOR '(' opt_list_exp ';' f_opt_list_exp ';'
		{ Compile::startCond(); }
	  opt_list_exp ')'
		{
		  Compile::endCond();
		  Compile::loop();
		  Compile::startCond();
		}
	  stmt	{
		  Compile::endCond();
		  $$ = Compile::forStmt(Compile::exprStmt($3), $5,
					Compile::exprStmt($8), $11);
		}
	| RLIMITS '(' f_list_exp ';' f_list_exp ')'
		{
		  if (typechecking) {
		      char tnbuf[TNBUFSIZE];

		      if ($3->mod != T_INT && $3->mod != T_MIXED) {
			  Compile::error("bad type for stack rlimit (%s)",
					 Value::typeName(tnbuf, $3->mod));
		      }
		      if ($5->mod != T_INT && $5->mod != T_MIXED) {
			  Compile::error("bad type for ticks rlimit (%s)",
				         Value::typeName(tnbuf, $5->mod));
		      }
		  }
		  Compile::startRlimits();
		}
	  compound_stmt
		{ $$ = Compile::endRlimits($3, $5, $8); }
	| TRY	{
		  Compile::startCatch();
		  Compile::startCond();
		}
	  compound_stmt
		{
		  Compile::endCond();
		  Compile::endCatch();
		  Compile::startCond();
		}
	  CATCH '(' exception ')' '{'
		{
		  nstatements = 0;
		  Compile::startCompound();
		  if ($7 != (Node *) NULL) {
		      $<node>9 = Compile::exprStmt(Compile::exception($7));
		  }
		}
	  dcltr_or_stmt_list '}'
		{
		  nstatements++;
		  $$ = Compile::endCompound($11);
		  Compile::endCond();
		  if ($7 != (Node *) NULL && $$ != (Node *) NULL) {
		      $$ = Compile::doneCatch($3, Compile::concat($<node>9, $$),
					      FALSE);
		  } else {
		      $$ = Compile::doneCatch($3, $$, TRUE);
		  }
		}
	| CATCH	{
		  Compile::startCatch();
		  Compile::startCond();
		}
	  compound_stmt
		{
		  Compile::endCond();
		  Compile::endCatch();
		  Compile::startCond();
		}
	  opt_caught_stmt
		{
		  Compile::endCond();
		  $$ = Compile::doneCatch($3, $5, TRUE);
		}
	| SWITCH '(' f_list_exp ')'
		{ Compile::startSwitch($3, typechecking); }
	  compound_stmt
		{ $$ = Compile::endSwitch($3, $6); }
	| ident ':'
		{ $<node>2 = Compile::label($1); }
	  stmt	{ $$ = Compile::concat($<node>2, $4); }
	| GOTO ident ';'
		{
		  $$ = Compile::gotoStmt($2);
		}
	| BREAK ';'
		{
		  $$ = Compile::breakStmt();
		}
	| CONTINUE ';'
		{
		  $$ = Compile::continueStmt();
		}
	| RETURN f_opt_list_exp ';'
		{ $$ = Compile::returnStmt($2, typechecking); }
	| ';'	{ $$ = (Node *) NULL; }
	;

compound_stmt
	: '{'	{
		  nstatements = 0;
		  Compile::startCompound();
		}
	  dcltr_or_stmt_list '}'
		{
		  nstatements++;
		  $$ = Compile::endCompound($3);
		}
	;

opt_caught_stmt
	: /* empty */
		{ $$ = (Node *) NULL; }
	| ':' stmt
		{ $$ = $2; }
	;

case_list
	: case	{
		  $$ = $1;
		  if ($1 != (Node *) NULL) {
		      $$->r.right = $$;
		  }
		}
	| case_list case
		{
		  if ($2 != (Node *) NULL) {
		      $$ = $2;
		      if ($1 != (Node *) NULL) {
			  $$->l.left = $1;
			  $$->r.right = $1->r.right;
		      } else {
			  $$->r.right = $$;
		      }
		  } else {
		      $$ = $1;
		  }
		}
	;

case
	: CASE exp ':'
		{ $$ = Compile::caseLabel($2, (Node *) NULL); }
	| CASE exp DOT_DOT exp ':'
		{ $$ = Compile::caseLabel($2, $4); }
	| DEFAULT ':'
		{ $$ = Compile::defaultLabel(); }
	;

function_call
	: function_name
		{ $$ = Compile::flookup($1, typechecking); }
	| COLON_COLON function_name
		{ $$ = Compile::iflookup($2, (Node *) NULL); }
	| function_name COLON_COLON function_name
		{ $$ = Compile::iflookup($3, $1); }
	;

primary_p1_exp
	: INT_CONST
		{ $$ = Node::createInt($1); }
	| FLOAT_CONST
		{ $$ = Node::createFloat(&$1); }
	| NIL	{ $$ = Node::createNil(); }
	| string
	| '(' '{' opt_arg_list_comma '}' ')'
		{ $$ = Compile::aggregate($3, T_ARRAY); }
	| '(' '[' opt_assoc_arg_list_comma ']' ')'
		{ $$ = Compile::aggregate($3, T_MAPPING); }
	| ident	{
		  $$ = Compile::localVar($1);
		  if ($$ == (Node *) NULL) {
		      $$ = Compile::globalVar($1);
		      if (typechecking) {
			  if ($$->mod != T_MIXED && !Config::typechecking()) {
			      /*
			       * global vars might be modified by untypechecked
			       * functions...
			       */
			      $$ = Node::createMon(N_CAST, $$->mod, $$);
			      $$->sclass = $$->l.left->sclass;
			      if ($$->sclass != (String *) NULL) {
				  $$->sclass->ref();
			      }
			  }
		      }
		  }
		  if (!typechecking) {
		      /* the variable could be anything */
		      $$->mod = T_MIXED;
		  }
		}
	| COLON_COLON ident {
		  $$ = Compile::globalVar($2);
		  if (typechecking) {
		      if ($$->mod != T_MIXED && !Config::typechecking()) {
			  /*
			   * global vars might be modified by untypechecked
			   * functions...
			   */
			  $$ = Node::createMon(N_CAST, $$->mod, $$);
			  $$->sclass = $$->l.left->sclass;
			  if ($$->sclass != (String *) NULL) {
			      $$->sclass->ref();
			  }
		      }
		  } else {
		      /* the variable could be anything */
		      $$->mod = T_MIXED;
		  }
		}
	| '(' list_exp ')'
		{ $$ = $2; }
	| function_call '(' opt_arg_list ')'
		{ $$ = Compile::checkcall(Compile::funcall($1, $3),
					  typechecking); }
	| '&' ident '(' opt_arg_list ')'
		{ $$ = Compile::address($2, $4, typechecking); }
	| '&' '(' '*' cast_exp ')' '(' opt_arg_list ')'
		{ $$ = Compile::extend($4, $7, typechecking); }
	| '(' '*' cast_exp ')' '(' opt_arg_list ')'
		{ $$ = Compile::call($3, $6, typechecking); }
	| CATCH '('
		{ Compile::startCond(); }
	  list_exp ')'
		{
		  Compile::endCond();
		  $$ = Node::createMon(N_CATCH, T_STRING, $4);
		}
	| NEW opt_object string_exp
		{ $$ = Compile::newObject($3, (Node *) NULL); }
	| NEW opt_object string_exp '(' opt_arg_list ')'
		{ $$ = Compile::newObject($3, $5); }
	| primary_p2_exp RARROW ident '(' opt_arg_list ')'
		{
		  YYParser::_void($1);
		  $$ = Compile::checkcall(Compile::arrow($1, $3, $5),
					  typechecking);
		}
	| primary_p2_exp LARROW opt_object string_exp
		{ $$ = Compile::instanceOf($1, $4); }
	;

primary_p2_exp
	: primary_p1_exp
	| primary_p2_exp '[' f_list_exp ']'
		{ $$ = YYParser::idx($1, $3); }
	| primary_p2_exp '[' f_opt_list_exp DOT_DOT f_opt_list_exp ']'
		{ $$ = YYParser::range($1, $3, $5); }
	;

postfix_exp
	: primary_p2_exp
	| postfix_exp PLUS_PLUS
		{ $$ = YYParser::postfix(N_PLUS_PLUS, $1, "++"); }
	| postfix_exp MIN_MIN
		{ $$ = YYParser::postfix(N_MIN_MIN, $1, "--"); }
	;

prefix_exp
	: postfix_exp
	| PLUS_PLUS cast_exp
		{ $$ = YYParser::prefix(N_ADD_EQ_1, $2, "++"); }
	| MIN_MIN cast_exp
		{ $$ = YYParser::prefix(N_SUB_EQ_1, $2, "--"); }
	| '-' cast_exp
		{ $$ = YYParser::umin($2); }
	| '+' cast_exp
		{ $$ = $2; }
	| '!' cast_exp
		{
		  YYParser::_void($2);
		  $$ = Compile::_not($2);
		}
	| '~' cast_exp
		{
		  $$ = $2;
		  YYParser::_void($$);
		  if ($$->mod == T_INT) {
		      $$ = YYParser::_xor(N_XOR, $$, Node::createInt(-1), "^");
		  } else if ($$->mod == T_OBJECT || $$->mod == T_CLASS) {
		      $$ = Node::createMon(N_NEG, T_OBJECT, $$);
		  } else {
		      if (typechecking && $$->mod != T_MIXED) {
			  char tnbuf[TNBUFSIZE];

			  Compile::error("bad argument type for ~ (%s)",
					 Value::typeName(tnbuf, $$->mod));
		      }
		      $$ = Node::createMon(N_NEG, T_MIXED, $$);
		  }
		}
	;

cast_exp
	: prefix_exp
	| '(' type_specifier star_list ')' cast_exp
		{
		  $2->mod |= ($3 << REFSHIFT) & T_REF;
		  $$ = YYParser::cast($5, $2);
		}
	;

mult_oper_exp
	: cast_exp
	| mult_oper_exp '*' cast_exp
		{ $$ = YYParser::mult(N_MULT, $1, $3, "*"); }
	| mult_oper_exp '/' cast_exp
		{ $$ = YYParser::mdiv(N_DIV, $1, $3, "/"); }
	| mult_oper_exp '%' cast_exp
		{ $$ = YYParser::mod(N_MOD, $1, $3, "%"); }
	;

add_oper_exp
	: mult_oper_exp
	| add_oper_exp '+' mult_oper_exp
		{ $$ = YYParser::add(N_ADD, $1, $3, "+"); }
	| add_oper_exp '-' mult_oper_exp
		{ $$ = YYParser::sub(N_SUB, $1, $3, "-"); }
	;

shift_oper_exp
	: add_oper_exp
	| shift_oper_exp LSHIFT add_oper_exp
		{ $$ = YYParser::lshift(N_LSHIFT, $1, $3, "<<"); }
	| shift_oper_exp RSHIFT add_oper_exp
		{ $$ = YYParser::rshift(N_RSHIFT, $1, $3, ">>"); }
	;

rel_oper_exp
	: shift_oper_exp
	| rel_oper_exp '<' shift_oper_exp
		{ $$ = YYParser::rel(N_LT, $$, $3, "<"); }
	| rel_oper_exp '>' shift_oper_exp
		{ $$ = YYParser::rel(N_GT, $$, $3, ">"); }
	| rel_oper_exp LE shift_oper_exp
		{ $$ = YYParser::rel(N_LE, $$, $3, "<="); }
	| rel_oper_exp GE shift_oper_exp
		{ $$ = YYParser::rel(N_GE, $$, $3, ">="); }
	;

equ_oper_exp
	: rel_oper_exp
	| equ_oper_exp EQ rel_oper_exp
		{ $$ = YYParser::eq($1, $3); }
	| equ_oper_exp NE rel_oper_exp
		{ $$ = Compile::_not(YYParser::eq($1, $3)); }
	;

bitand_oper_exp
	: equ_oper_exp
	| bitand_oper_exp '&' equ_oper_exp
		{ $$ = YYParser::_and(N_AND, $1, $3, "&"); }
	;

bitxor_oper_exp
	: bitand_oper_exp
	| bitxor_oper_exp '^' bitand_oper_exp
		{ $$ = YYParser::_xor(N_XOR, $1, $3, "^"); }
	;

bitor_oper_exp
	: bitxor_oper_exp
	| bitor_oper_exp '|' bitxor_oper_exp
		{ $$ = YYParser::_or(N_OR, $1, $3, "|"); }
	;

and_oper_exp
	: bitor_oper_exp
	| and_oper_exp LAND
		{ Compile::startCond(); }
	  bitor_oper_exp
		{
		  Compile::endCond();
		  $$ = YYParser::land($1, $4);
		}
	;

or_oper_exp
	: and_oper_exp
	| or_oper_exp LOR
		{ Compile::startCond(); }
	  and_oper_exp
		{
		  Compile::endCond();
		  $$ = YYParser::lor($1, $4);
		}
	;

cond_exp
	: or_oper_exp
	| or_oper_exp '?'
		{ Compile::startCond(); }
	  list_exp ':'
		{ Compile::startCond2(); }
	  cond_exp
		{
		  Compile::matchCond();
		  Compile::saveCond();
		  $$ = YYParser::quest($1, $4, $7);
		}
	;

exp
	: cond_exp
	| cond_exp '=' exp
		{ $$ = YYParser::assign(Compile::assign($1), $3); }
	| cond_exp PLUS_EQ exp
		{
		  $$ = YYParser::add(N_ADD_EQ, Compile::lvalue($1, "+="), $3,
				     "+=");
		}
	| cond_exp MIN_EQ exp
		{
		  $$ = YYParser::sub(N_SUB_EQ, Compile::lvalue($1, "-="), $3,
				     "-=");
		}
	| cond_exp MULT_EQ exp
		{
		  $$ = YYParser::mult(N_MULT_EQ, Compile::lvalue($1, "*="), $3,
				      "*=");
		}
	| cond_exp DIV_EQ exp
		{
		  $$ = YYParser::mdiv(N_DIV_EQ, Compile::lvalue($1, "/="), $3,
				      "/=");
		}
	| cond_exp MOD_EQ exp
		{
		  $$ = YYParser::mod(N_MOD_EQ, Compile::lvalue($1, "%="), $3,
				     "%=");
		}
	| cond_exp LSHIFT_EQ exp
		{
		  $$ = YYParser::lshift(N_LSHIFT_EQ, Compile::lvalue($1, "<<="),
					$3, "<<=");
		}
	| cond_exp RSHIFT_EQ exp
		{
		  $$ = YYParser::rshift(N_RSHIFT_EQ, Compile::lvalue($1, ">>="),
					$3, ">>=");
		}
	| cond_exp AND_EQ exp
		{
		  $$ = YYParser::_and(N_AND_EQ, Compile::lvalue($1, "&="), $3,
				      "&=");
		}
	| cond_exp XOR_EQ exp
		{
		  $$ = YYParser::_xor(N_XOR_EQ, Compile::lvalue($1, "^="), $3,
				      "^=");
		}
	| cond_exp OR_EQ exp
		{
		  $$ = YYParser::_or(N_OR_EQ, Compile::lvalue($1, "|="), $3,
				     "|=");
		}
	;

list_exp
	: exp
	| list_exp ',' exp
		{ $$ = YYParser::comma($1, $3); }
	;

opt_list_exp
	: /* empty */
		{ $$ = (Node *) NULL; }
	| list_exp
	;

f_list_exp
	: list_exp
		{ YYParser::_void($$ = $1); }
	;

f_opt_list_exp
	: opt_list_exp
		{ YYParser::_void($$ = $1); }
	;

arg_list
	: exp	{ YYParser::_void($$ = $1); }
	| arg_list ',' exp
		{
		  YYParser::_void($3);
		  $$ = Node::createBin(N_PAIR, 0, $1, $3);
		}
	;

opt_arg_list
	: /* empty */
		{ $$ = (Node *) NULL; }
	| arg_list
	| arg_list ELLIPSIS
		{
		  $$ = $1;
		  if ($$->type == N_PAIR) {
		      $$->r.right = Node::createMon(N_SPREAD, -1, $$->r.right);
		  } else {
		      $$ = Node::createMon(N_SPREAD, -1, $$);
		  }
		}
	;

opt_arg_list_comma
	: /* empty */
		{ $$ = (Node *) NULL; }
	| arg_list
	| arg_list ','
		{ $$ = $1; }
	;

assoc_exp
	: exp ':' exp
		{
		  YYParser::_void($1);
		  YYParser::_void($3);
		  $$ = Node::createBin(N_COMMA, 0, $1, $3);
		}
	;

assoc_arg_list
	: assoc_exp
	| assoc_arg_list ',' assoc_exp
		{ $$ = Node::createBin(N_PAIR, 0, $1, $3); }
	;

opt_assoc_arg_list_comma
	: /* empty */
		{ $$ = (Node *) NULL; }
	| assoc_arg_list
	| assoc_arg_list ','
		{ $$ = $1; }
	;

%%

/*
 * if the argument is of type void, an error will result
 */
void YYParser::_void(Node *n)
{
    if (n != (Node *) NULL && n->mod == T_VOID) {
	Compile::error("void value not ignored");
	n->mod = T_MIXED;
    }
}

/*
 * typecheck the argument of a unary operator
 */
bool YYParser::unary(Node *n, const char *name)
{
    char tnbuf[TNBUFSIZE];

    _void(n);
    if (typechecking && !T_ARITHMETIC(n->mod) && n->mod != T_MIXED) {
	Compile::error("bad argument type for %s (%s)", name,
		       Value::typeName(tnbuf, n->mod));
	n->mod = T_MIXED;
	return FALSE;
    }
    return TRUE;
}

/*
 * handle a postfix assignment operator
 */
Node *YYParser::postfix(int op, Node *n, const char *name)
{
    unary(n, name);
    return Node::createMon((n->mod == T_INT) ? op + 1 : op, n->mod,
			   Compile::lvalue(n, name));
}

/*
 * handle a prefix assignment operator
 */
Node *YYParser::prefix(int op, Node *n, const char *name)
{
    unsigned short type;

    if (n->mod == T_OBJECT || n->mod == T_CLASS) {
	type = T_OBJECT;
    } else {
	unary(n, name);
	type = n->mod;
    }
    return Node::createMon((type == T_INT) ? op + 1 : op, type,
			   Compile::lvalue(n, name));
}

/*
 * cast an expression to a type
 */
Node *YYParser::cast(Node *n, Node *type)
{
    Float flt;
    LPCint i;
    char *p, buffer[FLOAT_BUFFER];

    if (type->mod != n->mod) {
	switch (type->mod) {
	case T_INT:
	    switch (n->type) {
	    case N_FLOAT:
		/* cast float constant to int */
		NFLT_GET(n, flt);
		return Node::createInt(flt.ftoi());

	    case N_STR:
		/* cast string to int */
		p = n->l.string->text;
		i = strtoint(&p);
		if (p == n->l.string->text + n->l.string->len) {
		    return Node::createInt(i);
		} else {
		    Compile::error("cast of invalid string constant");
		    n->mod = T_MIXED;
		}
		break;

	    case N_TOFLOAT:
	    case N_TOSTRING:
		if (n->l.left->type == N_INT) {
		    /* (int) (float) i, (int) (string) i */
		    return n->l.left;
		}
		/* fall through */
	    default:
		if (n->mod == T_FLOAT || n->mod == T_STRING ||
		    n->mod == T_MIXED) {
		    return Node::createMon(N_TOINT, T_INT, n);
		}
		break;
	    }
	    break;

	case T_FLOAT:
	    switch (n->type) {
	    case N_INT:
		/* cast int constant to float */
		Float::itof(n->l.number, &flt);
		return Node::createFloat(&flt);

	    case N_STR:
		/* cast string to float */
		p = n->l.string->text;
		if (Float::atof(&p, &flt) &&
		    p == n->l.string->text + n->l.string->len) {
		    return Node::createFloat(&flt);
		} else {
		    yyerror("cast of invalid string constant");
		    n->mod = T_MIXED;
		}
		break;

	    case N_TOSTRING:
		if (n->l.left->mod == T_INT) {
		    return Node::createMon(N_TOFLOAT, T_FLOAT, n->l.left);
		}
		/* fall through */
	    default:
		if (n->mod == T_INT || n->mod == T_STRING || n->mod == T_MIXED)
		{
		    return Node::createMon(N_TOFLOAT, T_FLOAT, n);
		}
		break;
	    }
	    break;

	case T_STRING:
	    switch (n->type) {
	    case N_INT:
		/* cast int constant to string */
		snprintf(buffer, sizeof(buffer), "%ld", (long) n->l.number);
		return Node::createStr(String::create(buffer, strlen(buffer)));

	    case N_FLOAT:
		/* cast float constant to string */
		NFLT_GET(n, flt);
		flt.ftoa(buffer);
		return Node::createStr(String::create(buffer, strlen(buffer)));

	    default:
		if (n->mod == T_INT || n->mod == T_FLOAT || n->mod == T_MIXED) {
		    return Node::createMon(N_TOSTRING, T_STRING, n);
		}
		break;
	    }
	    break;
	}

	if (type->mod == T_MIXED || (type->mod & T_TYPE) == T_VOID) {
	    /* (mixed), (void), (void *) */
	    Compile::error("cannot cast to %s",
			   Value::typeName(buffer, type->mod));
	    n->mod = T_MIXED;
	} else if ((type->mod & T_REF) < (n->mod & T_REF)) {
	    /* (mixed *) of (mixed **) */
	    Compile::error("illegal cast of array type (%s)",
			   Value::typeName(buffer, n->mod));
	} else if ((n->mod & T_TYPE) != T_MIXED &&
		   ((type->mod & T_TYPE) != T_CLASS ||
		    ((n->mod & T_TYPE) != T_OBJECT &&
		     (n->mod & T_TYPE) != T_CLASS) ||
		    (type->mod & T_REF) != (n->mod & T_REF))) {
	    /* can only cast from mixed, or object/class to class */
	    Compile::error("cast of invalid type (%s)",
			   Value::typeName(buffer, n->mod));
	} else {
	    if ((type->mod & T_REF) == 0 || (n->mod & T_REF) == 0) {
		/* runtime cast */
		n = Node::createMon(N_CAST, type->mod, n);
	    } else {
		n->mod = type->mod;
	    }
	    n->sclass = type->sclass;
	    if (n->sclass != (String *) NULL) {
		n->sclass->ref();
	    }
	}
    } else if (type->mod == T_CLASS && type->sclass->cmp(n->sclass) != 0) {
	/*
	 * cast to different object class
	 */
	n = Node::createMon(N_CAST, type->mod, n);
	n->sclass = type->sclass;
	if (n->sclass != (String *) NULL) {
	    n->sclass->ref();
	}
    }
    return n;
}

/*
 * handle the [ ] operator
 */
Node *YYParser::idx(Node *n1, Node *n2)
{
    char tnbuf[TNBUFSIZE];
    unsigned short type;

    if (n1->type == N_STR && n2->type == N_INT) {
	/* str [ int ] */
	if (n2->l.number < 0 || n2->l.number >= (LPCint) n1->l.string->len) {
	    Compile::error("string index out of range");
	} else {
	    n2->l.number =
		UCHAR(n1->l.string->text[n1->l.string->index(n2->l.number)]);
	}
	return n2;
    }

    if ((n1->mod & T_REF) != 0) {
	/*
	 * array
	 */
	if (typechecking) {
	    type = n1->mod - (1 << REFSHIFT);
	    if (n2->mod != T_INT && n2->mod != T_MIXED) {
		Compile::error("bad index type (%s)",
			       Value::typeName(tnbuf, n2->mod));
	    }
	    if (type != T_MIXED &&
		(n1->type != N_FUNC ||
		 n1->r.number != (((LPCint) KFCALL << 24) | KF_CALL_TRACE))) {
		/* you can't trust these arrays */
		n2 = Node::createMon(N_CAST, type,
				     Node::createBin(N_INDEX, type, n1, n2));
		n2->sclass = n1->sclass;
		if (n2->sclass != (String *) NULL) {
		    n2->sclass->ref();
		}
		return n2;
	    }
	}
	type = T_MIXED;
    } else if (n1->mod == T_STRING) {
	/*
	 * string
	 */
	if (typechecking && n2->mod != T_INT && n2->mod != T_MIXED) {
	    Compile::error("bad index type (%s)",
			   Value::typeName(tnbuf, n2->mod));
	}
	type = T_INT;
    } else {
	if (typechecking && n1->mod != T_OBJECT && n1->mod != T_CLASS &&
	    n1->mod != T_MAPPING && n1->mod != T_MIXED) {
	    Compile::error("bad indexed type (%s)",
			   Value::typeName(tnbuf, n1->mod));
	}
	type = T_MIXED;
    }
    return Node::createBin(N_INDEX, type, n1, n2);
}

/*
 * handle the [ .. ] operator
 */
Node *YYParser::range(Node *n1, Node *n2, Node *n3)
{
    unsigned short type;

    if (n1->type == N_STR && (n2 == (Node *) NULL || n2->type == N_INT) &&
	(n3 == (Node *) NULL || n3->type == N_INT)) {
	LPCint from, to;

	/* str [ int .. int ] */
	from = (n2 == (Node *) NULL) ? 0 : n2->l.number;
	to = (n3 == (Node *) NULL) ? n1->l.string->len - 1 : n3->l.number;
	if (from < 0 || from > to + 1 || to >= n1->l.string->len) {
	    Compile::error("invalid string range");
	} else {
	    return Node::createStr(n1->l.string->range(from, to));
	}
    }

    type = T_MIXED;
    if (n1->mod == T_OBJECT || n1->mod == T_CLASS) {
	type = T_OBJECT;
    } else if (n1->mod == T_MAPPING) {
	type = T_MAPPING;
    } else if (typechecking && n1->mod != T_MIXED) {
	char tnbuf[TNBUFSIZE];

	/* indices */
	if (n2 != (Node *) NULL && n2->mod != T_INT && n2->mod != T_MIXED) {
	    Compile::error("bad index type (%s)",
			   Value::typeName(tnbuf, n2->mod));
	}
	if (n3 != (Node *) NULL && n3->mod != T_INT && n3->mod != T_MIXED) {
	    Compile::error("bad index type (%s)",
			   Value::typeName(tnbuf, n3->mod));
	}
	/* range */
	if ((n1->mod & T_REF) == 0 && n1->mod != T_STRING && n1->mod != T_MIXED)
	{
	    Compile::error("bad indexed type (%s)",
			   Value::typeName(tnbuf, n1->mod));
	}
	type = n1->mod;
    }

    return Node::createBin(N_RANGE, type, n1,
			   Node::createBin(N_PAIR, 0, n2, n3));
}

/*
 * handle a binary int operator
 */
Node *YYParser::bini(int op, Node *n1, Node *n2, const char *name)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    unsigned short type;

    _void(n1);
    _void(n2);

    type = T_MIXED;
    if (n1->mod == T_OBJECT || n1->mod == T_CLASS) {
	type = T_OBJECT;
    } else if (n1->mod == T_INT && (n2->mod == T_INT || n2->mod == T_MIXED)) {
	type = T_INT;
    } else if (typechecking && n1->mod != T_MIXED) {
	Compile::error("bad argument types for %s (%s, %s)", name,
		       Value::typeName(tnbuf1, n1->mod),
		       Value::typeName(tnbuf2, n2->mod));
    }
    if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return Node::createBin(op, type, n1, n2);
}


/*
 * handle a binary arithmetic operator
 */
Node *YYParser::bina(int op, Node *n1, Node *n2, const char *name)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    unsigned short type;

    _void(n1);
    _void(n2);

    type = T_MIXED;
    if (n1->mod == T_OBJECT || n1->mod == T_CLASS) {
	type = T_OBJECT;
    } else if (n1->mod == T_INT && (n2->mod == T_INT || n2->mod == T_MIXED)) {
	if (n1->mod == T_INT && n2->mod == T_INT) {
	    op++;
	}
	type = T_INT;
    } else if (n1->mod == T_FLOAT && (n2->mod == T_FLOAT || n2->mod == T_MIXED))
    {
	type = T_FLOAT;
	if (n1->mod == T_FLOAT && n2->mod == T_FLOAT) {
	    op += 2;
	}
    } else if (typechecking && n1->mod != T_MIXED) {
	Compile::error("bad argument types for %s (%s, %s)", name,
		       Value::typeName(tnbuf1, n1->mod),
		       Value::typeName(tnbuf2, n2->mod));
    }

    return Node::createBin(op, type, n1, n2);
}

/*
 * handle the * *= operators
 */
Node *YYParser::mult(int op, Node *n1, Node *n2, const char *name)
{
    Float f1, f2;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i * i */
	n1->l.number *= n2->l.number;
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	f1.mult(f2);
	NFLT_PUT(n1, f1);
	return n1;
    }
    return bina(op, n1, n2, name);
}

/*
 * handle the / /= operators
 */
Node *YYParser::mdiv(int op, Node *n1, Node *n2, const char *name)
{
    Float f1, f2;

    if (n1->type == N_INT && n2->type == N_INT) {
	LPCint i, d;

	/* i / i */
	i = n1->l.number;
	d = n2->l.number;
	if (d == 0) {
	    /* i / 0 */
	    Compile::error("division by zero");
	    return n1;
	}
	n1->l.number = i / d;
	return n1;
    } else if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	/* f / f */
	if (NFLT_ISZERO(n2)) {
	    /* f / 0.0 */
	    Compile::error("division by zero");
	    return n1;
	}
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	f1.div(f2);
	NFLT_PUT(n1, f1);
	return n1;
    }

    return bina(op, n1, n2, name);
}

/*
 * handle the % %= operators
 */
Node *YYParser::mod(int op, Node *n1, Node *n2, const char *name)
{
    if (n1->type == N_INT && n2->type == N_INT) {
	LPCint i, d;

	/* i % i */
	i = n1->l.number;
	d = n2->l.number;
	if (d == 0) {
	    /* i % 0 */
	    Compile::error("modulus by zero");
	    return n1;
	}
	n1->l.number = i % d;
	return n1;
    }

    return bini(op, n1, n2, name);
}

/*
 * handle the + += operators, possibly rearranging the order
 * of the expression
 */
Node *YYParser::add(int op, Node *n1, Node *n2, const char *name)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    Float f1, f2;
    unsigned short type;

    _void(n1);
    _void(n2);

    if (n1->mod == T_STRING) {
	if (n2->mod == T_INT || n2->mod == T_FLOAT ||
	    (n2->mod == T_MIXED && typechecking)) {
	    n2 = cast(n2, Node::createType(T_STRING, (String *) NULL));
	}
    } else if (n2->mod == T_STRING && op == N_ADD) {
	if (n1->mod == T_INT || n1->mod == T_FLOAT) {
	    n1 = cast(n1, Node::createType(T_STRING, (String *) NULL));
	}
    }

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i + i */
	n1->l.number += n2->l.number;
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	/* f + f */
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	f1.add(f2);
	NFLT_PUT(n1, f1);
	return n1;
    }
    if (n1->type == N_STR && n2->type == N_STR) {
	/* s + s */
	return Node::createStr(n1->l.string->add(n2->l.string));
    }

    if (n1->mod == T_OBJECT || n1->mod == T_CLASS) {
	type = T_OBJECT;
    } else if ((type=Compile::matchType(n1->mod, n2->mod)) == T_NIL) {
	type = T_MIXED;
	if (typechecking) {
	    Compile::error("bad argument types for %s (%s, %s)", name,
			   Value::typeName(tnbuf1, n1->mod),
			   Value::typeName(tnbuf2, n2->mod));
	}
    } else if (type == T_INT) {
	op++;
    } else if (op == N_ADD_EQ && n1->mod != n2->mod) {
	type = n1->mod;
	if (n1->mod == T_INT) {
	    n2 = Node::createMon(N_CAST, T_INT, n2);
	    type = T_INT;
	    op++;
	} else if (n1->mod == T_FLOAT) {
	    n2 = Node::createMon(N_CAST, T_FLOAT, n2);
	    type = T_FLOAT;
	}
    }
    return Node::createBin(op, type, n1, n2);
}

/*
 * handle the - -= operators
 */
Node *YYParser::sub(int op, Node *n1, Node *n2, const char *name)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    Float f1, f2;
    unsigned short type;

    _void(n1);
    _void(n2);

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i - i */
	n1->l.number -= n2->l.number;
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	/* f - f */
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	f1.sub(f2);
	NFLT_PUT(n1, f1);
	return n1;
    }

    if (n1->mod == T_OBJECT || n1->mod == T_CLASS) {
	type = T_OBJECT;
    } else if ((type=Compile::matchType(n1->mod, n2->mod)) == T_NIL ||
	       type == T_STRING || type == T_MAPPING) {
	if ((type=n1->mod) != T_MAPPING ||
	    (n2->mod != T_MIXED && (n2->mod & T_REF) == 0)) {
	    type = T_MIXED;
	    if (typechecking) {
		Compile::error("bad argument types for %s (%s, %s)", name,
			       Value::typeName(tnbuf1, n1->mod),
			       Value::typeName(tnbuf2, n2->mod));
	    }
	}
    } else if (type == T_INT) {
	op++;
    } else if (type == T_MIXED) {
	type = n1->mod;
    } else if (n1->mod == T_MIXED && (n2->mod & T_REF)) {
	type = T_MIXED;
    }
    return Node::createBin(op, type, n1, n2);
}

/*
 * handle unary minus
 */
Node *YYParser::umin(Node *n)
{
    Float flt;

    if (n->mod == T_OBJECT || n->mod == T_CLASS) {
	return Node::createMon(N_UMIN, T_OBJECT, n);
    } else if (n->mod == T_MIXED) {
	return Node::createMon(N_UMIN, T_MIXED, n);
    } else if (unary(n, "unary -")) {
	if (n->mod == T_FLOAT) {
	    flt.initZero();
	    n = sub(N_SUB, Node::createFloat(&flt), n, "-");
	} else {
	    n = sub(N_SUB, Node::createInt(0), n, "-");
	}
    }
    return n;
}

/*
 * handle the << <<= operators
 */
Node *YYParser::lshift(int op, Node *n1, Node *n2, const char *name)
{
    if (n2->type == N_INT) {
	if (n2->l.number < 0) {
	    Compile::error("negative left shift");
	    n2->l.number = 0;
	}
	if (n1->type == N_INT) {
	    /* i << i */
	    n1->l.number = (n2->l.number < LPCINT_BITS) ?
			    (LPCuint) n1->l.number << n2->l.number : 0;
	    return n1;
	}
    }

    return bini(op, n1, n2, name);
}

/*
 * handle the >> >>= operators
 */
Node *YYParser::rshift(int op, Node *n1, Node *n2, const char *name)
{
    if (n2->type == N_INT) {
	if (n2->l.number < 0) {
	    Compile::error("negative right shift");
	    n2->l.number = 0;
	}
	if (n1->type == N_INT) {
	    /* i >> i */
	    n1->l.number = (n2->l.number < LPCINT_BITS) ?
			    (LPCuint) n1->l.number >> n2->l.number : 0;
	    return n1;
	}
    }

    return bini(op, n1, n2, name);
}

/*
 * handle the < > <= >= operators
 */
Node *YYParser::rel(int op, Node *n1, Node *n2, const char *name)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];

    _void(n1);
    _void(n2);

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i . i */
	switch (op) {
	case N_GE:
	    n1->l.number = (n1->l.number >= n2->l.number);
	    break;

	case N_GT:
	    n1->l.number = (n1->l.number > n2->l.number);
	    break;

	case N_LE:
	    n1->l.number = (n1->l.number <= n2->l.number);
	    break;

	case N_LT:
	    n1->l.number = (n1->l.number < n2->l.number);
	    break;
	}
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	Float f1, f2;

	/* f . f */
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);

	switch (op) {
	case N_GE:
	    return Node::createInt((f1.cmp(f2) >= 0));

	case N_GT:
	    return Node::createInt((f1.cmp(f2) > 0));

	case N_LE:
	    return Node::createInt((f1.cmp(f2) <= 0));

	case N_LT:
	    return Node::createInt((f1.cmp(f2) < 0));
	}
	return n1;
    }
    if (n1->type == N_STR && n2->type == N_STR) {
	/* s . s */
	switch (op) {
	case N_GE:
	    return Node::createInt((n1->l.string->cmp(n2->l.string) >= 0));

	case N_GT:
	    return Node::createInt((n1->l.string->cmp(n2->l.string) > 0));

	case N_LE:
	    return Node::createInt((n1->l.string->cmp(n2->l.string) <= 0));

	case N_LT:
	    return Node::createInt((n1->l.string->cmp(n2->l.string) < 0));
	}
    }

    if (n1->mod != T_OBJECT && n1->mod != T_CLASS && n1->mod != T_MIXED &&
	typechecking &&
	((n1->mod != n2->mod && n2->mod != T_MIXED) || !T_ARITHSTR(n1->mod) ||
	 (!T_ARITHSTR(n2->mod) && n2->mod != T_MIXED))) {
	Compile::error("bad argument types for %s (%s, %s)", name,
		       Value::typeName(tnbuf1, n1->mod),
		       Value::typeName(tnbuf2, n2->mod));
    } else if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return Node::createBin(op, T_INT, n1, n2);
}

/*
 * handle the == operator
 */
Node *YYParser::eq(Node *n1, Node *n2)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    Float f1, f2;
    int op;

    _void(n1);
    _void(n2);

    switch (n1->type) {
    case N_INT:
	if (n2->type == N_INT) {
	    /* i == i */
	    n1->l.number = (n1->l.number == n2->l.number);
	    return n1;
	}
	if (nil_node == N_INT && n1->l.number == 0 && n2->type == N_STR) {
	    /* nil == str */
	    return Node::createInt(FALSE);
	}
	break;

    case N_FLOAT:
	if (n2->type == N_FLOAT) {
	    /* f == f */
	    NFLT_GET(n1, f1);
	    NFLT_GET(n2, f2);
	    return Node::createInt((f1.cmp(f2) == 0));
	}
	break;

    case N_STR:
	if (n2->type == N_STR) {
	    /* s == s */
	    return Node::createInt((n1->l.string->cmp(n2->l.string) == 0));
	}
	if (n2->type == nil_node && n2->l.number == 0) {
	    /* s == nil */
	    return Node::createInt(FALSE);
	}
	break;

    case N_NIL:
	if (n2->type == N_NIL) {
	    /* nil == nil */
	    return Node::createInt(TRUE);
	}
	if (n2->type == N_STR) {
	    /* nil == str */
	    return Node::createInt(FALSE);
	}
	break;
    }

    op = N_EQ;
    if (Compile::matchType(n1->mod, n2->mod) == T_NIL &&
	(!Compile::nil(n1) || !T_POINTER(n2->mod)) &&
	(!Compile::nil(n2) || !T_POINTER(n1->mod))) {
	if (typechecking) {
	    Compile::error("incompatible types for equality (%s, %s)",
			   Value::typeName(tnbuf1, n1->mod),
			   Value::typeName(tnbuf2, n2->mod));
	}
    } else if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return Node::createBin(op, T_INT, n1, n2);
}

/*
 * handle the & &= operators
 */
Node *YYParser::_and(int op, Node *n1, Node *n2, const char *name)
{
    unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i & i */
	n1->l.number &= n2->l.number;
	return n1;
    }
    if ((((type=n1->mod) == T_MIXED || type == T_MAPPING) &&
	 ((n2->mod & T_REF) != 0 || n2->mod == T_MIXED)) ||
	((type=Compile::matchType(n1->mod, n2->mod)) & T_REF) != 0) {
	/*
	 * possibly array & array or mapping & array
	 */
	return Node::createBin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * handle the ^ ^= operators
 */
Node *YYParser::_xor(int op, Node *n1, Node *n2, const char *name)
{
    unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i ^ i */
	n1->l.number ^= n2->l.number;
	return n1;
    }
    if (((type=n1->mod) == T_MIXED && n2->mod == T_MIXED) ||
	((type=Compile::matchType(n1->mod, n2->mod)) & T_REF) != 0) {
	/*
	 * possibly array ^ array
	 */
	return Node::createBin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * handle the | |= operators
 */
Node *YYParser::_or(int op, Node *n1, Node *n2, const char *name)
{
    unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i | i */
	n1->l.number |= n2->l.number;
	return n1;
    }
    if (((type=n1->mod) == T_MIXED && n2->mod == T_MIXED) ||
	((type=Compile::matchType(n1->mod, n2->mod)) & T_REF) != 0) {
	/*
	 * possibly array | array
	 */
	return Node::createBin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * handle the && operator
 */
Node *YYParser::land(Node *n1, Node *n2)
{
    _void(n1);
    _void(n2);

    if ((n1->flags & F_CONST) && (n2->flags & F_CONST)) {
	n1 = Compile::tst(n1);
	n2 = Compile::tst(n2);
	n1->l.number &= n2->l.number;
	return n1;
    }

    return Node::createBin(N_LAND, T_INT, n1, n2);
}

/*
 * handle the || operator
 */
Node *YYParser::lor(Node *n1, Node *n2)
{
    _void(n1);
    _void(n2);

    if ((n1->flags & F_CONST) && (n2->flags & F_CONST)) {
	n1 = Compile::tst(n1);
	n2 = Compile::tst(n2);
	n1->l.number |= n2->l.number;
	return n1;
    }

    return Node::createBin(N_LOR, T_INT, n1, n2);
}

/*
 * handle the ? : operator
 */
Node *YYParser::quest(Node *n1, Node *n2, Node *n3)
{
    unsigned short type;

    _void(n1);

    if ((n2->flags & F_CONST) && n3->type == n2->type) {
	switch (n1->type) {
	case N_INT:
	    return (n1->l.number == 0) ? n3 : n2;

	case N_FLOAT:
	    return (NFLT_ISZERO(n1)) ? n3 : n2;

	case N_STR:
	    return n2;

	case N_NIL:
	    return n3;
	}
    }

    type = T_MIXED;
    if (Compile::nil(n2) && T_POINTER(n3->mod)) {
	/*
	 * expr ? nil : expr
	 */
	type = n3->mod;
    } else if (Compile::nil(n3) && T_POINTER(n2->mod)) {
	/*
	 * expr ? expr : nil;
	 */
	type = n2->mod;
    } else if (typechecking) {
	/*
	 * typechecked
	 */
	if (n2->mod == T_VOID || n3->mod == T_VOID) {
	    /* result can never be used */
	    type = T_VOID;
	} else {
	    type = Compile::matchType(n2->mod, n3->mod);
	    if (type == T_NIL) {
		/* no typechecking here, just let the result be mixed */
		type = T_MIXED;
	    }
	}
    }

    n1 = Node::createBin(N_QUEST, type, n1, Node::createBin(N_PAIR, 0, n2, n3));
    if ((type & T_TYPE) == T_CLASS) {
	if (n2->sclass == (String *) NULL) {
	    n1->sclass = n3->sclass;
	    if (n1->sclass != (String *) NULL) {
		n1->sclass->ref();
	    }
	} else if (n3->sclass == (String *) NULL ||
		   n2->sclass->cmp(n3->sclass) == 0) {
	    n1->sclass = n2->sclass;
	    if (n1->sclass != (String *) NULL) {
		n1->sclass->ref();
	    }
	} else {
	    /* downgrade to object */
	    n1->type = (n1->type & T_REF) | T_OBJECT;
	}
    }
    return n1;
}

/*
 * handle the assignment operator
 */
Node *YYParser::assign(Node *n1, Node *n2)
{
    char tnbuf1[TNBUFSIZE], tnbuf2[TNBUFSIZE];
    Node *n, *m;
    unsigned short type;

    if (n1->type == N_AGGR) {
	/*
	 * ({ a, b }) = array;
	 */
	if (typechecking) {
	    type = n2->mod;
	    if ((n2->mod & T_REF) != 0) {
		type -= 1 << REFSHIFT;
		if (type != T_MIXED) {
		    n = Node::createMon(N_TYPE, type, (Node *) NULL);
		    n->sclass = n2->sclass;
		    if (n->sclass != (String *) NULL) {
			n->sclass->ref();
		    }
		    n1->r.right = n;
		}
	    } else if (type != T_MIXED) {
		Compile::error("incompatible types for = (%s, %s)",
			       Value::typeName(tnbuf1, n1->mod),
			       Value::typeName(tnbuf2, type));
		type = T_MIXED;
	    }

	    n = n1->l.left;
	    while (n != (Node *) NULL) {
		if (n->type == N_PAIR) {
		    m = n->l.left;
		    n = n->r.right;
		} else {
		    m = n;
		    n = (Node *) NULL;
		}
		if (Compile::matchType(m->mod, type) == T_NIL) {
		    Compile::error("incompatible types for = (%s, %s)",
				   Value::typeName(tnbuf1, m->mod),
				   Value::typeName(tnbuf2, type));
		}
	    }
	}
	n1 = Node::createBin(N_ASSIGN, n2->mod, n1, n2);
	n1->sclass = n2->sclass;
	if (n1->sclass != (String *) NULL) {
	    n1->sclass->ref();
	}
	return n1;
    } else {
	if (typechecking && (!Compile::nil(n2) || !T_POINTER(n1->mod))) {
	    /*
	     * typechecked
	     */
	    if (Compile::matchType(n1->mod, n2->mod) == T_NIL) {
		Compile::error("incompatible types for = (%s, %s)",
			       Value::typeName(tnbuf1, n1->mod),
			       Value::typeName(tnbuf2, n2->mod));
	    } else if ((n1->mod != T_MIXED && n2->mod == T_MIXED) ||
		       (n1->mod == T_CLASS &&
			(n2->mod != T_CLASS ||
			 n1->sclass->cmp(n2->sclass) != 0))) {
		n2 = Node::createMon(N_CAST, n1->mod, n2);
		n2->sclass = n1->sclass;
		if (n2->sclass != (String *) NULL) {
		    n2->sclass->ref();
		}
	    }
	}

	n2 = Node::createBin(N_ASSIGN, n1->mod, n1, n2);
	n2->sclass = n1->sclass;
	if (n2->sclass != (String *) NULL) {
	    n2->sclass->ref();
	}
	return n2;
    }
}

/*
 * handle the comma operator, rearranging the order of the
 * expression if needed
 */
Node *YYParser::comma(Node *n1, Node *n2)
{
    if (n2->type == N_COMMA) {
	/* a, (b, c) --> (a, b), c */
	n2->l.left = comma(n1, n2->l.left);
	return n2;
    } else {
	n1 = Node::createBin(N_COMMA, n2->mod, n1, n2);
	n1->sclass = n2->sclass;
	    if (n1->sclass != (String *) NULL) {
		n1->sclass->ref();
	    }
	return n1;
    }
}
