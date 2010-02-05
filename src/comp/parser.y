/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
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
# include "interpret.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "compile.h"

# define yylex		pp_gettok
# define yyerror	c_error

int nerrors;			/* number of errors encountered so far */
static int ndeclarations;	/* number of declarations */
static int nstatements;		/* number of statements in current function */
static bool typechecking;	/* does the current function have it? */

static void  t_void	P((node*));
static bool  t_unary	P((node*, char*));
static node *uassign	P((int, node*, char*));
static node *cast	P((node*, node*));
static node *idx	P((node*, node*));
static node *range	P((node*, node*, node*));
static node *bini	P((int, node*, node*, char*));
static node *bina	P((int, node*, node*, char*));
static node *mult	P((int, node*, node*, char*));
static node *mdiv	P((int, node*, node*, char*));
static node *mod	P((int, node*, node*, char*));
static node *add	P((int, node*, node*, char*));
static node *sub	P((int, node*, node*, char*));
static node *umin	P((node*));
static node *lshift	P((int, node*, node*, char*));
static node *rshift	P((int, node*, node*, char*));
static node *rel	P((int, node*, node*, char*));
static node *eq		P((node*, node*));
static node *and	P((int, node*, node*, char*));
static node *xor	P((int, node*, node*, char*));
static node *or		P((int, node*, node*, char*));
static node *land	P((node*, node*));
static node *lor	P((node*, node*));
static node *quest	P((node*, node*, node*));
static node *assign	P((node*, node*));
static node *comma	P((node*, node*));

%}


/*
 * Keywords. The order is determined in tokenz() in the lexical scanner.
 */
%token STRING NOMASK NIL BREAK ELSE CASE WHILE DEFAULT STATIC CONTINUE INT
       RLIMITS FLOAT FOR INHERIT VOID IF CATCH SWITCH VARARGS MAPPING PRIVATE
       DO RETURN ATOMIC MIXED OBJECT

/*
 * composite tokens
 */
%token LARROW RARROW PLUS_PLUS MIN_MIN LSHIFT RSHIFT LE GE EQ NE LAND LOR
       PLUS_EQ MIN_EQ MULT_EQ DIV_EQ MOD_EQ LSHIFT_EQ RSHIFT_EQ AND_EQ
       XOR_EQ OR_EQ COLON_COLON DOT_DOT ELLIPSIS STRING_CONST IDENTIFIER

%union {
    Int number;			/* lex input */
    xfloat real;		/* lex input */
    unsigned short type;	/* internal */
    struct _node_ *node;	/* internal */
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
	     formal_declaration_list varargs_formal_declaration
	     formal_declaration type_specifier data_dcltr function_dcltr dcltr
	     list_dcltr dcltr_or_stmt_list dcltr_or_stmt if_stmt stmt
	     compound_stmt opt_caught_stmt function_name primary_p1_exp
	     primary_p2_exp postfix_exp prefix_exp cast_exp mult_oper_exp
	     add_oper_exp shift_oper_exp rel_oper_exp equ_oper_exp
	     bitand_oper_exp bitxor_oper_exp bitor_oper_exp and_oper_exp
	     or_oper_exp cond_exp exp list_exp opt_list_exp f_list_exp
	     f_opt_list_exp arg_list opt_arg_list opt_arg_list_comma assoc_exp
	     assoc_arg_list opt_assoc_arg_list_comma ident

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
	: opt_private INHERIT opt_inherit_label composite_string ';'
		{
		  if (ndeclarations > 0) {
		      c_error("inherit must precede all declarations");
		  } else if (nerrors > 0 ||
			     !c_inherit($4->l.string->text, $3, $1 != 0)) {
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
		{ $$ = (node *) NULL; }
	| ident
	;

ident
	: IDENTIFIER
		{ $$ = node_str(str_new(yytext, (long) yyleng)); }
	;

composite_string
	: string
	| composite_string '+' string
		{ $$ = node_str(str_add($1->l.string, $3->l.string)); }
	| '(' composite_string ')'
		{ $$ = $2; }
	;

string
	: STRING_CONST
		{ $$ = node_str(str_new(yytext, (long) yyleng)); }
	;

data_declaration
	: class_specifier_list type_specifier list_dcltr ';'
		{ c_global($1, $2, $3); }
	;

function_declaration
	: class_specifier_list type_specifier function_dcltr
		{ 
		  typechecking = TRUE;
		  c_function($1, $2, $3);
		}
	  compound_stmt
		{
		  if (nerrors == 0) {
		      c_funcbody($5);
		  }
		}
	| class_specifier_list ident '(' formals_declaration ')'
		{
		  typechecking = c_typechecking();
		  c_function($1, node_type((typechecking) ? T_VOID : T_NIL,
					   (string *) NULL),
			     node_bin(N_FUNC, 0, $2, $4));
		}
	  compound_stmt
		{
		  if (nerrors == 0) {
		      c_funcbody($7);
		  }
		}
	;

local_data_declaration
	: class_specifier_list type_specifier list_dcltr ';'
		{ c_local($1, $2, $3); }
	;

formals_declaration
	: /* empty */
		{ $$ = (node *) NULL; }
	| VOID	{ $$ = (node *) NULL; }
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
		{ $$ = node_bin(N_PAIR, 0, $1, $3); }
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
		  $$->class = $1->class;
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
	: INT	{ $$ = node_type(T_INT, (string *) NULL); }
	| FLOAT	{ $$ = node_type(T_FLOAT, (string *) NULL); }
	| STRING
		{ $$ = node_type(T_STRING, (string *) NULL); }
	| OBJECT
		{ $$ = node_type(T_OBJECT, (string *) NULL); }
	| OBJECT composite_string
		{ $$ = node_type(T_CLASS, c_objecttype($2)); }
	| MAPPING
		{ $$ = node_type(T_MAPPING, (string *) NULL); }
	| MIXED	{ $$ = node_type(T_MIXED, (string *) NULL); }
	| VOID	{ $$ = node_type(T_VOID, (string *) NULL); }
	;

star_list
	: /* empty */
		{ $$ = 0; }
	| star_list '*'
		{
		  $$ = $1 + 1;
		  if ($$ == 1 << (8 - REFSHIFT)) {
		      c_error("too deep indirection");
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

function_dcltr
	: star_list ident '(' formals_declaration ')'
		{ $$ = node_bin(N_FUNC, ($1 << REFSHIFT) & T_REF, $2, $4); }
	;

dcltr
	: data_dcltr
	| function_dcltr
	;

list_dcltr
	: dcltr
	| list_dcltr ',' dcltr
		{ $$ = node_bin(N_PAIR, 0, $1, $3); }
	;

dcltr_or_stmt_list
	: /* empty */
		{ $$ = (node *) NULL; }
	| dcltr_or_stmt_list dcltr_or_stmt
		{ $$ = c_concat($1, $2); }
	;

dcltr_or_stmt
	: local_data_declaration
		{
		  if (nstatements > 0) {
		      c_error("declaration after statement");
		  }
		  $$ = (node *) NULL;
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
		  $$ = (node *) NULL;
		}
	;

if_stmt
	: IF '(' f_list_exp ')'
		{ c_startcond(); }
	  stmt	{ $$ = c_if($3, $6); }
	;

stmt
	: list_exp ';'
		{ $$ = c_exp_stmt($1); }
	| compound_stmt
	| if_stmt
		{
		  c_endcond();
		  $$ = c_endif($1, (node *) NULL);
		}
/* will cause shift/reduce conflict */
	| if_stmt ELSE
		{ c_startcond2(); }
	  stmt
		{
		  c_matchcond();
		  $$ = c_endif($1, $4);
		}
	| DO	{ c_loop(); }
	  stmt WHILE '(' f_list_exp ')' ';'
		{ $$ = c_do($6, $3); }
	| WHILE '(' f_list_exp ')'
		{
		  c_loop();
		  c_startcond();
		}
	  stmt	{
		  c_endcond();
		  $$ = c_while($3, $6);
		}
	| FOR '(' opt_list_exp ';' f_opt_list_exp ';' opt_list_exp ')'
		{
		  c_loop();
		  c_startcond();
		}
	  stmt	{
		  c_endcond();
		  $$ = c_for(c_exp_stmt($3), $5, c_exp_stmt($7), $10);
		}
	| RLIMITS '(' f_list_exp ';' f_list_exp ')'
		{
		  if (typechecking) {
		      char tnbuf[17];

		      if ($3->mod != T_INT && $3->mod != T_MIXED) {
			  c_error("bad type for stack rlimit (%s)",
				  i_typename(tnbuf, $3->mod));
		      }
		      if ($5->mod != T_INT && $5->mod != T_MIXED) {
			  c_error("bad type for ticks rlimit (%s)",
				  i_typename(tnbuf, $5->mod));
		      }
		  }
		  c_startrlimits();
		}
	  compound_stmt
		{ $$ = c_endrlimits($3, $5, $8); }
	| CATCH	{
		  c_startcatch();
		  c_startcond();
		}
	  compound_stmt
		{
		  c_endcond();
		  c_endcatch();
		  c_startcond();
		}
	  opt_caught_stmt
		{
		  c_endcond();
		  $$ = c_donecatch($3, $5);
		}
	| SWITCH '(' f_list_exp ')'
		{
		  c_startswitch($3, typechecking);
		  c_startcond();
		}
	  compound_stmt
		{
		  c_endcond();
		  $$ = c_endswitch($3, $6);
		}
	| CASE exp ':'
		{ $2 = c_case($2, (node *) NULL); }
	  stmt	{
		  $$ = $2;
		  if ($$ != (node *) NULL) {
		      $$->l.left = $5;
		  } else {
		      $$ = $5;
		  }
		}
	| CASE exp DOT_DOT exp ':'
		{ $2 = c_case($2, $4); }
	  stmt	{
		  $$ = $2;
		  if ($$ != (node *) NULL) {
		      $$->l.left = $7;
		  } else {
		      $$ = $7;
		  }
		}
	| DEFAULT ':'
		{ $<node>2 = c_default(); }
	  stmt	{
		  $$ = $<node>2;
		  if ($$ != (node *) NULL) {
		      $$->l.left = $4;
		  } else {
		      $$ = $4;
		  }
		}
	| BREAK ';'
		{
		  $$ = c_break();
		}
	| CONTINUE ';'
		{
		  $$ = c_continue();
		}
	| RETURN f_opt_list_exp ';'
		{ $$ = c_return($2, typechecking); }
	| ';'	{ $$ = (node *) NULL; }
	;

compound_stmt
	: '{'	{
		  nstatements = 0;
		  c_startcompound();
		}
	  dcltr_or_stmt_list '}'
		{
		  nstatements++;
		  $$ = c_endcompound($3);
		}
	;

opt_caught_stmt
	: /* empty */
		{ $$ = (node *) NULL; }
	| ':' stmt
		{ $$ = $2; }
	;

function_name
	: ident	{ $$ = c_flookup($1, typechecking); }
	| COLON_COLON ident
		{ $$ = c_iflookup($2, (node *) NULL); }
	| ident COLON_COLON ident
		{ $$ = c_iflookup($3, $1); }
	;

primary_p1_exp
	: INT_CONST
		{ $$ = node_int($1); }
	| FLOAT_CONST
		{ $$ = node_float(&$1); }
	| NIL	{ $$ = node_nil(); }
	| string
	| '(' '{' opt_arg_list_comma '}' ')'
		{ $$ = c_aggregate($3, T_MIXED | (1 << REFSHIFT)); }
	| '(' '[' opt_assoc_arg_list_comma ']' ')'
		{ $$ = c_aggregate($3, T_MAPPING); }
	| ident	{
		  $$ = c_variable($1);
		  if (typechecking) {
		      if ($$->type == N_GLOBAL && $$->mod != T_MIXED &&
			  !conf_typechecking()) {
			  /*
			   * global vars might be modified by untypechecked
			   * functions...
			   */
			  $$ = node_mon(N_CAST, $$->mod, $$);
			  $$->class = $$->l.left->class;
		      }
		  } else {
		      /* the variable could be anything */
		      $$->mod = T_MIXED;
		  }
		}
	| '(' list_exp ')'
		{ $$ = $2; }
	| function_name '(' opt_arg_list ')'
		{ $$ = c_checkcall(c_funcall($1, $3), typechecking); }
	| CATCH '('
		{ c_startcond(); }
	  list_exp ')'
		{
		  c_endcond();
		  $$ = node_mon(N_CATCH, T_STRING, $4);
		}
	| primary_p2_exp RARROW ident '(' opt_arg_list ')'
		{
		  t_void($1);
		  $$ = c_checkcall(c_arrow($1, $3, $5), typechecking);
		}
	| primary_p2_exp LARROW string
		{ $$ = c_instanceof($1, $3); }
	| primary_p2_exp LARROW '(' composite_string ')'
		{ $$ = c_instanceof($1, $4); }
	;

primary_p2_exp
	: primary_p1_exp
	| primary_p2_exp '[' f_list_exp ']'
		{ $$ = idx($1, $3); }
	| primary_p2_exp '[' f_opt_list_exp DOT_DOT f_opt_list_exp ']'
		{ $$ = range($1, $3, $5); }
	;

postfix_exp
	: primary_p2_exp
	| postfix_exp PLUS_PLUS
		{ $$ = uassign(N_PLUS_PLUS, $1, "++"); }
	| postfix_exp MIN_MIN
		{ $$ = uassign(N_MIN_MIN, $1, "--"); }
	;

prefix_exp
	: postfix_exp
	| PLUS_PLUS cast_exp
		{ $$ = uassign(N_ADD_EQ_1, $2, "++"); }
	| MIN_MIN cast_exp
		{ $$ = uassign(N_SUB_EQ_1, $2, "--"); }
	| '-' cast_exp
		{ $$ = umin($2); }
	| '+' cast_exp
		{ $$ = $2; }
	| '!' cast_exp
		{
		  t_void($2);
		  $$ = c_not($2);
		}
	| '~' cast_exp
		{
		  $$ = $2;
		  t_void($$);
		  if (typechecking && $$->mod != T_INT && $$->mod != T_MIXED) {
		      char tnbuf[17];

		      c_error("bad argument type for ~ (%s)",
			      i_typename(tnbuf, $$->mod));
		      $$->mod = T_MIXED;
		  } else {
		      $$ = xor(N_XOR, $$, node_int((Int) -1), "^");
		  }
		}
	;

cast_exp
	: prefix_exp
	| '(' type_specifier star_list ')' cast_exp
		{
		  $2->mod |= ($3 << REFSHIFT) & T_REF;
		  $$ = cast($5, $2);
		}
	;

mult_oper_exp
	: cast_exp
	| mult_oper_exp '*' cast_exp
		{ $$ = mult(N_MULT, $1, $3, "*"); }
	| mult_oper_exp '/' cast_exp
		{ $$ = mdiv(N_DIV, $1, $3, "/"); }
	| mult_oper_exp '%' cast_exp
		{ $$ = mod(N_MOD, $1, $3, "%"); }
	;

add_oper_exp
	: mult_oper_exp
	| add_oper_exp '+' mult_oper_exp
		{ $$ = add(N_ADD, $1, $3, "+"); }
	| add_oper_exp '-' mult_oper_exp
		{ $$ = sub(N_SUB, $1, $3, "-"); }
	;

shift_oper_exp
	: add_oper_exp
	| shift_oper_exp LSHIFT add_oper_exp
		{ $$ = lshift(N_LSHIFT, $1, $3, "<<"); }
	| shift_oper_exp RSHIFT add_oper_exp
		{ $$ = rshift(N_RSHIFT, $1, $3, ">>"); }
	;

rel_oper_exp
	: shift_oper_exp
	| rel_oper_exp '<' shift_oper_exp
		{ $$ = rel(N_LT, $$, $3, "<"); }
	| rel_oper_exp '>' shift_oper_exp
		{ $$ = rel(N_GT, $$, $3, ">"); }
	| rel_oper_exp LE shift_oper_exp
		{ $$ = rel(N_LE, $$, $3, "<="); }
	| rel_oper_exp GE shift_oper_exp
		{ $$ = rel(N_GE, $$, $3, ">="); }
	;

equ_oper_exp
	: rel_oper_exp
	| equ_oper_exp EQ rel_oper_exp
		{ $$ = eq($1, $3); }
	| equ_oper_exp NE rel_oper_exp
		{ $$ = c_not(eq($1, $3)); }
	;

bitand_oper_exp
	: equ_oper_exp
	| bitand_oper_exp '&' equ_oper_exp
		{ $$ = and(N_AND, $1, $3, "&"); }
	;

bitxor_oper_exp
	: bitand_oper_exp
	| bitxor_oper_exp '^' bitand_oper_exp
		{ $$ = xor(N_XOR, $1, $3, "^"); }
	;

bitor_oper_exp
	: bitxor_oper_exp
	| bitor_oper_exp '|' bitxor_oper_exp
		{ $$ = or(N_OR, $1, $3, "|"); }
	;

and_oper_exp
	: bitor_oper_exp
	| and_oper_exp LAND bitor_oper_exp
		{ $$ = land($1, $3); }
	;

or_oper_exp
	: and_oper_exp
	| or_oper_exp LOR and_oper_exp
		{ $$ = lor($1, $3); }
	;

cond_exp
	: or_oper_exp
	| or_oper_exp '?'
		{ c_startcond(); }
	  list_exp ':'
		{ c_startcond2(); }
	  cond_exp
		{
		  c_matchcond();
		  $$ = quest($1, $4, $7);
		}
	;

exp
	: cond_exp
	| cond_exp '=' exp
		{ $$ = assign(c_assign($1), $3); }
	| cond_exp PLUS_EQ exp
		{ $$ = add(N_ADD_EQ, c_lvalue($1, "+="), $3, "+="); }
	| cond_exp MIN_EQ exp
		{ $$ = sub(N_SUB_EQ, c_lvalue($1, "-="), $3, "-="); }
	| cond_exp MULT_EQ exp
		{ $$ = mult(N_MULT_EQ, c_lvalue($1, "*="), $3, "*="); }
	| cond_exp DIV_EQ exp
		{ $$ = mdiv(N_DIV_EQ, c_lvalue($1, "/="), $3, "/="); }
	| cond_exp MOD_EQ exp
		{ $$ = mod(N_MOD_EQ, c_lvalue($1, "%="), $3, "%="); }
	| cond_exp LSHIFT_EQ exp
		{ $$ = lshift(N_LSHIFT_EQ, c_lvalue($1, "<<="), $3, "<<="); }
	| cond_exp RSHIFT_EQ exp
		{ $$ = rshift(N_RSHIFT_EQ, c_lvalue($1, ">>="), $3, ">>="); }
	| cond_exp AND_EQ exp
		{ $$ = and(N_AND_EQ, c_lvalue($1, "&="), $3, "&="); }
	| cond_exp XOR_EQ exp
		{ $$ = xor(N_XOR_EQ, c_lvalue($1, "^="), $3, "^="); }
	| cond_exp OR_EQ exp
		{ $$ = or(N_OR_EQ, c_lvalue($1, "|="), $3, "|="); }
	;

list_exp
	: exp
	| list_exp ',' exp
		{ $$ = comma($1, $3); }
	;

opt_list_exp
	: /* empty */
		{ $$ = (node *) NULL; }
	| list_exp
	;

f_list_exp
	: list_exp
		{ t_void($$ = $1); }
	;

f_opt_list_exp
	: opt_list_exp
		{ t_void($$ = $1); }
	;

arg_list
	: exp	{ t_void($$ = $1); }
	| arg_list ',' exp
		{
		  t_void($3);
		  $$ = node_bin(N_PAIR, 0, $1, $3);
		}
	;

opt_arg_list
	: /* empty */
		{ $$ = (node *) NULL; }
	| arg_list
	| arg_list ELLIPSIS
		{
		  $$ = $1;
		  if ($$->type == N_PAIR) {
		      $$->r.right = node_mon(N_SPREAD, -1, $$->r.right);
		  } else {
		      $$ = node_mon(N_SPREAD, -1, $$);
		  }
		}
	;

opt_arg_list_comma
	: /* empty */
		{ $$ = (node *) NULL; }
	| arg_list
	| arg_list ','
		{ $$ = $1; }
	;

assoc_exp
	: exp ':' exp
		{
		  t_void($1);
		  t_void($3);
		  $$ = node_bin(N_COMMA, 0, $1, $3);
		}
	;

assoc_arg_list
	: assoc_exp
	| assoc_arg_list ',' assoc_exp
		{ $$ = node_bin(N_PAIR, 0, $1, $3); }
	;

opt_assoc_arg_list_comma
	: /* empty */
		{ $$ = (node *) NULL; }
	| assoc_arg_list
	| assoc_arg_list ','
		{ $$ = $1; }
	;

%%

/*
 * NAME:	t_void()
 * DESCRIPTION:	if the argument is of type void, an error will result
 */
static void t_void(n)
register node *n;
{
    if (n != (node *) NULL && n->mod == T_VOID) {
	c_error("void value not ignored");
	n->mod = T_MIXED;
    }
}

/*
 * NAME:	t_unary()
 * DESCRIPTION:	typecheck the argument of a unary operator
 */
static bool t_unary(n, name)
register node *n;
char *name;
{
    char tnbuf[17];

    t_void(n);
    if (typechecking && !T_ARITHMETIC(n->mod) && n->mod != T_MIXED) {
	c_error("bad argument type for %s (%s)", name,
		i_typename(tnbuf, n->mod));
	n->mod = T_MIXED;
	return FALSE;
    }
    return TRUE;
}

/*
 * NAME:	uassign()
 * DESCRIPTION:	handle a unary assignment operator
 */
static node *uassign(op, n, name)
int op;
register node *n;
char *name;
{
    t_unary(n, name);
    return node_mon((n->mod == T_INT) ? op + 1 : op, n->mod, c_lvalue(n, name));
}

/*
 * NAME:	cast()
 * DESCRIPTION:	cast an expression to a type
 */
static node *cast(n, type)
register node *n, *type;
{
    xfloat flt;
    Int i;
    char *p, buffer[18];

    if (type->mod != n->mod) {
	switch (type->mod) {
	case T_INT:
	    switch (n->type) {
	    case N_FLOAT:
		/* cast float constant to int */
		NFLT_GET(n, flt);
		return node_int(flt_ftoi(&flt));

	    case N_STR:
		/* cast string to int */
		p = n->l.string->text;
		i = strtoint(&p);
		if (p == n->l.string->text + n->l.string->len) {
		    return node_int(i);
		} else {
		    c_error("cast of invalid string constant");
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
		    return node_mon(N_TOINT, T_INT, n);
		}
		break;
	    }
	    break;

	case T_FLOAT:
	    switch (n->type) {
	    case N_INT:
		/* cast int constant to float */
		flt_itof(n->l.number, &flt);
		return node_float(&flt);

	    case N_STR:
		/* cast string to float */
		p = n->l.string->text;
		if (flt_atof(&p, &flt) &&
		    p == n->l.string->text + n->l.string->len) {
		    return node_float(&flt);
		} else {
		    yyerror("cast of invalid string constant");
		    n->mod = T_MIXED;
		}
		break;

	    case N_TOSTRING:
		if (n->l.left->mod == T_INT) {
		    return node_mon(N_TOFLOAT, T_FLOAT, n->l.left);
		}
		/* fall through */
	    default:
		if (n->mod == T_INT || n->mod == T_STRING || n->mod == T_MIXED)
		{
		    return node_mon(N_TOFLOAT, T_FLOAT, n);
		}
		break;
	    }
	    break;

	case T_STRING:
	    switch (n->type) {
	    case N_INT:
		/* cast int constant to string */
		sprintf(buffer, "%ld", (long) n->l.number);
		return node_str(str_new(buffer, (long) strlen(buffer)));

	    case N_FLOAT:
		/* cast float constant to string */
		NFLT_GET(n, flt);
		flt_ftoa(&flt, buffer);
		return node_str(str_new(buffer, (long) strlen(buffer)));

	    default:
		if (n->mod == T_INT || n->mod == T_FLOAT || n->mod == T_MIXED) {
		    return node_mon(N_TOSTRING, T_STRING, n);
		}
		break;
	    }
	    break;
	}

	if (type->mod == T_MIXED || (type->mod & T_TYPE) == T_VOID) {
	    /* (mixed), (void), (void *) */
	    c_error("cannot cast to %s", i_typename(buffer, type->mod));
	    n->mod = T_MIXED;
	} else if ((type->mod & T_REF) < (n->mod & T_REF)) {
	    /* (mixed *) of (mixed **) */
	    c_error("illegal cast of array type (%s)",
		    i_typename(buffer, n->mod));
	} else if ((n->mod & T_TYPE) != T_MIXED &&
		   ((type->mod & T_TYPE) != T_CLASS ||
		    ((n->mod & T_TYPE) != T_OBJECT &&
		     (n->mod & T_TYPE) != T_CLASS) ||
		    (type->mod & T_REF) != (n->mod & T_REF))) {
	    /* can only cast from mixed, or object/class to class */
	    c_error("cast of invalid type (%s)", i_typename(buffer, n->mod));
	} else {
	    if ((type->mod & T_REF) == 0 || (n->mod & T_REF) == 0) {
		/* runtime cast */
		n = node_mon(N_CAST, type->mod, n);
	    } else {
		n->mod = type->mod;
	    }
	    n->class = type->class;
	}
    } else if (type->mod == T_CLASS && str_cmp(type->class, n->class) != 0) {
	/*
	 * cast to different object class
	 */
	n = node_mon(N_CAST, type->mod, n);
	n->class = type->class;
    }
    return n;
}

/*
 * NAME:	idx()
 * DESCRIPTION:	handle the [ ] operator
 */
static node *idx(n1, n2)
register node *n1, *n2;
{
    char tnbuf[17];
    register unsigned short type;

    if (n1->type == N_STR && n2->type == N_INT) {
	/* str [ int ] */
	if (n2->l.number < 0 || n2->l.number >= (Int) n1->l.string->len) {
	    c_error("string index out of range");
	} else {
	    n2->l.number =
		    UCHAR(n1->l.string->text[str_index(n1->l.string,
						       (long) n2->l.number)]);
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
		c_error("bad index type (%s)", i_typename(tnbuf, n2->mod));
	    }
	    if (type != T_MIXED) {
		/* you can't trust these arrays */
		n2 = node_mon(N_CAST, type, node_bin(N_INDEX, type, n1, n2));
		n2->class = n1->class;
		return n2;
	    }
	}
	type = T_MIXED;
    } else if (n1->mod == T_STRING) {
	/*
	 * string
	 */
	if (typechecking && n2->mod != T_INT && n2->mod != T_MIXED) {
	    c_error("bad index type (%s)", i_typename(tnbuf, n2->mod));
	}
	type = T_INT;
    } else {
	if (typechecking && n1->mod != T_MAPPING && n1->mod != T_MIXED) {
	    c_error("bad indexed type (%s)", i_typename(tnbuf, n1->mod));
	}
	type = T_MIXED;
    }
    return node_bin(N_INDEX, type, n1, n2);
}

/*
 * NAME:	range()
 * DESCRIPTION:	handle the [ .. ] operator
 */
static node *range(n1, n2, n3)
register node *n1, *n2, *n3;
{
    if (n1->type == N_STR && (n2 == (node *) NULL || n2->type == N_INT) &&
	(n3 == (node *) NULL || n3->type == N_INT)) {
	Int from, to;

	/* str [ int .. int ] */
	from = (n2 == (node *) NULL) ? 0 : n2->l.number;
	to = (n3 == (node *) NULL) ? n1->l.string->len - 1 : n3->l.number;
	if (from < 0 || from > to + 1 || to >= n1->l.string->len) {
	    c_error("invalid string range");
	} else {
	    return node_str(str_range(n1->l.string, (long) from, (long) to));
	}
    }

    if (typechecking && n1->mod != T_MAPPING && n1->mod != T_MIXED) {
	char tnbuf[17];

	/* indices */
	if (n2 != (node *) NULL && n2->mod != T_INT && n2->mod != T_MIXED) {
	    c_error("bad index type (%s)", i_typename(tnbuf, n2->mod));
	}
	if (n3 != (node *) NULL && n3->mod != T_INT && n3->mod != T_MIXED) {
	    c_error("bad index type (%s)", i_typename(tnbuf, n3->mod));
	}
	/* range */
	if ((n1->mod & T_REF) == 0 && n1->mod != T_STRING) {
	    c_error("bad indexed type (%s)", i_typename(tnbuf, n1->mod));
	}
    }

    return node_bin(N_RANGE, n1->mod, n1, node_bin(N_PAIR, 0, n2, n3));
}

/*
 * NAME:	bini()
 * DESCRIPTION:	handle a binary int operator
 */
static node *bini(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char tnbuf1[17], tnbuf2[17];

    t_void(n1);
    t_void(n2);

    if (typechecking &&
	((n1->mod != T_INT && n1->mod != T_MIXED) ||
	 (n2->mod != T_INT && n2->mod != T_MIXED))) {
	c_error("bad argument types for %s (%s, %s)", name,
		i_typename(tnbuf1, n1->mod), i_typename(tnbuf2, n2->mod));
    }
    if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return node_bin(op, T_INT, n1, n2);
}

/*
 * NAME:	bina()
 * DESCRIPTION:	handle a binary arithmetic operator
 */
static node *bina(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char tnbuf1[17], tnbuf2[17];
    register unsigned short type;

    t_void(n1);
    t_void(n2);

    type = T_MIXED;
    if (typechecking &&
	((n1->mod != n2->mod && n1->mod != T_MIXED && n2->mod != T_MIXED) ||
	 (!T_ARITHMETIC(n1->mod) && n1->mod != T_MIXED) ||
	 (!T_ARITHMETIC(n2->mod) && n2->mod != T_MIXED))) {
	c_error("bad argument types for %s (%s, %s)", name,
		i_typename(tnbuf1, n1->mod), i_typename(tnbuf2, n2->mod));
    } else if (n1->mod == T_INT || n2->mod == T_INT) {
	if (n1->mod == T_INT && n2->mod == T_INT) {
	    op++;
	}
	type = T_INT;
    } else if (n1->mod == T_FLOAT || n2->mod == T_FLOAT) {
	type = T_FLOAT;
    }

    return node_bin(op, type, n1, n2);
}

/*
 * NAME:	mult()
 * DESCRIPTION:	handle the * *= operators
 */
static node *mult(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    xfloat f1, f2;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i * i */
	n1->l.number *= n2->l.number;
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	flt_mult(&f1, &f2);
	NFLT_PUT(n1, f1);
	return n1;
    }
    return bina(op, n1, n2, name);
}

/*
 * NAME:	mdiv()
 * DESCRIPTION:	handle the / /= operators
 */
static node *mdiv(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    xfloat f1, f2;

    if (n1->type == N_INT && n2->type == N_INT) {
	register Int i, d;

	/* i / i */
	i = n1->l.number;
	d = n2->l.number;
	if (d == 0) {
	    /* i / 0 */
	    c_error("division by zero");
	    return n1;
	}
	if ((d | i) < 0) {
	    Int r;

            r = ((Uint) ((i < 0) ? -i : i)) / ((Uint) ((d < 0) ? -d : d));
            n1->l.number = ((i ^ d) < 0) ? -r : r;
	} else {
	    n1->l.number = ((Uint) i) / ((Uint) d);
	}
	return n1;
    } else if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	/* f / f */
	if (NFLT_ISZERO(n2)) {
	    /* f / 0.0 */
	    c_error("division by zero");
	    return n1;
	}
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	flt_div(&f1, &f2);
	NFLT_PUT(n1, f1);
	return n1;
    }

    return bina(op, n1, n2, name);
}

/*
 * NAME:	mod()
 * DESCRIPTION:	handle the % %= operators
 */
static node *mod(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n1->type == N_INT && n2->type == N_INT) {
	register Int i, d;

	/* i % i */
	i = n1->l.number;
	d = n2->l.number;
	if (d == 0) {
	    /* i % 0 */
	    c_error("modulus by zero");
	    return n1;
	}
	if (d < 0) {
	    d = -d;
	}
	if (i < 0) {
            n1->l.number = - (Int) (((Uint) -i) % ((Uint) d));
	} else {
	    n1->l.number = ((Uint) i) % ((Uint) d);
	}
	return n1;
    }

    return bini(op, n1, n2, name);
}

/*
 * NAME:	add()
 * DESCRIPTION:	handle the + += operators, possibly rearranging the order
 *		of the expression
 */
static node *add(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char tnbuf1[17], tnbuf2[17];
    xfloat f1, f2;
    register unsigned short type;

    t_void(n1);
    t_void(n2);

    if (n1->mod == T_STRING) {
	if (n2->mod == T_INT || n2->mod == T_FLOAT ||
	    (n2->mod == T_MIXED && typechecking)) {
	    n2 = cast(n2, node_type(T_STRING, (string *) NULL));
	}
    } else if (n2->mod == T_STRING && op == N_ADD) {
	if (n1->mod == T_INT || n1->mod == T_FLOAT ||
	    (n1->mod == T_MIXED && typechecking)) {
	    n1 = cast(n1, node_type(T_STRING, (string *) NULL));
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
	flt_add(&f1, &f2);
	NFLT_PUT(n1, f1);
	return n1;
    }
    if (n1->type == N_STR && n2->type == N_STR) {
	/* s + s */
	return node_str(str_add(n1->l.string, n2->l.string));
    }

    type = c_tmatch(n1->mod, n2->mod);
    if (type == T_NIL || type == T_OBJECT || type == T_CLASS) {
	type = T_MIXED;
	if (typechecking) {
	    c_error("bad argument types for %s (%s, %s)", name,
		    i_typename(tnbuf1, n1->mod), i_typename(tnbuf2, n2->mod));
	}
    } else if (type == T_INT) {
	op++;
    } else if (op == N_ADD_EQ) {
	if (n1->mod == T_INT) {
	    n2 = node_mon(N_CAST, T_INT, n2);
	    type = T_INT;
	    op++;
	} else if (n1->mod == T_FLOAT && n2->mod != T_FLOAT) {
	    n2 = node_mon(N_CAST, T_FLOAT, n2);
	    type = T_FLOAT;
	}
    }
    return node_bin(op, type, n1, n2);
}

/*
 * NAME:	sub()
 * DESCRIPTION:	handle the - -= operators
 */
static node *sub(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char tnbuf1[17], tnbuf2[17];
    xfloat f1, f2;
    register unsigned short type;

    t_void(n1);
    t_void(n2);

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i - i */
	n1->l.number -= n2->l.number;
	return n1;
    }
    if (n1->type == N_FLOAT && n2->type == N_FLOAT) {
	/* f - f */
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);
	flt_sub(&f1, &f2);
	NFLT_PUT(n1, f1);
	return n1;
    }

    type = c_tmatch(n1->mod, n2->mod);
    if (type == T_NIL || type == T_STRING || type == T_OBJECT ||
	type == T_CLASS || type == T_MAPPING) {
	if ((type=n1->mod) != T_MAPPING ||
	    (n2->mod != T_MIXED && (n2->mod & T_REF) == 0)) {
	    type = T_MIXED;
	    if (typechecking) {
		c_error("bad argument types for %s (%s, %s)", name,
			i_typename(tnbuf1, n1->mod),
			i_typename(tnbuf2, n2->mod));
	    }
	}
    } else if (type == T_INT) {
	op++;
    } else if (type == T_MIXED) {
	type = (n1->mod == T_MIXED) ? n2->mod : n1->mod;
    } else if (n1->mod == T_MIXED && (n2->mod & T_REF)) {
	type = T_MIXED;
    }
    return node_bin(op, type, n1, n2);
}

/*
 * NAME:	umin()
 * DESCRIPTION:	handle unary minus
 */
static node *umin(n)
register node *n;
{
    xfloat flt;

    if (t_unary(n, "unary -")) {
	if (n->mod == T_FLOAT) {
	    FLT_ZERO(flt.high, flt.low);
	    n = sub(N_SUB, node_float(&flt), n, "-");
	} else {
	    n = sub(N_SUB, node_int((Int) 0), n, "-");
	}
    }
    return n;
}

/*
 * NAME:	lshift()
 * DESCRIPTION:	handle the << <<= operators
 */
static node *lshift(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n2->type == N_INT) {
	if (n2->l.number < 0) {
	    c_error("negative left shift");
	    n2->l.number = 0;
	}
	if (n1->type == N_INT) {
	    /* i << i */
	    n1->l.number = (n2->l.number < 32) ?
			    (Uint) n1->l.number << n2->l.number : 0;
	    return n1;
	}
    }

    return bini(op, n1, n2, name);
}

/*
 * NAME:	rshift()
 * DESCRIPTION:	handle the >> >>= operators
 */
static node *rshift(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n2->type == N_INT) {
	if (n2->l.number < 0) {
	    c_error("negative right shift");
	    n2->l.number = 0;
	}
	if (n1->type == N_INT) {
	    /* i >> i */
	    n1->l.number = (n2->l.number < 32) ?
			    (Uint) n1->l.number >> n2->l.number : 0;
	    return n1;
	}
    }

    return bini(op, n1, n2, name);
}

/*
 * NAME:	rel()
 * DESCRIPTION:	handle the < > <= >= operators
 */
static node *rel(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char tnbuf1[17], tnbuf2[17];

    t_void(n1);
    t_void(n2);

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
	xfloat f1, f2;

	/* f . f */
	NFLT_GET(n1, f1);
	NFLT_GET(n2, f2);

	switch (op) {
	case N_GE:
	    return node_int((Int) (flt_cmp(&f1, &f2) >= 0));

	case N_GT:
	    return node_int((Int) (flt_cmp(&f1, &f2) > 0));

	case N_LE:
	    return node_int((Int) (flt_cmp(&f1, &f2) <= 0));

	case N_LT:
	    return node_int((Int) (flt_cmp(&f1, &f2) < 0));
	}
	return n1;
    }
    if (n1->type == N_STR && n2->type == N_STR) {
	/* s . s */
	switch (op) {
	case N_GE:
	    return node_int((Int) (str_cmp(n1->l.string, n2->l.string) >= 0));

	case N_GT:
	    return node_int((Int) (str_cmp(n1->l.string, n2->l.string) > 0));

	case N_LE:
	    return node_int((Int) (str_cmp(n1->l.string, n2->l.string) <= 0));

	case N_LT:
	    return node_int((Int) (str_cmp(n1->l.string, n2->l.string) < 0));
	}
    }

    if (typechecking &&
	((n1->mod != n2->mod && n1->mod != T_MIXED && n2->mod != T_MIXED) ||
	 (!T_ARITHSTR(n1->mod) && n1->mod != T_MIXED) ||
	 (!T_ARITHSTR(n2->mod) && n2->mod != T_MIXED))) {
	c_error("bad argument types for %s (%s, %s)", name,
		i_typename(tnbuf1, n1->mod), i_typename(tnbuf2, n2->mod));
    } else if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return node_bin(op, T_INT, n1, n2);
}

/*
 * NAME:	eq()
 * DESCRIPTION:	handle the == operator
 */
static node *eq(n1, n2)
register node *n1, *n2;
{
    char tnbuf1[17], tnbuf2[17];
    xfloat f1, f2;
    int op;

    t_void(n1);
    t_void(n2);

    switch (n1->type) {
    case N_INT:
	if (n2->type == N_INT) {
	    /* i == i */
	    n1->l.number = (n1->l.number == n2->l.number);
	    return n1;
	}
	if (nil_node == N_INT && n1->l.number == 0 && n2->type == N_STR) {
	    /* nil == str */
	    return node_int((Int) FALSE);
	}
	break;

    case N_FLOAT:
	if (n2->type == N_FLOAT) {
	    /* f == f */
	    NFLT_GET(n1, f1);
	    NFLT_GET(n2, f2);
	    return node_int((Int) (flt_cmp(&f1, &f2) == 0));
	}
	break;

    case N_STR:
	if (n2->type == N_STR) {
	    /* s == s */
	    return node_int((Int) (str_cmp(n1->l.string, n2->l.string) == 0));
	}
	if (n2->type == nil_node && n2->l.number == 0) {
	    /* s == nil */
	    return node_int((Int) FALSE);
	}
	break;

    case N_NIL:
	if (n2->type == N_NIL) {
	    /* nil == nil */
	    return node_int((Int) TRUE);
	}
	if (n2->type == N_STR) {
	    /* nil == str */
	    return node_int((Int) FALSE);
	}
	break;
    }

    op = N_EQ;
    if (n1->mod != n2->mod && n1->mod != T_MIXED && n2->mod != T_MIXED &&
	(!c_nil(n1) || !T_POINTER(n2->mod)) &&
	(!c_nil(n2) || !T_POINTER(n1->mod))) {
	if (typechecking) {
	    c_error("incompatible types for equality (%s, %s)",
		    i_typename(tnbuf1, n1->mod), i_typename(tnbuf2, n2->mod));
	}
    } else if (n1->mod == T_INT && n2->mod == T_INT) {
	op++;
    }
    return node_bin(op, T_INT, n1, n2);
}

/*
 * NAME:	and()
 * DESCRIPTION:	handle the & &= operators
 */
static node *and(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    register unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i & i */
	n1->l.number &= n2->l.number;
	return n1;
    }
    if ((((type=n1->mod) == T_MIXED || type == T_MAPPING) &&
	 ((n2->mod & T_REF) != 0 || n2->mod == T_MIXED)) ||
	((type=c_tmatch(n1->mod, n2->mod)) & T_REF) != T_NIL) {
	/*
	 * possibly array & array or mapping & array
	 */
	return node_bin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * NAME:	xor()
 * DESCRIPTION:	handle the ^ ^= operators
 */
static node *xor(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    register unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i ^ i */
	n1->l.number ^= n2->l.number;
	return n1;
    }
    if (((type=n1->mod) == T_MIXED && n2->mod == T_MIXED) ||
	((type=c_tmatch(n1->mod, n2->mod)) & T_REF) != T_NIL) {
	/*
	 * possibly array ^ array
	 */
	return node_bin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * NAME:	or()
 * DESCRIPTION:	handle the | |= operators
 */
static node *or(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    register unsigned short type;

    if (n1->type == N_INT && n2->type == N_INT) {
	/* i | i */
	n1->l.number |= n2->l.number;
	return n1;
    }
    if (((type=n1->mod) == T_MIXED && n2->mod == T_MIXED) ||
	((type=c_tmatch(n1->mod, n2->mod)) & T_REF) != T_NIL) {
	/*
	 * possibly array | array
	 */
	return node_bin(op, type, n1, n2);
    }
    return bini(op, n1, n2, name);
}

/*
 * NAME:	land()
 * DESCRIPTION:	handle the && operator
 */
static node *land(n1, n2)
register node *n1, *n2;
{
    t_void(n1);
    t_void(n2);

    if ((n1->flags & F_CONST) && (n2->flags & F_CONST)) {
	n1 = c_tst(n1);
	n2 = c_tst(n2);
	n1->l.number &= n2->l.number;
	return n1;
    }

    return node_bin(N_LAND, T_INT, n1, n2);
}

/*
 * NAME:	lor()
 * DESCRIPTION:	handle the || operator
 */
static node *lor(n1, n2)
register node *n1, *n2;
{
    t_void(n1);
    t_void(n2);

    if ((n1->flags & F_CONST) && (n2->flags & F_CONST)) {
	n1 = c_tst(n1);
	n2 = c_tst(n2);
	n1->l.number |= n2->l.number;
	return n1;
    }

    return node_bin(N_LOR, T_INT, n1, n2);
}

/*
 * NAME:	quest()
 * DESCRIPTION:	handle the ? : operator
 */
static node *quest(n1, n2, n3)
register node *n1, *n2, *n3;
{
    register unsigned short type;

    t_void(n1);

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
    if (c_nil(n2) && T_POINTER(n3->mod)) {
	/*
	 * expr ? nil : expr
	 */
	type = n3->mod;
    } else if (c_nil(n3) && T_POINTER(n2->mod)) {
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
	    type = c_tmatch(n2->mod, n3->mod);
	    if (type == T_NIL) {
		/* no typechecking here, just let the result be mixed */
		type = T_MIXED;
	    }
	}
    }

    n1 = node_bin(N_QUEST, type, n1, node_bin(N_PAIR, 0, n2, n3));
    if ((type & T_TYPE) == T_CLASS) {
	if (n2->class == (string *) NULL) {
	    n1->class = n3->class;
	} else if (n3->class == (string *) NULL ||
		   str_cmp(n2->class, n3->class) == 0) {
	    n1->class = n2->class;
	} else {
	    /* downgrade to object */
	    n1->type = (n1->type & T_REF) | T_OBJECT;
	}
    }
    return n1;
}

/*
 * NAME:	assign()
 * DESCRIPTION:	handle the assignment operator
 */
static node *assign(n1, n2)
register node *n1, *n2;
{
    char tnbuf1[17], tnbuf2[17];
    register node *n, *m;
    register unsigned short type;

    if (n1->type == N_AGGR) {
	/*
	 * ({ a, b }) = array;
	 */
	if (typechecking) {
	    type = n2->mod;
	    if ((n2->mod & T_REF) != 0) {
		type -= 1 << REFSHIFT;
		if (type != T_MIXED) {
		    n = node_mon(N_TYPE, type, (node *) NULL);
		    n->class = n2->class;
		    n1->r.right = n;
		}
	    } else if (type != T_MIXED) {
		c_error("incompatible types for = (%s, %s)",
			i_typename(tnbuf1, n1->mod),
			i_typename(tnbuf2, type));
		type = T_MIXED;
	    }
  
	    n = n1->l.left;
	    while (n != (node *) NULL) {
		if (n->type == N_PAIR) {
		    m = n->l.left;
		    n = n->r.right;
		} else {
		    m = n;
		    n = (node *) NULL;
		}
		if (c_tmatch(m->mod, type) == T_NIL) {
		    c_error("incompatible types for = (%s, %s)",
			    i_typename(tnbuf1, m->mod),
			    i_typename(tnbuf2, type));
		}
	    }
	}
	n1 = node_bin(N_ASSIGN, n2->mod, n1, n2);
	n1->class = n2->class;
	return n1;
    } else {
	if (typechecking && (!c_nil(n2) || !T_POINTER(n1->mod))) {
	    /*
	     * typechecked
	     */
	    if (c_tmatch(n1->mod, n2->mod) == T_NIL) {
		c_error("incompatible types for = (%s, %s)",
			i_typename(tnbuf1, n1->mod),
			i_typename(tnbuf2, n2->mod));
	    } else if ((n1->mod != T_MIXED && n2->mod == T_MIXED) ||
		       (n1->mod == T_CLASS &&
			(n2->mod != T_CLASS ||
			 str_cmp(n1->class, n2->class) != 0))) {
		n2 = node_mon(N_CAST, n1->mod, n2);
		n2->class = n1->class;
	    }
	}

	n2 = node_bin(N_ASSIGN, n1->mod, n1, n2);
	n2->class = n1->class;
	return n2;
    }
}

/*
 * NAME:	comma()
 * DESCRIPTION:	handle the comma operator, rearranging the order of the
 *		expression if needed
 */
static node *comma(n1, n2)
register node *n1, *n2;
{
    if (n2->type == N_COMMA) {
	/* a, (b, c) --> (a, b), c */
	n2->l.left = comma(n1, n2->l.left);
	return n2;
    } else {
	n1 = node_bin(N_COMMA, n2->mod, n1, n2);
	n1->class = n2->class;
	return n1;
    }
}
