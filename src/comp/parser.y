/*
 * LPC grammar, handles construction of parse trees and type checking.
 * Currently there are two shift/reduce conflicts, one on opt_inherit_label
 * and one on else.  These could be disposed of by moving the ugliness from
 * the grammar to the C code.
 * The node->mod field is used to store the type of an expression. (!)
 */

%{

# include "comp.h"
# include "lex.h"
# include "interpret.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "data.h"
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
       XOR_EQ OR_EQ COLON_COLON DOT_DOT

%union {
    Int number;			/* lex input */
    char *string;		/* lex input */
    unsigned short type;	/* internal */
    struct _node_ *node;	/* internal */
}

/*
 * token types
 */
%token <number> INT_CONST
%token <string> STRING_CONST IDENTIFIER

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
	     f_list_exp opt_list_exp_comma assoc_exp list_assoc_exp
	     opt_list_assoc_exp_comma ident

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
		     yyerror("inherit statement must precede all declarations");
		     YYABORT;
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
	| error ';'
	;

function_declaration
	: class_specifier_list type_specifier function_dcltr
		{ c_function($1, $2, $3, typechecking = TRUE); }
	  compound_stmt
		{
		  if (nerrors == 0) {
		      c_funcbody($5);
		  }
		}
	| class_specifier_list ident '(' formals_declaration ')'
		{
		  c_function($1, T_ERROR, node_bin(N_FUNC, 0, $2, $4),
			     typechecking = c_typechecking());
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
		{ c_local($1, $2, $3, typechecking); }
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
		  $$->mod = T_ERROR;	/* only if typechecking, though */
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
		  $$ = ($1 | (1 << REFSHIFT)) & T_REF;
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
		{
		  $$ = $1;
		  if ($$ == (node *) NULL) {
		      $$ = $2;
		  } else if ($2 != (node *) NULL && ($2->type == N_CASE ||
			     ($$->type != N_BREAK && $$->type != N_CONTINUE &&
			      $$->type != N_RETURN && ($$->type != N_PAIR ||
			      ($$->r.right->type != N_BREAK &&
			       $$->r.right->type != N_CONTINUE &&
			       $$->r.right->type != N_RETURN))))) {
		      $$ = node_bin(N_PAIR, 0, $$, $2);
		  }
		  /* doesn't handle { foo(); break; } bar(); properly */
		}
	| error ';'
		{
		  if (nerrors >= MAX_ERRORS) {
		      YYABORT;
		  }
		  $$ = (node *) NULL;
		}
	;

dcltr_or_stmt
	: local_data_declaration
		{
		  if (nstatements > 0) {
		      yyerror("local declarations must precede all statements");
		  }
		  $$ = (node *) NULL;
		}
	| stmt	{
		  nstatements++;
		  $$ = $1;
		}
	;

stmt
	: f_list_exp ';'
		{ $$ = c_exp_stmt($1); }
	| compound_stmt
	| IF '(' f_list_exp ')' stmt
		{ $$ = c_if($3, $5, (node *) NULL); }
/* will cause shift/reduce conflict */
	| IF '(' f_list_exp ')' stmt ELSE stmt
		{ $$ = c_if($3, $5, $7); }
	| DO	{ c_loop(); }
	  stmt WHILE '(' f_list_exp ')' ';'
		{ $$ = c_do($6, $3); }
	| WHILE '(' f_list_exp ')'
		{ c_loop(); }
	  stmt	{ $$ = c_while($3, $6); }
	| FOR '(' opt_list_exp ';' opt_list_exp ';' opt_list_exp ')'
		{ c_loop(); }
	  stmt	{ $$ = c_for($3, $5, $7, $10); }
	| SWITCH '(' f_list_exp ')'
		{ c_startswitch($3); }
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
	| RETURN ';'
		{ $$ = node_mon(N_RETURN, 0, node_int((Int) 0)); }
	| RETURN f_list_exp ';'
		{
		  if (typechecking &&
		      ($2->type != N_INT || $2->l.number != 0) &&
		      c_tmatch($2->mod, c_ftype()) == T_ERROR) {
		      /*
		       * type error
		       */
		      yyerror("return type doesn't match %s (%s)",
			      c_typename(c_ftype()), c_typename($2->mod));
		  }
		  $$ = node_mon(N_RETURN, 0, $2);
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
		{ $$ = node_str(str_new($1, (long) strlen($1))); }
	| '(' '{' opt_list_exp_comma '}' ')'
		{ $$ = node_mon(N_AGGR, T_MIXED | 1, $3); }
	| '(' '[' opt_list_assoc_exp_comma ']' ')'
		{ $$ = node_mon(N_AGGR, T_MAPPING, $3); }
	| ident	{ $$ = c_variable($1, typechecking); }
	| '(' f_list_exp ')'
		{ $$ = $2; }
	| function_name '(' opt_list_exp ')'
		{ $$ = c_funcall($1, $3); }
	| primary_p2_exp ARROW ident '(' opt_list_exp ')'
		{ $$ = c_arrow($1, $3, $5); }
	;

primary_p2_exp
	: primary_p1_exp
	| primary_p2_exp '[' f_list_exp ']'
		{
		  if ($1->type == N_STR && $3->type == N_INT) {
		     $$ = $3;
		     $$->l.number = str_index($1->l.string, (Int) $3->l.number);
		  } else {
		      $$ = tc_index($1, $3);
		  }
		}
	| primary_p2_exp '[' f_list_exp DOT_DOT f_list_exp ']'
		{
		  if ($1->type == N_STR && $3->type == N_INT &&
		      $5->type == N_INT) {
		      $$ = node_str(str_range($1->l.string, (Int) $3->l.number,
					      (Int) $5->l.number));
		  } else {
		      $$ = tc_range($1, $3, $5);
		  }
		}
	;

postfix_exp
	: primary_p2_exp
	| postfix_exp PLUS_PLUS
		{ $$ = tc_moni(N_PLUS_PLUS, c_lvalue($1, "++"), "++"); }
	| postfix_exp MIN_MIN
		{ $$ = tc_moni(N_MIN_MIN, c_lvalue($1, "--"), "--"); }
	;

prefix_exp
	: postfix_exp
	| PLUS_PLUS cast_exp
		{
		  $$ = tc_moni(N_ADD_EQ, c_lvalue($2, "++"), "++");
		  $$->r.right = node_int((Int) 1);  /* convert to binary op */
		}
	| MIN_MIN cast_exp
		{
		  $$ = tc_moni(N_MIN_EQ, c_lvalue($2, "--"), "--");
		  $$->r.right = node_int((Int) 1);  /* convert to binary op */
		}
	| '-' cast_exp
		{
		  switch ($2->type) {
		  case N_INT:
		      $$ = $2;
		      $$->l.number = -$2->l.number;
		      break;

		  case N_UMIN:
		      if ($2->mod == T_NUMBER) {
			  $$ = $2->l.left;
			  break;
		      }
		  default:
		      $$ = tc_moni(N_UMIN, $2, "unary -");
		      break;
		  }
		}
	| '+' cast_exp
		{
		  $$ = $2;
		  if (typechecking && $$->mod != T_NUMBER) {
		      yyerror("bad type for unary + (%s)", c_typename($$->mod));
		  }
		}
	| '!' cast_exp
		{ $$ = c_not($2); }
	| '~' cast_exp
		{
		  switch ($2->type) {
		  case N_INT:
		      $$ = $2;
		      $$->l.number = ~$$->l.number;
		      break;

		  case N_NEG:
		      if ($$->mod == T_NUMBER) {
			  $$ = $2->l.left;
			  break;
		      }
		  default:
		      $$ = tc_moni(N_NEG, $2, "~");
		      break;
		  }
		}
	;

cast_exp
	: prefix_exp
	| '(' type_specifier star_list ')' prefix_exp
		{
		  unsigned short type;

		  type = $2 | ($3 & T_REF);
		  if (typechecking && type != $5->mod) {
		      if (($5->mod & T_TYPE) != T_MIXED) {
			  yyerror("cast of non-mixed type (%s)",
				  c_typename($5->mod));
		      }
		      $$ = node_mon(N_CAST, type, $5);
		  } else {
		      $$ = $5;	/* ignore cast */
		  }
		}
	;

mult_oper_exp
	: cast_exp
	| mult_oper_exp '*' cast_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number *= $3->l.number;
		      } else if ($$->type == N_MULT &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number *= $3->l.number;
		      } else {
			  $$ = tc_bini(N_MULT, $$, $3, "*");
		      }
		  } else if ($$->type == N_INT) {
		      $$ = tc_bini(N_MULT, $3, $$, "*");
		  } else {
		      $$ = tc_bini(N_MULT, $$, $3, "*");
		  }
		}
	| mult_oper_exp '/' cast_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  if ($3->l.number == 0) {
			      yyerror("division by zero");
			  } else {
			      $$->l.number /= $3->l.number;
			  }
		      } else if ($$->type == N_DIV &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number *= $3->l.number;
		      } else {
			  $$ = tc_bini(N_DIV, $$, $3, "/");
		      }
		  } else {
		      $$ = tc_bini(N_DIV, $$, $3, "/");
		  }
		}
	| mult_oper_exp '%' cast_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  if ($3->l.number == 0) {
			      yyerror("modulus by zero");
			  } else {
			      $$->l.number %= $3->l.number;
			  }
		      } else if ($$->type == N_MOD &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number *= $3->l.number;
		      } else {
			  $$ = tc_bini(N_MOD, $$, $3, "%");
		      }
		  } else {
		      $$ = tc_bini(N_MOD, $$, $3, "%");
		  }
		}
	;

add_oper_exp
	: mult_oper_exp
	| add_oper_exp '+' mult_oper_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number += $3->l.number;
		      } else if ($$->type == N_STR) {
			  char buffer[12];

			  sprintf(buffer, "%ld", (long) (Int) $3->l.number);
			  $$ = node_str(str_new((char *) NULL,
						$$->l.string->len +
						(long) strlen(buffer)));
			  strcpy($$->l.string->text, $$->l.string->text);
			  strcat($$->l.string->text, buffer);
		      } else if ($$->type == N_MIN &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number -= $3->l.number;
		      } else if ($$->type == N_ADD &&
				 $$->r.right->type == N_STR) {
			  char buffer[12];
			  register string *str;

			  sprintf(buffer, "%ld", (long) (Int) $3->l.number);
			  str = str_new((char *) NULL,
					$$->r.right->l.string->len +
					(long) strlen(buffer));
			  strcpy(str->text, $$->r.right->l.string->text);
			  strcat(str->text, buffer);
			  $$->r.right = node_str(str);
		      } else if ($$->mod == T_NUMBER) {
			  if ($$->type == N_ADD && $$->r.right->type == N_INT) {
			      $$->r.right->l.number += $3->l.number;
			  } else {
			      $$ = tc_add(N_ADD, $3, $$, "+");
			  }
		      } else {
			  $$ = tc_add(N_ADD, $$, $3, "+");
		      }
		  } else if ($3->type == N_STR) {
		      if ($$->type == N_INT) {
			  char buffer[12];

			  sprintf(buffer, "%ld", (long) (Int) $$->l.number);
			  $$ = node_str(str_new((char *) NULL,
						(long) strlen(buffer) +
						$3->l.string->len));
			  strcpy($$->l.string->text, buffer);
			  strcat($$->l.string->text, $3->l.string->text);
		      } else if ($$->type == N_STR) {
			  if (ec_push()) {
			      yyerror("string constant too long");
			  } else {
			     $$ = node_str(str_add($$->l.string, $3->l.string));
			     ec_pop();
			  }
		      } else if ($$->type == N_ADD && $$->mod == T_STRING &&
				 $$->r.right->type == N_INT) {
			  char buffer[12];
			  register string *str;

			  sprintf(buffer, "%ld",
				  (long) (Int) $$->r.right->l.number);
			  str = str_new((char *) NULL, (long) strlen(buffer) +
						       $3->l.string->len);
			  strcpy(str->text, buffer);
			  strcat(str->text, $3->l.string->text);
			  $$->r.right = node_str(str);
		      } else if ($$->type == N_ADD &&
				 $$->r.right->type == N_STR) {
			  if (ec_push()) {
			      yyerror("string constant too long");
			  } else {
			   $$->r.right = node_str(str_add($$->r.right->l.string,
							  $3->l.string));
			   ec_pop();
			  }
		      } else {
			  $$ = tc_add(N_ADD, $$, $3, "+");
		      }
		  } else if ($$->mod == T_NUMBER && $$->r.right->type == N_INT)
		  {
		      $$ = tc_add(N_ADD, $3, $$, "+");
		  } else {
		      $$ = tc_add(N_ADD, $$, $3, "+");
		  }
		}
	| add_oper_exp '-' mult_oper_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number -= $3->l.number;
		      } else if ($$->type == N_MIN &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number += $3->l.number;
		      } else if ($$->type == N_ADD && $$->mod == T_NUMBER &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number -= $3->l.number;
		      } else {
			  $$ = tc_min(N_MIN, $$, $3, "-");
		      }
		  } else {
		      $$ = tc_min(N_MIN, $$, $3, "-");
		  }
		}
	;

shift_oper_exp
	: add_oper_exp
	| shift_oper_exp LSHIFT add_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number <<= $3->l.number;
		  } else {
		      $$ = tc_bini(N_LSHIFT, $$, $3, "<<");
		  }
		}
	| shift_oper_exp RSHIFT add_oper_exp
		{
		  $$ = $1;
		  if ($$->type == N_INT && $3->type == N_INT) {
		      $$->l.number >>= $3->l.number;
		  } else {
		      $$ = tc_bini(N_RSHIFT, $$, $3, ">>");
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
		      $$ = tc_rel(N_LT, $$, $3, "<");
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
		      $$ = tc_rel(N_GT, $$, $3, ">");
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
		      $$ = tc_rel(N_LE, $$, $3, "<=");
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
		      $$ = tc_rel(N_GE, $$, $3, ">=");
		  }
		}
	;

equ_oper_exp
	: rel_oper_exp
	| equ_oper_exp EQ rel_oper_exp
		{
		  $$ = (node *) NULL;
		  switch ($1->type) {
		  case N_INT:
		      if ($3->type == N_INT) {
			  $$ = $1;
			  $$->l.number = ($$->l.number == $3->l.number);
		      } else if ($1->l.number == 0) {
			  if ($3->type == N_STR) {
			      $$ = $1;	/* FALSE */
			  } else {
			      $$ = c_not($3);
			  }
		      }
		      break;

		  case N_STR:
		      if ($3->type == N_STR) {
			  $$ = node_int((Int) (strcmp($1->l.string->text,
						     $3->l.string->text) == 0));
		      } else if ($3->type == N_INT && $3->l.number == 0) {
			  $$ = $3;	/* FALSE */
		      }
		      break;

		  default:
		      if ($3->type == N_INT && $3->l.number == 0) {
			  $$ = c_not($1);
		      }
		      break;
		  }
		  if ($$ == (node *) NULL) {
		      $$ = tc_equ(N_EQ, $1, $3, "==");
		  }
		}
	| equ_oper_exp NE rel_oper_exp
		{
		  $$ = (node *) NULL;
		  switch ($1->type) {
		  case N_INT:
		      if ($3->type == N_INT) {
			  $$ = $1;
			  $$->l.number = ($$->l.number != $3->l.number);
		      } else if ($1->l.number == 0) {
			  if ($3->type == N_STR) {
			      $$ = $1;
			      $$->l.number = TRUE;
			  } else {
			      $$ = c_tst($3);
			  }
		      }
		      break;

		  case N_STR:
		      if ($3->type == N_STR) {
			  $$ = node_int((Int) (strcmp($1->l.string->text,
						     $3->l.string->text) != 0));
		      } else if ($3->type == N_INT && $3->l.number == 0) {
			  $$ = $3;
			  $$->l.number = TRUE;
		      }
		      break;

		  default:
		      if ($3->type == N_INT && $3->l.number == 0) {
			  $$ = c_tst($1);
		      }
		      break;
		  }
		  if ($$ == (node *) NULL) {
		      $$ = tc_equ(N_NE, $1, $3, "!=");
		  }
		}
	;

bitand_oper_exp
	: equ_oper_exp
	| bitand_oper_exp '&' equ_oper_exp
		{
		  unsigned short type;

		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number &= $3->l.number;
		      } else if ($$->type == N_AND &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number &= $3->l.number;
		      } else {
			  $$ = tc_bini(N_AND, $$, $3, "&");
		      }
		  } else if ($$->type == N_INT) {
		      $$ = tc_bini(N_AND, $3, $$, "&");
		  } else if ((($$->type | $3->type) & T_REF) != 0 &&
			     (type=c_tmatch($$->type, $3->type)) != T_ERROR) {
		      /*
		       * array & array
		       */
		      $$ = node_bin(N_AND, type, $$, $3);
		  } else {
		      $$ = tc_bini(N_AND, $$, $3, "&");
		  }
		}
	;

bitxor_oper_exp
	: bitand_oper_exp
	| bitxor_oper_exp '^' bitand_oper_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number ^= $3->l.number;
		      } else if ($$->type == N_XOR &&
				 $$->r.right->type == N_INT) {
			  $$->r.right->l.number ^= $3->l.number;
		      } else {
			  $$ = tc_bini(N_XOR, $$, $3, "^");
		      }
		  } else if ($$->type == N_INT) {
		      $$ = tc_bini(N_XOR, $3, $$, "^");
		  } else {
		      $$ = tc_bini(N_XOR, $$, $3, "^");
		  }
		}
	;

bitor_oper_exp
	: bitxor_oper_exp
	| bitor_oper_exp '|' bitxor_oper_exp
		{
		  $$ = $1;
		  if ($3->type == N_INT) {
		      if ($$->type == N_INT) {
			  $$->l.number |= $3->l.number;
		      } else if ($$->type == N_OR &&
				 $$->r.right->type == N_INT) {
			  $$ = $$;
			  $$->r.right->l.number |= $3->l.number;
		      } else {
			  $$ = tc_bini(N_OR, $$, $3, "|");
		      }
		  } else if ($$->type == N_INT) {
		      $$ = tc_bini(N_OR, $3, $$, "|");
		  } else {
		      $$ = tc_bini(N_OR, $$, $3, "|");
		  }
		}
	;

and_oper_exp
	: bitor_oper_exp
	| and_oper_exp LAND bitor_oper_exp
		{
		  if ($1->type == N_TST) {
		      $1 = $1->l.left;
		  }
		  if ($3->type == N_TST) {
		      $3 = $3->l.left;
		  }
		  if ($1->type == N_STR) {
		      $$ = c_tst($3);
		  } else if ($3->type == N_STR) {
		      $$ = c_tst($1);
		  } else if ($1->type == N_INT) {
		      if ($1->l.number == 0) {
			  $$ = $1;
		      } else {
			  $$ = c_tst($3);
		      }
		  } else if ($3->type == N_INT) {
		      if ($3->l.number != 0) {
			  $$ = c_tst($1);
		      } else {
			  $$ = node_bin(N_COMMA, T_NUMBER, $1, $3);
		      } 
		  } else {
		      $$ = node_bin(N_LAND, T_NUMBER, $1, $3);
		  }
		}
	;

or_oper_exp
	: and_oper_exp
	| or_oper_exp LOR and_oper_exp
		{
		  if ($1->type == N_TST) {
		      $1 = $1->l.left;
		  }
		  if ($3->type == N_TST) {
		      $3 = $3->l.left;
		  }
		  if ($1->type == N_STR) {
		      $$ = node_int((Int) TRUE);
		  } else if ($3->type == N_STR) {
		      $$ = c_tst($1);
		  } else if ($1->type == N_INT) {
		      if ($1->l.number != 0) {
			  $$ = $1;
			  $$->l.number = TRUE;
		      } else {
			  $$ = c_tst($3);
		      }
		  } else if ($3->type == N_INT) {
		      if ($3->l.number == 0) {
			  $$ = c_tst($1);
		      } else {
			  $$ = node_bin(N_COMMA, T_NUMBER, $1, $3);
			  $3->l.number = TRUE;
		      }
		  } else {
		      $$ = node_bin(N_LAND, T_NUMBER, $1, $3);
		  }
		}
	;

cond_exp
	: or_oper_exp
	| or_oper_exp '?' f_list_exp ':' cond_exp
		{ $$ = tc_quest($1, $3, $5); }
	;

exp
	: cond_exp
	| cond_exp '=' exp
		{ $$ = tc_assign(c_lvalue($1, "assignment"), $3); }
	| cond_exp PLUS_EQ exp
		{ $$ = tc_add(N_ADD_EQ, c_lvalue($1, "+="), $3, "+="); }
	| cond_exp MIN_EQ exp
		{ $$ = tc_min(N_MIN_EQ, c_lvalue($1, "-="), $3, "-="); }
	| cond_exp MULT_EQ exp
		{ $$ = tc_bini(N_MULT_EQ, c_lvalue($1, "*="), $3, "*="); }
	| cond_exp DIV_EQ exp
		{ $$ = tc_bini(N_DIV_EQ, c_lvalue($1, "/="), $3, "/="); }
	| cond_exp MOD_EQ exp
		{ $$ = tc_bini(N_MOD_EQ, c_lvalue($1, "%="), $3, "%="); }
	| cond_exp RSHIFT_EQ exp
		{ $$ = tc_bini(N_RSHIFT_EQ, c_lvalue($1, ">>="), $3, ">>="); }
	| cond_exp LSHIFT_EQ exp
		{ $$ = tc_bini(N_LSHIFT_EQ, c_lvalue($1, "<<="), $3, "<<="); }
	| cond_exp AND_EQ exp
		{ $$ = tc_bini(N_AND_EQ, c_lvalue($1, "&="), $3, "&="); }
	| cond_exp XOR_EQ exp
		{ $$ = tc_bini(N_XOR_EQ, c_lvalue($1, "^="), $3, "^="); }
	| cond_exp OR_EQ exp
		{ $$ = tc_bini(N_OR_EQ, c_lvalue($1, "|="), $3, "|="); }
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

opt_list_exp_comma
	: opt_list_exp
	| list_exp ','
		{ $$ = $1; }
	;

f_list_exp
	: list_exp
		{ c_list_exp($$ = $1); }

assoc_exp
	: exp ':' exp
		{ $$ = node_bin(N_PAIR, 0, $1, $3); }
	;

list_assoc_exp
	: assoc_exp
	| list_assoc_exp ',' assoc_exp
		{ $$ = node_bin(N_COMMA, 0, $1, $3); }
	;

opt_list_assoc_exp_comma
	: /* empty */
		{ $$ = (node *) NULL; }
	| list_assoc_exp
	| list_assoc_exp ','
		{ $$ = $1; }
	;

ident
	: IDENTIFIER
		{ $$ = node_str(str_new($1, (long) strlen($1))); }
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

    sprintf(buf, "\"%s\", %u: ", tk_filename(), tk_line());
    sprintf(buf + strlen(buf), f, a1, a2, a3);
    warning("%s", buf);
    nerrors++;
}

/*
 * NAME:	tc_index()
 * DESCRIPTION:	create a proper node for the [ ] operator
 */
static node *tc_index(n1, n2)
register node *n1, *n2;
{
    register unsigned short type;

    type = T_MIXED;
    if (typechecking) {
	if ((n1->mod & T_REF)) {
	    /*
	     * array
	     */
	    if (n2->mod != T_NUMBER && n2->mod != T_MIXED) {
		yyerror("bad array index type (%s)", c_typename(n2->mod));
		return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	    }
	    type = n1->mod - (1 << REFSHIFT);
	} else if (n1->mod == T_STRING) {
	    /*
	     * string
	     */
	    if (n2->mod != T_NUMBER && n2->mod != T_MIXED) {
		yyerror("bad string index type (%s)", c_typename(n2->mod));
		return node_mon(N_FAKE, T_NUMBER, (node *) NULL);
	    }
	    type = T_NUMBER;
	} else if (n1->mod != T_MAPPING && n1->mod != T_MIXED) {
	    yyerror("bad indexed type (%s)", c_typename(n1->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	}
    }
    return node_bin(N_INDEX, type, n1, n2);
}

/*
 * NAME:	tc_range()
 * DESCRIPTION:	create a proper node for the [ .. ] operator
 */
static node *tc_range(n1, n2, n3)
register node *n1, *n2, *n3;
{
    if (typechecking) {
	/* check index 1 */
	if (n2->mod != T_NUMBER && n2->mod != T_MIXED) {
	    yyerror("bad range type (%s)", c_typename(n2->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	}
	/* check index 2 */
	if (n3->mod != T_NUMBER && n3->mod != T_MIXED) {
	    yyerror("bad range type (%s)", c_typename(n3->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	}
	if (!(n1->mod & T_REF) && n1->mod != T_STRING && n1->mod != T_MIXED) {
	    yyerror("bad indexed type (%s)", c_typename(n1->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	}
    }

    return node_bin(N_RANGE, n1->mod, n1, node_bin(N_PAIR, 0, n2, n3));
}

/*
 * NAME:	tc_moni()
 * DESCRIPTION:	create a proper node for a monadic int operator
 */
static node *tc_moni(oper, n, op)
int oper;
register node *n;
char *op;
{
    if (typechecking) {
	if (n->mod == T_NUMBER || n->mod == T_MIXED) {
	    return node_mon(oper, T_NUMBER, n);
	} else {
	    yyerror("bad type for %s (%s)", op, c_typename(n->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	}
    } else {
	/*
	 * untypechecked
	 */
	return node_mon(oper, T_MIXED, n);
    }
}

/*
 * NAME:	tc_bini()
 * DESCRIPTION:	create a proper node for a binary int operator
 */
static node *tc_bini(oper, n1, n2, op)
int oper;
node *n1, *n2;
char *op;
{
    if (typechecking) {
	register node *n, *m;

	/* check left expression */
	n = n1;
	if (n->mod != T_NUMBER && n->mod != T_MIXED) {
	    yyerror("bad types for %s (%s, %s)", op, c_typename(n1->mod),
		    c_typename(n2->mod));
	}
	/* check right expression */
	m = n2;
	if (m->mod != T_NUMBER && m->mod != T_MIXED) {
	    yyerror("bad types for %s (%s, %s)", op, c_typename(n1->mod),
		    c_typename(n2->mod));
	}
	return node_bin(oper, T_NUMBER, n, m);
    } else {
	/*
	 * untypechecked
	 */
	return node_bin(oper, T_MIXED, n1, n2);
    }
}

/*
 * NAME:	tc_add()
 * DESCRIPTION:	create a proper node for the + or += operator
 */
static node *tc_add(oper, n1, n2, op)
int oper;
register node *n1, *n2;
char *op;
{
    if (typechecking) {
	register unsigned short type;

	type = c_tmatch(n1->mod, n2->mod);
	if (type == T_OBJECT ||	/* object + object not allowed */
	    (type == T_ERROR &&	/* only if not adding int to string */
	     ((n1->mod != T_NUMBER && n1->mod != T_STRING) ||
	      (n2->mod != T_NUMBER && n2->mod != T_STRING) ||
	      (n1->mod == T_NUMBER && oper == N_ADD_EQ)))) {
	    yyerror("bad types for %s (%s, %s)", op, c_typename(n1->mod),
		    c_typename(n2->mod));
	    return node_mon(N_FAKE, T_MIXED, (node *) NULL);
	} else if (n1->mod == T_NUMBER) {
	    return node_bin(oper, n2->mod, n1, n2);
	} else if (n1->mod == T_STRING) {
	    return node_bin(oper, T_STRING, n1, n2);
	} else {
	    return node_bin(oper, type, n1, n2);
	}
    } else {
	/*
	 * untypechecked
	 */
	return node_bin(oper, T_MIXED, n1, n2);
    }
}

/*
 * NAME:	tc_min()
 * DESCRIPTION:	create a proper node for the - or -= operator
 */
static node *tc_min(oper, n1, n2, op)
int oper;
register node *n1, *n2;
char *op;
{
    if (typechecking) {
	register unsigned short type;

	type = c_tmatch(n1->mod, n2->mod);
	if (type == T_STRING || type == T_OBJECT || type == T_ERROR) {
	    yyerror("bad types for %s (%s, %s)", op, c_typename(n1->mod),
		    c_typename(n2->mod));
	    return node_mon(N_FAKE, type, (node *) NULL);
	} else {
	    return node_bin(oper, type, n1, n2);
	}
    } else {
	/*
	 * untypechecked
	 */
	return node_bin(oper, T_MIXED, n1, n2);
    }
}

/*
 * NAME:	tc_rel()
 * DESCRIPTION:	create a proper node for the < > <= >= operators
 */
static node *tc_rel(oper, n1, n2, op)
int oper;
register node *n1, *n2;
char *op;
{
    if (typechecking) {
	register unsigned short type;

	type = c_tmatch(n1->mod, n2->mod);
	if (type != T_NUMBER && type != T_STRING && type != T_MIXED) {
	    yyerror("bad types for %s (%s, %s)", op, c_typename(n1->mod),
		    c_typename(n2->mod));
	    return node_mon(N_FAKE, T_NUMBER, (node *) NULL);
	}
    }
    return node_bin(oper, T_NUMBER, n1, n2);
}

/*
 * NAME:	tc_equ()
 * DESCRIPTION:	create a proper node for the == != operators
 */
static node *tc_equ(oper, n1, n2, op)
int oper;
register node *n1, *n2;
char *op;
{
    if (typechecking && n1->mod != n2->mod &&
	(n1->type != N_INT || n1->l.number != 0) &&
	(n2->type != N_INT || n2->l.number != 0)) {
	yyerror("incompatible types for %s (%s, %s)", op,
		c_typename(n1->mod), c_typename(n2->mod));
    }
    return node_bin(oper, T_NUMBER, n1, n2);
}

/*
 * NAME:	tc_quest()
 * DESCRIPTION:	create a proper node for the ? : operator
 */
static node *tc_quest(n1, n2, n3)
register node *n1, *n2, *n3;
{
    register unsigned short type;

    if (n2->type == N_INT && n2->l.number == 0) {
	/*
	 * expr ? 0 : expr
	 */
	type = n3->mod;
    } else if (n3->type == N_INT && n3->l.number == 0) {
	/*
	 * expr ? expr : 0;
	 */
	type = n2->mod;
    } else if (typechecking) {
	/*
	 * typechecked
	 */
	type = c_tmatch(n2->mod, n3->mod);
	if (type == T_ERROR) {
	    yyerror("incompatible types for ? : (%s, %s)", c_typename(n2->mod),
		    c_typename(n3->mod));
	    type = T_MIXED;
	}
    } else {
	/*
	 * untypechecked
	 */
	type = T_MIXED;
    }
    if (n1->type == N_INT) {
	return (n1->l.number != 0) ? n2 : n3;
    } else if (n1->type == N_STR) {
	return n2;
    }
    return node_bin(N_QUEST, type, n1, node_bin(N_PAIR, 0, n2, n3));
}

/*
 * NAME:	tc_assign()
 * DESCRIPTION:	create a proper node for the assignment operator
 */
static node *tc_assign(n1, n2)
register node *n1, *n2;
{
    register unsigned short type;

    if (n2->type == N_INT && n2->l.number == 0) {
	/*
	 * can assign constant 0 to anything anytime
	 */
	type = T_NUMBER;
    } else if (typechecking) {
	/*
	 * typechecked
	 */
	type = c_tmatch(n1->mod, n2->mod);
	if (type == T_ERROR) {
	    yyerror("incompatible types for = (%s, %s)", c_typename(n1->mod),
		    c_typename(n2->mod));
	    return node_bin(N_FAKE, T_MIXED, n1, n2);
	} else if (n2->mod == T_MIXED) {
	    if (n1->mod & T_REF) {
		n2 = node_mon(N_CAST, T_ARRAY, n2);
	    } else if (n1->mod != T_MIXED) {
		n2 = node_mon(N_CAST, n1->mod, n2);
	    }
	}
    } else {
	/*
	 * untypechecked
	 */
	type = T_MIXED;
    }
    return node_bin(N_ASSIGN, type, n1, n2);
}
