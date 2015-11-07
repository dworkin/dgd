/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "macro.h"
# include "token.h"
# include "node.h"

# define NODE_CHUNK	128

static class nodechunk : public Chunk<node, NODE_CHUNK> {
public:
    /*
     * NAME:		item()
     * DESCRIPTION:	dereference strings when iterating through items
     */
    virtual bool item(node *n) {
	if (n->type == N_STR || n->type == N_GOTO || n->type == N_LABEL) {
	    str_del(n->l.string);
	} else if (n->type == N_TYPE && n->sclass != (String *) NULL) {
	    str_del(n->sclass);
	}
	return TRUE;
    }
} nchunk;

int nil_node;				/* N_NIL or N_INT */

/*
 * NAME:	node->init()
 * DESCRIPTION:	initialize node handling
 */
void node_init(int flag)
{
    nil_node = (flag) ? N_NIL : N_INT;
}

/*
 * NAME:	node->new()
 * DESCRIPTION:	create a new node
 */
node *node_new(unsigned int line)
{
    node *n;

    n = nchunk.alloc();
    n->type = N_INT;
    n->flags = 0;
    n->mod = 0;
    n->line = line;
    n->sclass = (String *) NULL;
    n->l.left = (node *) NULL;
    n->r.right = (node *) NULL;
    return n;
}

/*
 * NAME:	node->int()
 * DESCRIPTION:	create an integer node
 */
node *node_int(Int num)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_INT;
    n->flags = F_CONST;
    n->mod = T_INT;
    n->l.number = num;

    return n;
}

/*
 * NAME:	node->float()
 * DESCRIPTION:	create a float node
 */
node *node_float(xfloat *flt)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_FLOAT;
    n->flags = F_CONST;
    n->mod = T_FLOAT;
    NFLT_PUT(n, *flt);

    return n;
}

/*
 * NAME:	node->nil()
 * DESCRIPTION:	create a nil node
 */
node *node_nil()
{
    node *n;

    n = node_new(tk_line());
    n->type = nil_node;
    n->flags = F_CONST;
    n->mod = nil_type;
    n->l.number = 0;

    return n;
}

/*
 * NAME:	node->str()
 * DESCRIPTION:	create a string node
 */
node *node_str(String *str)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_STR;
    n->flags = F_CONST;
    n->mod = T_STRING;
    str_ref(n->l.string = str);
    n->r.right = (node *) NULL;

    return n;
}

/*
 * NAME:	node->var()
 * DESCRIPTION:	create a variable type node
 */
node *node_var(unsigned int type, int idx)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_VAR;
    n->mod = type;
    n->l.number = idx;

    return n;
}

/*
 * NAME:	node->type()
 * DESCRIPTION:	create a type node
 */
node *node_type(int type, String *tclass)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_TYPE;
    n->mod = type;
    n->sclass = tclass;
    if (tclass != (String *) NULL) {
	str_ref(tclass);
    }

    return n;
}

/*
 * NAME:	node->fcall()
 * DESCRIPTION:	create a function call node
 */
node *node_fcall(int mod, String *tclass, char *func, Int call)
{
    node *n;

    n = node_new(tk_line());
    n->type = N_FUNC;
    n->mod = mod;
    n->sclass = tclass;
    n->l.ptr = func;
    n->r.number = call;

    return n;
}

/*
 * NAME:	node->op()
 * DESCRIPTION:	create an operator node
 */
node *node_op(const char *op)
{
    return node_str(str_new(op, strlen(op)));
}

/*
 * NAME:	node->mon()
 * DESCRIPTION:	create a node for a monadic operator
 */
node *node_mon(int type, int mod, node *left)
{
    node *n;

    n = node_new(tk_line());
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = (node *) NULL;

    return n;
}

/*
 * NAME:	node->bin()
 * DESCRIPTION:	create a node for a binary operator
 */
node *node_bin(int type, int mod, node *left, node *right)
{
    node *n;

    n = node_new(tk_line());
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = right;

    return n;
}

/*
 * NAME:	node->toint()
 * DESCRIPTION:	convert node type to integer constant
 */
void node_toint(node *n, Int i)
{
    if (n->type == N_STR) {
	str_del(n->l.string);
    } else if (n->type == N_TYPE && n->sclass != (String *) NULL) {
	str_del(n->sclass);
    }
    n->type = N_INT;
    n->flags = F_CONST;
    n->l.number = i;
}

/*
 * NAME:	node->tostr()
 * DESCRIPTION:	convert node type to string constant
 */
void node_tostr(node *n, String *str)
{
    str_ref(str);
    if (n->type == N_STR) {
	str_del(n->l.string);
    } else if (n->type == N_TYPE && n->sclass != (String *) NULL) {
	str_del(n->sclass);
    }
    n->type = N_STR;
    n->flags = F_CONST;
    n->l.string = str;
}

/*
 * NAME:	node->clear()
 * DESCRIPTION:	cleanup after node handling
 */
void node_clear()
{
    nchunk.items();
    nchunk.clean();
}
