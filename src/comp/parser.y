/*
 * LPC grammar, handles construction of parse trees and type checking.
 * Currently there are two shift/reduce conflicts, one on opt_inherit_label
 * and one on else.  These could be disposed of by moving the ugliness from
 * the grammar to the C code.
 * The node->mod field is used to store the type of an expression. (!)
 */

%{

# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "macro.h"
# include "token.h"
# include "ppcontrol.h"
# include "node.h"
# include "compile.h"

# define yylex()	pp_gettok()

static int nerrors;		/* number of errors encountered so far */
static int ndeclarations;	/* number of declarations */
static int nstatements;		/* number of statements in current function */
static bool typechecking;	/* does the current function have it? */

%}


/*
 * keywords. The order is determined in tokenz() in the lexical scanner.
 */
%token FOR VOID MIXED INHERIT INT MAPPING NOMASK STATIC STRING CASE RETURN
       DO BREAK CONTINUE DEFAULT PRIVATE IF VARARGS WHILE ELSE SWITCH OBJECT

/*
 * composite tokens
 */
%token ARROW PLUS_PLUS MIN_MIN LSHIFT RSHIFT LE GE EQ NE LAND LOR
       PLUS_EQ MIN_EQ MULT_EQ DIV_EQ MOD_EQ LSHIFT_EQ RSHIFT_EQ AND_EQ
       XOR_EQ OR_EQ COLON_COLON DOT_DOT STRING_CONST IDENTIFIER

%union {
    Int number;			/* lex input */
    unsigned short type;	/* internal */
    struct _node_ *node;	/* internal */
}

/*
 * token types
 */
%token <number> INT_CONST

/*
 * lexical scanner tokens
 */
%token MARK HASH HASH_HASH INCL_CONST NR_TOKENS

/*
 * rule types
 */
%type <type> class_specifier_list class_specifier type_specifier star_list
%type <node> opt_inherit_label formals_declaration formal_declaration_list
	     formal_declaration data_dcltr function_dcltr dcltr list_dcltr
	     dcltr_or_stmt_list dcltr_or_stmt stmt compound_stmt function_name
	     primary_p1_exp primary_p2_exp postfix_exp prefix_exp cast_exp
	     mult_oper_exp add_oper_exp shift_oper_exp rel_oper_exp
	     equ_oper_exp bitand_oper_exp bitxor_oper_exp bitor_oper_exp
	     and_oper_exp or_oper_exp cond_exp exp list_exp opt_list_exp
	     f_list_exp f_opt_list_exp arg_list opt_arg_list opt_arg_list_comma
	     assoc_arg assoc_arg_list opt_assoc_arg_list_comma ident

%%

program
	:	{
		  nerrors = 0;
		  ndeclarations = 0;
		}
	  top_level_declarations
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
	: INHERIT opt_inherit_label exp ';'
		{
		  if (ndeclarations > 0) {
		      yyerror("inherit must precede all declarations");
		  } else if ($3->type != N_STR) {
		      yyerror("inherit argument must be a string constant");
		  } else if (!c_inherit($3->l.string->text, $2) && nerrors == 0)
		  {
		      /*
		       * The object to be inherited is unloaded. Load it first,
		       * then recompile.
		       */
		      YYACCEPT;
		  }
		}
	| data_declaration
		{ ndeclarations++; }
	| function_declaration
		{ ndeclarations++; }
	;

/* will cause shift/reduce conflict */
opt_inherit_label
	: /* empty */
		{ $$ = (node *) NULL; }
	| ident
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
		  typechecking = FALSE;
		  c_function($1, T_INVALID, node_bin(N_FUNC, 0, $2, $4));
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
	;

formal_declaration_list
	: formal_declaration
	| formal_declaration_list ',' formal_declaration
		{ $$ = node_bin(N_PAIR, 0, $1, $3); }
	;

formal_declaration
	: type_specifier data_dcltr
		{
		  $$ = $2;
		  $$->mod |= $1;
		}
	| ident {
		  $$ = $1;
		  $$->mod = T_INVALID;	/* only if typechecking, though */
		}
	;

class_specifier_list
	: /* empty */
		{ $$ = 0; }
	| class_specifier_list class_specifier
		{ $$ = $1 | $2; }
	;

class_specifier
	: PRIVATE
		{ $$ = C_PRIVATE | C_STATIC | C_LOCAL; }
	| STATIC
		{ $$ = C_STATIC; }
	| NOMASK
		{ $$ = C_NOMASK | C_LOCAL; }
	| VARARGS
		{ $$ = C_VARARGS; }
	;

type_specifier
	: INT	{ $$ = T_NUMBER; }
	| STRING
		{ $$ = T_STRING; }
	| OBJECT
		{ $$ = T_OBJECT; }
	| MAPPING
		{ $$ = T_MAPPING; }
	| MIXED	{ $$ = T_MIXED; }
	| VOID	{ $$ = T_VOID; }
	;

star_list
	: /* empty */
		{ $$ = 0; }
	| star_list '*'
		{
		  $$ = ($1 + (1 << REFSHIFT)) & T_REF;
		  if ($$ == 0) {
		      $$ = T_REF;
		  }
		}
	;

data_dcltr
	: star_list ident
		{
		  $$ = $2;
		  $$->mod = $1;
		}
	;

function_dcltr
	: star_list ident '(' formals_declaration ')'
		{ $$ = node_bin(N_FUNC, $1, $2, $4); }
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
		      yyerror("declaration after statement");
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

stmt
	: list_exp ';'
		{ $$ = c_exp_stmt($1); }
	| compound_stmt
	| IF '(' f_list_exp ')' stmt
		{
		  t_void($3);
		  $$ = c_if($3, $5, (node *) NULL);
		}
/* will cause shift/reduce conflict */
	| IF '(' f_list_exp ')' stmt ELSE stmt
		{
		  t_void($3);
		  $$ = c_if($3, $5, $7);
		}
	| DO	{ c_loop(); }
	  stmt WHILE '(' f_list_exp ')' ';'
		{
		  t_void($6);
		  $$ = c_do($6, $3);
		}
	| WHILE '(' f_list_exp ')'
		{
		  t_void($3);
		  c_loop();
		}
	  stmt	{ $$ = c_while($3, $6); }
	| FOR '(' f_opt_list_exp ';' f_opt_list_exp ';' f_opt_list_exp ')'
		{
		  t_void($5);
		  c_loop();
		}
	  stmt	{ $$ = c_for($3, $5, $7, $10); }
	| SWITCH '(' f_list_exp ')'
		{ c_startswitch($3, typechecking); }
	  compound_stmt
		{ $$ = c_endswitch($3, $6); }
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
		{ $$ = c_break(); }
	| CONTINUE ';'
		{ $$ = c_continue(); }
	| RETURN f_opt_list_exp ';'
		{
		  if ($2 == (node *) NULL) {
		      /*
		       * always possible
		       */
		      $$ = node_mon(N_RETURN, 0, node_int((Int) 0));
		  } else {
		      if (typechecking) {
			  unsigned short type;

			  type = c_ftype();
			  if (type == T_VOID) {
			      /*
			       * can't return anything from a void function
			       */
			      yyerror("value returned from void function");
			  } else if (($2->type != N_INT || $2->l.number != 0) &&
				     c_tmatch($2->mod, type) == T_INVALID) {
			      /*
			       * type error
			       */
			      yyerror("returned type doesn't match %s (%s)",
				      c_typename(type), c_typename($2->mod));
			  } else if (type == T_NUMBER && $2->mod == T_MIXED) {
			      /* cast return value to int */
			      $2 = node_mon(N_CAST, T_NUMBER, $2);
			  }
		      }
		      $$ = node_mon(N_RETURN, 0, $2);
		  }
		}
	| ';'	{ $$ = (node *) NULL; }
	;

compound_stmt
	: '{'	{
		  nstatements = 0;
		  c_startcompound();
		}
	  dcltr_or_stmt_list '}'
		{
		  nstatements = 1;	/* any non-zero value will do */
		  $$ = c_endcompound($3);
		}
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
	| STRING_CONST
		{ $$ = node_str(str_new(yytext, (long) yyleng)); }
	| '(' '{' opt_arg_list_comma '}' ')'
		{ $$ = node_mon(N_AGGR, T_MIXED | (1 << REFSHIFT), $3); }
	| '(' '[' opt_assoc_arg_list_comma ']' ')'
		{ $$ = node_mon(N_AGGR, T_MAPPING, $3); }
	| ident	{
		  $$ = c_variable($1);
		  if (typechecking) {
		      if ($$->type == N_GLOBAL && $$->mod == T_NUMBER) {
			  /*
			   * global vars might be modified by untypechecked
			   * functions...
			   */
			  $$ = node_mon(N_CAST, T_NUMBER, $$);
		      }
		  } else {
		      /* the variable could be anything */
		      $$->mod = T_MIXED;
		  }
		}
	| '(' list_exp ')'
		{ $$ = $2; }
	| function_name '(' opt_arg_list ')'
		{
		  $$ = c_funcall($1, $3);
		  if (typechecking) {
		      $$ = c_checklval($$);
		  } else if ($$->mod == T_VOID) {
		      $$->mod = T_NUMBER;	/* get rid of void */
		  }
		}
	| primary_p2_exp ARROW ident '(' opt_arg_list ')'
		{
		  $$ = c_arrow(c_list_exp($1), $3, $5);
		  if ($$->mod == T_VOID && !typechecking) {
		      /*
		       * This is not impossible -- call_other() may have been
		       * redeclared.
		       */
		      $$->mod = T_NUMBER;	/* get rid of void */
		  }
		}
	;

primary_p2_exp
	: primary_p1_exp
	| primary_p2_exp '[' f_list_exp ']'
		{ $$ = idx($1, $3); }
	| primary_p2_exp '[' f_list_exp DOT_DOT f_list_exp ']'
		{ $$ = range($1, $3, $5); }
	;

postfix_exp
	: primary_p2_exp
	| postfix_exp PLUS_PLUS
		{ $$ = assign_uni(N_PLUS_PLUS, $1, "++"); }
	| postfix_exp MIN_MIN
		{ $$ = assign_uni(N_MIN_MIN, $1, "--"); }
	;

prefix_exp
	: postfix_exp
	| PLUS_PLUS cast_exp
		{
		  $$ = assign_uni(N_ADD_EQ, $2, "++");
		  $$->r.right = node_int((Int) 1);  /* convert to binary op */
		}
	| MIN_MIN cast_exp
		{
		  $$ = assign_uni(N_SUB_EQ, $2, "--");
		  $$->r.right = node_int((Int) 1);  /* convert to binary op */
		}
	| '-' cast_exp
		{
		  $$ = $2;
		  if (t_uni($$, "unary -")) {
		      $$ = sub(N_SUB, node_int((Int) 0), $$, "-");
		  }
		}
	| '+' cast_exp
		{
		  $$ = $2;
		  t_uni($$, "unary +");
		}
	| '!' cast_exp
		{
		  t_void($2);
		  $$ = c_not($2);
		}
	| '~' cast_exp
		{
		  $$ = $2;
		  if (t_uni($$, "~")) {
		      $$ = xor(N_XOR, $$, node_int((Int) -1), "^");
		  }
		}
	;

cast_exp
	: prefix_exp
	| '(' type_specifier star_list ')' cast_exp
		{
		  unsigned short type;

		  type = $2 | $3;
		  $$ = $5;
		  if (type != $$->mod) {
		      if (($$->mod & T_TYPE) != T_MIXED) {
			  yyerror("cast of non-mixed type (%s)",
				  c_typename($$->mod));
		      } else if ($3 < ($$->mod & T_REF)) {
			  yyerror("illegal cast of array type (%s)",
				  c_typename($$->mod));
		      } else if ($2 == T_VOID) {
			  yyerror("cannot cast to %s", c_typename(type));
			  type = T_MIXED;
		      } else if (typechecking && type == T_NUMBER) {
			  /* only casts to int are kept */
			  $$ = node_mon(N_CAST, 0, $$);
		      }
		      $$->mod = type;
		  }
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
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number <<= $3->l.number;
		  } else {
		      $$ = bini(N_LSHIFT, $$, $3, "<<");
		  }
		}
	| shift_oper_exp RSHIFT add_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number >>= $3->l.number;
		  } else {
		      $$ = bini(N_RSHIFT, $$, $3, ">>");
		  }
		}
	;

rel_oper_exp
	: shift_oper_exp
	| rel_oper_exp '<' shift_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number = ($$->l.number < $3->l.number);
		  } else if ($$->type == N_STR && $3->type == N_STR) {
		      $$ = node_int((Int) (strcmp($$->l.string->text,
						  $3->l.string->text) < 0));
		  } else {
		      $$ = rel(N_LT, $$, $3, "<");
		  }
		}
	| rel_oper_exp '>' shift_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number = ($$->l.number > $3->l.number);
		  } else if ($$->type == N_STR && $3->type == N_STR) {
		      $$ = node_int((Int) (strcmp($$->l.string->text,
						  $3->l.string->text) > 0));
		  } else {
		      $$ = rel(N_GT, $$, $3, ">");
		  }
		}
	| rel_oper_exp LE shift_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number = ($$->l.number <= $3->l.number);
		  } else if ($$->type == N_STR && $3->type == N_STR) {
		      $$ = node_int((Int) (strcmp($$->l.string->text,
						  $3->l.string->text) <= 0));
		  } else {
		      $$ = rel(N_LE, $$, $3, "<=");
		  }
		}
	| rel_oper_exp GE shift_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number = ($$->l.number >= $3->l.number);
		  } else if ($$->type == N_STR && $3->type == N_STR) {
		      $$ = node_int((Int) (strcmp($$->l.string->text,
						  $3->l.string->text) >= 0));
		  } else {
		      $$ = rel(N_GE, $$, $3, ">=");
		  }
		}
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
	| or_oper_exp '?' f_list_exp ':' cond_exp
		{
		  $$ = c_quest($1, $3, $5);
		  $$->mod = t_quest($1, $3, $5);
		}
	;

exp
	: cond_exp
	| cond_exp '=' exp
		{ $$ = assign(c_lvalue($1, "assignment"), asgnop($1, $3)); }
	| cond_exp PLUS_EQ exp
		{
		  $$ = add(N_ADD_EQ, c_lvalue($1, "+="), asgnop($1, $3), "+=");
		}
	| cond_exp MIN_EQ exp
		{
		  $$ = sub(N_SUB_EQ, c_lvalue($1, "-="), asgnop($1, $3), "-=");
		}
	| cond_exp MULT_EQ exp
		{
		  $$ = mult(N_MULT_EQ, c_lvalue($1, "*="), asgnop($1, $3),
			    "*=");
		}
	| cond_exp DIV_EQ exp
		{
		  $$ = mdiv(N_DIV_EQ, c_lvalue($1, "/="), asgnop($1, $3), "/=");
		}
	| cond_exp MOD_EQ exp
		{
		  $$ = mod(N_MOD_EQ, c_lvalue($1, "%="), asgnop($1, $3), "%=");
		}
	| cond_exp RSHIFT_EQ exp
		{
		  $$ = bini(N_RSHIFT_EQ, c_lvalue($1, ">>="), asgnop($1, $3),
			    ">>=");
		}
	| cond_exp LSHIFT_EQ exp
		{
		  $$ = bini(N_LSHIFT_EQ, c_lvalue($1, "<<="), asgnop($1, $3),
			    "<<=");
		}
	| cond_exp AND_EQ exp
		{
		  $$ = and(N_AND_EQ, c_lvalue($1, "&="), asgnop($1, $3), "&=");
		}
	| cond_exp XOR_EQ exp
		{
		  $$ = xor(N_XOR_EQ, c_lvalue($1, "^="), asgnop($1, $3), "^=");
		}
	| cond_exp OR_EQ exp
		{
		  $$ = or(N_OR_EQ, c_lvalue($1, "|="), asgnop($1, $3), "|=");
		}
	;

list_exp
	: exp
	| list_exp ',' exp
		{
		  if ($3->type == N_COMMA) {
		      /* minor tweak to help condition optimisation */
		      $$ = node_bin(N_COMMA, $3->mod, $3, $3->r.right);
		      $3->r.right = $3->l.left;
		      $3->l.left = $1;
		  } else {
		      $$ = node_bin(N_COMMA, $3->mod, $1, $3);
		  }
		}
	;

opt_list_exp
	: /* empty */
		{ $$ = (node *) NULL; }
	| list_exp
	;

f_list_exp
	: list_exp
		{ $$ = c_list_exp($1); }
	;

f_opt_list_exp
	: opt_list_exp
		{
		  $$ = $1;
		  if ($$ != (node *) NULL) {
		      $$ = c_list_exp($$);
		  }
		}
	;

arg_list
	: exp	{ $$ = c_list_exp($1); }
	| arg_list ',' exp
		{ $$ = node_bin(N_COMMA, $3->mod, $1, c_list_exp($3)); }
	;

opt_arg_list
	: /* empty */
		{ $$ = (node *) NULL; }
	| arg_list
	;

opt_arg_list_comma
	: /* empty */
		{ $$ = (node *) NULL; }
	| arg_list
	| arg_list ','
		{ $$ = $1; }
	;

assoc_arg
	: exp ':' exp
		{ $$ = node_bin(N_PAIR, 0, c_list_exp($1), c_list_exp($3)); }
	;

assoc_arg_list
	: assoc_arg
	| assoc_arg_list ',' assoc_arg
		{ $$ = node_bin(N_COMMA, 0, $1, $3); }
	;

opt_assoc_arg_list_comma
	: /* empty */
		{ $$ = (node *) NULL; }
	| assoc_arg_list
	| assoc_arg_list ','
		{ $$ = $1; }
	;

ident
	: IDENTIFIER
		{ $$ = node_str(str_new(yytext, (long) yyleng)); }
	;

%%

/*
 * NAME:	yyerror()
 * DESCRIPTION:	Produce a warning with the supplied error message.
 */
void yyerror(f, a1, a2, a3)
char *f, *a1, *a2, *a3;
{
    char buf[4 * STRINGSZ];	/* file name + 2 * string + overhead */

    sprintf(buf, "\"/%s\", %u: ", tk_filename(), tk_line());
    sprintf(buf + strlen(buf), f, a1, a2, a3);
    message("%s\n", buf);
    nerrors++;
}

/*
 * NAME:	t_void()
 * DESCRIPTION:	if the argument is of type void, an error will result
 */
static void t_void(n)
register node *n;
{
    if (n != (node *) NULL && n->mod == T_VOID) {
	yyerror("void value not ignored");
	n->mod == T_MIXED;
    }
}

/*
 * NAME:	t_uni()
 * DESCRIPTION:	typecheck the argument of a unary int operator
 */
static bool t_uni(n, name)
register node *n;
char *name;
{
    if (typechecking && n->mod != T_NUMBER && n->mod != T_MIXED) {
	yyerror("bad argument type for %s (%s)", name, c_typename(n->mod));
	n->mod = T_MIXED;
	return FALSE;
    }
    return TRUE;
}

/*
 * NAME:	override()
 * DESCRIPTION:	handle n op m --> (n, m), n = m
 */
static node *override(n1, n2)
register node *n1, *n2;
{
    if (n1->type == N_LVALUE) {
	n1 = n1->l.left;
	return node_bin(N_ASSIGN, n1->mod, n1, n2);
    } else {
	return node_bin(N_COMMA, n1->mod, n1, n2);
    }
}

/*
 * NAME:	idx()
 * DESCRIPTION:	handle the [ ] operator
 */
static node *idx(n1, n2)
register node *n1, *n2;
{
    register unsigned short type;

    if (n1->type == N_STR && n2->type == N_INT) {
	/* str [ int ] */
	n2->l.number = UCHAR(n1->l.string->text[str_index(n1->l.string,
						         (long) n2->l.number)]);
	return n2;
    }

    if ((n1->mod & T_REF) != 0) {
	/*
	 * array
	 */
	if (typechecking && n2->mod != T_NUMBER && n2->mod != T_MIXED) {
	    yyerror("bad index type (%s)", c_typename(n2->mod));
	}
	type = n1->mod - (1 << REFSHIFT);
	if (type == T_NUMBER) {
	    /* you can't trust these arrays */
	    return node_mon(N_CAST, T_NUMBER,
			    node_bin(N_INDEX, T_NUMBER, n1, n2));
	}
    } else if (n1->mod == T_STRING) {
	/*
	 * string
	 */
	if (typechecking && n2->mod != T_NUMBER && n2->mod != T_MIXED) {
	    yyerror("bad index type (%s)", c_typename(n2->mod));
	}
	type = T_NUMBER;
    } else {
	type = T_MIXED;
	if (typechecking && n1->mod != T_MAPPING && n1->mod != T_MIXED) {
	    yyerror("bad indexed type (%s)", c_typename(n1->mod));
	}
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
    if (n1->type == N_STR && n2->type == N_INT && n3->type == N_INT) {
	/* str [ int .. int ] */
	return node_str(str_range(n1->l.string, (long) n2->l.number,
				  (long) n3->l.number));
    }
    if (typechecking) {
	/* indices */
	if ((n2->mod != T_NUMBER && n2->mod != T_MIXED) ||
	    (n3->mod != T_NUMBER && n3->mod != T_MIXED)) {
	    yyerror("bad index type (%s)", c_typename(n2->mod));
	}
	/* range */
	if ((n1->mod & T_REF) == 0 && n1->mod != T_STRING && n1->mod != T_MIXED)
	{
	    yyerror("bad indexed type (%s)", c_typename(n1->mod));
	}
    }

    return node_bin(N_RANGE, n1->mod, n1, node_bin(N_PAIR, 0, n2, n3));
}

/*
 * NAME:	assign_uni()
 * DESCRIPTION:	handle a unary int assignment operator
 */
static node *assign_uni(op, n, name)
int op;
node *n;
char *name;
{
    n = c_lvalue(n, name);
    t_uni(n, name);
    if (n->mod == T_NUMBER) {
	op++;
    }
    return node_mon(op, n->mod, n);
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
    if (typechecking &&
	((n1->mod != T_NUMBER && n1->mod != T_MIXED) ||
	 (n2->mod != T_NUMBER && n2->mod != T_MIXED))) {
	yyerror("bad argument types for %s (%s, %s)", name,
		c_typename(n1->mod), c_typename(n2->mod));
    }
    if (n1->mod == T_NUMBER && n2->mod == T_NUMBER) {
	op++;
    }
    return node_bin(op, T_NUMBER, n1, n2);
}


/*
 * NAME:	mult()
 * DESCRIPTION:	handle the * operator
 */
static node *mult(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n1->type == N_INT) {
	node *n;

	/* int * foo --> foo * int */
	n = n1;
	n1 = n2;
	n2 = n;
    }

    if (n2->type == N_INT) {
	if (n2->l.number == 0 && n1->mod == T_NUMBER) {
	    /* intfoo * 0 */
	    return override(n1, n2);
	}
	if (n1->type == N_INT) {
	    /* int * int */
	    n1->l.number *= n2->l.number;
	    return n1;
	}
	if ((n1->type == N_MULT || n1->type == N_MULT_INT) &&
	    n1->r.right->type == N_INT) {
	    /* (foo * int) * int */
	    n1->r.right->l.number *= n2->l.number;
	    return n1;
	}
    } else if ((n1->type == N_MULT || n1->type == N_MULT_INT) &&
	       (n2->type == N_MULT || n2->type == N_MULT_INT) &&
	       n1->r.right->type == N_INT && n2->r.right->type == N_INT) {
	/* (foo * int) * (foo * int) */
	n1->r.right->l.number *= n2->r.right->l.number;
	return node_bin(N_MULT_INT, T_NUMBER,
			mult(N_MULT, n1->l.left, n2->l.left, "*"), n1->r.right);
    }

    /* foo * int, foo * foo */
    return bini(op, n1, n2, name);
}

/*
 * NAME:	mdiv()
 * DESCRIPTION:	handle the / operator
 */
static node *mdiv(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n2->type == N_INT) {
	if (n1->type == N_INT) {
	    /* int / int */
	    if (n2->l.number == 0) {
		yyerror("division by zero in constant");
	    } else {
		n1->l.number /= n2->l.number;
	    }
	    return n1;
	}
	if ((n1->type == N_DIV || n1->type == N_DIV_INT) &&
	    n1->r.right->type == N_INT) {
	    /* (foo / int) / int */
	    n1->r.right->l.number *= n2->l.number;
	    return n1;
	}
    }
    /* foo / int, foo / foo */
    return bini(op, n1, n2, name);
}

/*
 * NAME:	mod()
 * DESCRIPTION:	handle the % operator
 */
static node *mod(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n2->type == N_INT) {
	if (n2->l.number == 1 && n1->mod == T_NUMBER) {
	    /* intfoo % 1 */
	    return override(n1, node_int((Int) 0));
	}
	if (n1->type == N_INT) {
	    /* int % int */
	    if (n2->l.number == 0) {
		yyerror("modulus by zero in constant");
	    } else {
		n1->l.number %= n2->l.number;
	    }
	    return n1;
	}
    }
    /* foo % foo */
    return bini(op, n1, n2, name);
}

/*
 * NAME:	add()
 * DESCRIPTION:	handle the + operator
 */
static node *add(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    char buffer[12];
    register string *str;
    register unsigned short type;

    if (n1->type == N_INT && n2->mod == T_NUMBER) {
	node *n;

	/* int + foo --> foo + int */
	n = n1;
	n1 = n2;
	n2 = n;
    }

    if (n2->type == N_INT) {
	if (n1->type == N_INT) {
	    /* int + int */
	    n1->l.number += n2->l.number;
	    return n1;
	}
	if (n1->mod == T_STRING) {
	    /* strfoo + int */
	    sprintf(buffer, "%ld", (long) n2->l.number);
	    n2 = node_str(str_new(buffer, (long) strlen(buffer)));
	    if (n1->type == N_STR) {
		/* str + int */
		return node_str(str_add(n1->l.string, n2->l.string));
	    }
	} else if (n1->type == N_ADD_INT && n1->r.right->type == N_INT) {
	    /* (intfoo + int) + int */
	    n1->r.right->l.number += n2->l.number;
	    return n1;
	} else if (n1->type == N_ADD && n1->r.right->type == N_STR) {
	    /* (foo + str) + int */
	    sprintf(buffer, "%ld", (long) n2->l.number);
	    str = str_new((char *) NULL, n1->r.right->l.string->len +
					 (long) strlen(buffer));
	    memcpy(str->text, n1->r.right->l.string->text,
		   n1->r.right->l.string->len);
	    strcpy(str->text + n1->r.right->l.string->len, buffer);
	    n1->r.right = node_str(str);
	    return n1;
	} else if ((n1->type == N_SUB || n1->type == N_SUB_INT) &&
	    n1->l.left->type == N_INT) {
	    /* (int - foo) + int */
	    n1->l.left->l.number += n2->l.number;
	    return n1;
	}
	/* foo + int */
    } else if (n2->mod == T_STRING) {
	if (n1->type == N_INT) {
	    /* int + strfoo */
	    sprintf(buffer, "%ld", (long) n1->l.number);
	    n1 = node_str(str_new(buffer, (long) strlen(buffer)));
	}
	if (n2->type == N_STR) {
	    if (n1->type == N_STR) {
		/* str + str */
		return node_str(str_add(n1->l.string, n2->l.string));
	    }
	    if (n1->type == N_ADD && n1->r.right->type == N_STR) {
		/* (foo + str) + str */
		n1->r.right = node_str(str_add(n1->r.right->l.string,
					       n2->l.string));
		return n1;
	    }
	}
	/* foo + strfoo */
    } else if (n1->type == N_ADD_INT && n1->r.right->type == N_INT &&
	       n2->type == N_ADD_INT && n2->r.right->type == N_INT) {
	/* (intfoo + int) + (intfoo + int) */
	n1->r.right->l.number += n2->r.right->l.number;
	return node_bin(N_ADD_INT, T_NUMBER,
			node_bin(N_ADD_INT, T_NUMBER, n1->l.left, n2->l.left),
			n1->r.right);
    }

    /* foo + int, foo + str, foo + foo */
    type = c_tmatch(n1->mod, n2->mod);
    if (type == T_OBJECT || type == T_VOID ||
	(type == T_INVALID &&	/* only if not adding int to string */
	 (((n1->mod != T_NUMBER || op == N_ADD_EQ) &&
	   n1->mod != T_STRING) ||
	   (n2->mod != T_NUMBER && n2->mod != T_STRING)))) {
	type = T_MIXED;
	if (typechecking) {
	    yyerror("bad argument types for %s (%s, %s)", name,
		    c_typename(n1->mod), c_typename(n2->mod));
	}
    } else if (type == T_INVALID) {
	type = T_STRING;
    } else if (type == T_NUMBER) {
	op++;
    }
    return node_bin(op, type, n1, n2);
}

/*
 * NAME:	sub()
 * DESCRIPTION:	handle the - operator
 */
static node *sub(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    register unsigned short type;

    if (n2->type == N_INT) {
	n2->l.number = -n2->l.number;
	if (op == N_SUB) {
	    /* - */
	    return add(N_ADD, n1, n2, "+");
	} else {
	    /* -= */
	    return add(N_ADD_EQ, n1, n2, "+=");
	}
    }
    if (n1->type == N_INT) {
	if ((n2->type == N_SUB || n2->type == N_SUB_INT) &&
	    n2->l.left->type == N_INT) {
	    /* int - (int - foo) */
	    n1->l.number -= n2->l.left->l.number;
	    return add(N_ADD, n2->r.right, n1, "+");
	}
	if (n2->type == N_ADD_INT && n2->r.right->type == N_INT) {
	    /* int - (intfoo + int) */
	    n1->l.number -= n2->r.right->l.number;
	    return node_bin(N_SUB_INT, T_NUMBER, n1, n2->l.left);
	}
    } else if (n1->type == N_ADD_INT && n1->r.right->type == N_INT &&
	       n2->type == N_ADD_INT && n2->r.right->type == N_INT) {
	/* (intfoo + int) - (intfoo + int) */
	n1->r.right->l.number -= n2->r.right->l.number;
	return node_bin(N_ADD_INT, T_NUMBER,
			node_bin(N_SUB_INT, T_NUMBER, n1->l.left, n2->l.left),
			n1->r.right);
    }

    /* int - foo, foo - foo */
    type = c_tmatch(n1->mod, n2->mod);
    if (type == T_STRING || type == T_OBJECT || type == T_VOID ||
	type == T_INVALID) {
	type = T_MIXED;
	if (typechecking) {
	    yyerror("bad argument types for %s (%s, %s)", name,
		    c_typename(n1->mod), c_typename(n2->mod));
	}
    } else if (type == T_NUMBER) {
	op++;
    }
    return node_bin(op, type, n1, n2);
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
    register unsigned short type;

    type = c_tmatch(n1->mod, n2->mod);
    if (typechecking && type != T_NUMBER && type != T_STRING && type != T_MIXED)
    {
	yyerror("bad argument types for %s (%s, %s)", name,
		c_typename(n1->mod), c_typename(n2->mod));
    } else if (type == T_NUMBER) {
	op++;
    }
    return node_bin(op, T_NUMBER, n1, n2);
}

/*
 * NAME:	eq()
 * DESCRIPTION:	handle the == operator
 */
static node *eq(n1, n2)
register node *n1, *n2;
{
    t_void(n1);
    t_void(n2);

    switch (n1->type) {
    case N_INT:
	if (n2->type == N_INT) {
	    /* int == int */
	    n1->l.number = (n1->l.number == n2->l.number);
	    return n1;
	}
	if (n1->l.number == 0) {
	    if (n2->type == N_STR) {
		/* 0 == str */
		return n1;	/* FALSE */
	    }
	    /* 0 == foo */
	    return c_not(n2);
	}
	break;

    case N_STR:
	if (n2->type == N_STR) {
	    /* str == str */
	    return node_int((Int) (strcmp(n1->l.string->text,
					  n2->l.string->text) == 0));
	}
	if (n2->type == N_INT && n2->l.number == 0) {
	    /* str == 0 */
	    return n2;	/* FALSE */
	}
	break;

    default:
	if (n2->type == N_INT && n2->l.number == 0) {
	    /* foo == 0 */
	    return c_not(n1);
	}
	break;
    }

    /* foo == foo */
    if (n1->mod != n2->mod && n1->mod != T_MIXED && n2->mod != T_MIXED &&
	(n1->type != N_INT || n1->l.number != 0) &&
	(n2->type != N_INT || n2->l.number != 0)) {
	if (typechecking) {
	    yyerror("incompatible types for comparision (%s, %s)",
		    c_typename(n1->mod), c_typename(n2->mod));
	}
    } else if (n1->mod == T_NUMBER && n2->mod == T_NUMBER) {
	return node_bin(N_EQ_INT, T_NUMBER, n1, n2);
    }
    return node_bin(N_EQ, T_NUMBER, n1, n2);
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
    unsigned short type;

    if (n1->type == N_INT) {
	node *n;

	/* int & foo --> foo & int */
	n = n1;
	n1 = n2;
	n2 = n;
    }

    if (n2->type == N_INT) {
	if (n2->l.number == 0 && n1->mod == T_NUMBER) {
	    /* intfoo & 0 */
	    return override(n1, n2);
	}
	if (n1->type == N_INT) {
	    /* int & int */
	    n1->l.number &= n2->l.number;
	    return n1;
	}
	if ((n1->type == N_AND || n1->type == N_AND_INT) &&
	    n1->r.right->type == N_INT) {
	    /* (foo & int) & int */
	    n1->r.right->l.number &= n2->l.number;
	    if (n1->r.right->l.number == 0 && n1->l.left->mod == T_NUMBER) {
		/* intfoo & 0 */
		n1->type = N_COMMA;
	    }
	    return n1;
	}
    } else if ((n1->mod == T_MIXED && (type=n2->mod) == T_MIXED) ||
	       ((type=c_tmatch(n1->mod, n2->mod)) & T_REF) != 0) {
	/*
	 * possibly array & array
	 */
	return node_bin(op, type, n1, n2);
    } else if ((n1->type == N_AND || n1->type == N_AND_INT) &&
	       (n2->type == N_AND || n2->type == N_AND_INT) &&
	       n1->r.right->type == N_INT && n2->r.right->type == N_INT) {
	/* (foo & int) & (foo & int) */
	n1->r.right->l.number &= n2->r.right->l.number;
	n2 = and(N_AND, n1->l.left, n2->l.left, "&");
	if (n1->r.right->l.number == 0) {
	    /* (foo & foo) & 0 */
	    op = N_COMMA;
	} else if (n2->mod == T_NUMBER) {
	    op++;
	}
	return node_bin(op, T_NUMBER, n2, n1->r.right);
    }

    /* foo & int, foo & foo */
    return bini(op, n1, n2, name);
}

/*
 * NAME:	xor()
 * DESCRIPTION:	handle the ^ operator
 */
static node *xor(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n1->type == N_INT) {
	node *n;

	/* int ^ foo --> foo ^ int */
	n = n1;
	n1 = n2;
	n2 = n;
    }

    if (n2->type == N_INT) {
	if (n1->type == N_INT) {
	    /* int ^ int */
	    n1->l.number ^= n2->l.number;
	    return n1;
	}
	if ((n1->type == N_XOR || n1->type == N_XOR_INT) &&
	    n1->r.right->type == N_INT) {
	    /* (foo ^ int) ^ int */
	    n1->r.right->l.number ^= n2->l.number;
	    return n1;
	}
    } else if ((n1->type == N_XOR || n1->type == N_XOR_INT) &&
	       (n2->type == N_XOR || n2->type == N_XOR_INT) &&
	       n1->r.right->type == N_INT && n2->r.right->type == N_INT) {
	/* (foo ^ int) ^ (foo ^ int) */
	n1->r.right->l.number ^= n2->r.right->l.number;
	return node_bin(N_XOR_INT, T_NUMBER,
			xor(N_XOR, n1->l.left, n2->l.left, "^"),
			n1->r.right);
    }

    /* foo ^ int, foo ^ foo */
    return bini(op, n1, n2, name);
}

/*
 * NAME:	or()
 * DESCRIPTION:	handle the | operator
 */
static node *or(op, n1, n2, name)
int op;
register node *n1, *n2;
char *name;
{
    if (n1->type == N_INT) {
	node *n;

	/* int | foo --> foo | int */
	n = n1;
	n1 = n2;
	n2 = n;
    }

    if (n2->type == N_INT) {
	if (n2->l.number == -1 && n1->mod == T_NUMBER) {
	    /* intfoo | -1 */
	    return override(n1, n2);
	}
	if (n1->type == N_INT) {
	    /* int | int */
	    n1->l.number |= n2->l.number;
	    return n1;
	}
	if ((n1->type == N_OR || n1->type == N_OR_INT) &&
	    n1->r.right->type == N_INT) {
	    /* (foo | int) | int */
	    n1->r.right->l.number |= n2->l.number;
	    if (n1->r.right->l.number == -1 && n1->l.left->mod == T_NUMBER) {
		/* intfoo | -1 */
		n1->type = N_COMMA;
	    }
	    return n1;
	}
    } else if ((n1->type == N_OR || n1->type == N_OR_INT) &&
	       (n2->type == N_OR || n2->type == N_OR_INT) &&
	       n1->r.right->type == N_INT && n2->r.right->type == N_INT) {
	/* (foo | int) | (foo | int) */
	n1->r.right->l.number |= n2->r.right->l.number;
	return node_bin((n1->r.right->l.number == -1) ? N_COMMA : N_OR_INT,
			T_NUMBER, or(N_OR, n1->l.left, n2->l.left, "|"),
			n1->r.right);
    }

    /* foo | int, foo | foo */
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
    if (n1->type == N_TST) {
	n1 = n1->l.left;
    }
    if (n2->type == N_TST) {
	n2 = n2->l.left;
    }

    if (n1->type == N_STR) {
	/* str && foo */
	return c_tst(n2);
    }
    if (n2->type == N_STR) {
	/* foo && str */
	return c_tst(n1);
    }
    if (n1->type == N_INT) {
	if (n1->l.number == 0) {
	    /* 0 && foo */
	    return n1;
	}
	/* true && foo */
	return c_tst(n2);
    }
    if (n2->type == N_INT) {
	if (n2->l.number != 0) {
	    /* foo && true */
	    return c_tst(n1);
	}
	/* foo && 0 */
	return node_bin(N_COMMA, T_NUMBER, n1, n2);
    }
    /* foo && foo */
    return node_bin(N_LAND, T_NUMBER, n1, n2);
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
    if (n1->type == N_TST) {
	n1 = n1->l.left;
    }
    if (n2->type == N_TST) {
	n2 = n2->l.left;
    }

    if (n1->type == N_STR) {
	/* str || foo */
	return node_int((Int) TRUE);
    }
    if (n2->type == N_STR) {
	/* foo || str */
	return node_bin(N_COMMA, T_NUMBER, n1, node_int((Int) 1));
    }
    if (n1->type == N_INT) {
	if (n1->l.number != 0) {
	    /* true || foo */
	    n1->l.number = TRUE;
	    return n1;
	}
	/* 0 || foo */
	return c_tst(n2);
    }
    if (n2->type == N_INT) {
	if (n2->l.number == 0) {
	    /* foo || 0 */
	    return c_tst(n1);
	}
	/* foo || true */
	n2->l.number = TRUE;
	return node_bin(N_COMMA, T_NUMBER, n1, n2);
    }
    /* foo || foo */
    return node_bin(N_LOR, T_NUMBER, n1, n2);
}

/*
 * NAME:	t_quest()
 * DESCRIPTION:	typecheck the ? : operator
 */
static unsigned short t_quest(n1, n2, n3)
register node *n1, *n2, *n3;
{
    t_void(n1);

    if (n2->type == N_INT && n2->l.number == 0) {
	/*
	 * expr ? 0 : expr
	 */
	return n3->mod;
    }
    if (n3->type == N_INT && n3->l.number == 0) {
	/*
	 * expr ? expr : 0;
	 */
	return n2->mod;
    }
    if (typechecking) {
	unsigned short type;

	/*
	 * typechecked
	 */
	type = c_tmatch(n2->mod, n3->mod);
	if (type == T_INVALID) {
	    yyerror("incompatible types for ? : (%s, %s)", c_typename(n2->mod),
		    c_typename(n3->mod));
	    return T_MIXED;
	}
	return type;
    }
    return T_MIXED;
}

/*
 * NAME:	assign()
 * DESCRIPTION:	handle the assignment operator
 */
static node *assign(n1, n2)
register node *n1, *n2;
{
    register unsigned short type;

    if (n2->type == N_INT && n2->l.number == 0) {
	/*
	 * can assign constant 0 to anything anytime
	 */
	type = n1->mod;
    } else if (typechecking) {
	/*
	 * typechecked
	 */
	type = c_tmatch(n1->mod, n2->mod);
	if (type == T_INVALID) {
	    yyerror("incompatible types for = (%s, %s)",
		    c_typename(n1->mod), c_typename(n2->mod));
	    type = T_MIXED;
	}
    } else {
	/*
	 * untypechecked
	 */
	type = T_MIXED;
    }
    return node_bin(N_ASSIGN, type, n1, n2);
}

/*
 * NAME:	asgnop()
 * DESCRIPTION:	check the value of a binary int assignment operator
 */
static node *asgnop(n1, n2)
node *n1, *n2;
{
    if (typechecking && n1->mod == T_NUMBER && n2->mod == T_MIXED) {
	/* can only assign integer values to integer lvalues */
	return node_mon(N_CAST, T_NUMBER, n2);
    }
    return n2;
}
