# define INCLUDE_CTYPE
# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "grammar.h"
# include "dfa.h"
# include "pda.h"
# include "parse.h"

typedef struct _parser_ {
    string *source;		/* grammar source */
    string *grammar;		/* preprocessed grammar */
    dfa *dfa;			/* (partial) DFA */
} parser;

/*
 * NAME:	parser->save()
 * DESCRIPTION:	save parse_string data
 */
void ps_save(data)
register dataspace *data;
{
    register parser *ps;
    register value *v;
    value val;
    string *s1, *s2;

    ps = data->parser;
    if (dfa_save(ps->dfa, &s1, &s2)) {
	val.type = T_ARRAY;
	val.u.array = arr_new(data, 4L);
	v = val.u.array->elts;
	v->type = T_STRING;
	str_ref((v++)->u.string = ps->source);
	v->type = T_STRING;
	str_ref((v++)->u.string = ps->grammar);
	v->type = T_STRING;
	str_ref((v++)->u.string = s1);
	if (s2 != (string *) NULL) {
	    v->type = T_STRING;
	    str_ref(v->u.string = s2);
	} else {
	    v->type = T_INT;
	    v->u.number = 0;
	}
	d_assign_var(data, d_get_variable(data, data->nvariables - 1), &val);
    }
}

/*
 * NAME:	parser->free()
 * DESCRIPTION:	free parse_string data
 */
void ps_free(data)
register dataspace *data;
{
    dfa_del(data->parser->dfa);
    str_del(data->parser->source);
    str_del(data->parser->grammar);
    FREE(data->parser);
}

/*
 * NAME:	parse_string()
 * DESCRIPTION:	parse a string
 */
array *ps_parse_string(data, source, str)
register dataspace *data;
string *source;
string *str;
{
    register parser *ps;
    string *grammar;
    bool same;

    if (data->parser != (parser *) NULL) {
	ps = data->parser;
	same = (str_cmp(ps->source, source) == 0);
    } else {
	value *val;

	val = d_get_variable(data, data->nvariables - 1);
	if (val->type == T_ARRAY &&
	    str_cmp(d_get_elts(val->u.array)->u.string, source) == 0) {
	    ps = data->parser = ALLOC(parser, 1);
	    val = d_get_elts(val->u.array);
	    str_ref(ps->source = val[0].u.string);
	    str_ref(ps->grammar = val[1].u.string);
	    ps->dfa = dfa_load(ps->grammar->text, val[2].u.string,
			       val[3].u.string);
	    same = TRUE;
	} else {
	    ps = (parser *) NULL;
	    same = FALSE;
	}
    }

    if (!same) {
	grammar = parse_grammar(source);

	if (data->parser != (parser *) NULL) {
	    ps_free(data);
	}
	data->parser = ps = ALLOC(parser, 1);
	str_ref(ps->source = source);
	str_ref(ps->grammar = grammar);
	ps->dfa = dfa_new(grammar->text);
    }

    {
	int tokens[10000];
	unsigned int size;
	int i, n;
	array *a;

	size = str->len;
	for (i = 0;; i++) {
	    n = dfa_lazyscan(ps->dfa, str, &size);
	    if (n < 0) {
		if (n == -2) {
		    error("Invalid token");
		}
		break;
	    }
	    tokens[i] = n;
	}

	a = arr_new(data, (long) i);
	for (n = 0; n < i; n++) {
	    a->elts[n].type = T_INT;
	    a->elts[n].u.number = tokens[n];
	}

	return a;
    }
}
