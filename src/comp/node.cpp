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

# include "comp.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "ppcontrol.h"
# include "node.h"

# define NODE_CHUNK	128

static class nodechunk : public Chunk<Node, NODE_CHUNK> {
public:
    /*
     * dereference strings when iterating through items
     */
    virtual bool item(Node *n) {
	if (n->type == N_STR || n->type == N_GOTO || n->type == N_LABEL) {
	    n->l.string->del();
	}
	if (n->sclass != (String *) NULL) {
	    n->sclass->del();
	}
	return TRUE;
    }
} nchunk;

int nil_node;				/* N_NIL or N_INT */

/*
 * initialize node handling
 */
void Node::init(int flag)
{
    nil_node = (flag) ? N_NIL : N_INT;
}

/*
 * constructor
 */
Node::Node(unsigned short line)
{
    type = N_INT;
    flags = 0;
    mod = 0;
    this->line = line;
    sclass = (String *) NULL;
    l.left = (Node *) NULL;
    r.right = (Node *) NULL;
}

/*
 * create a new node
 */
Node *Node::create(unsigned short line)
{
    return chunknew (nchunk) Node(line);
}

/*
 * create an integer node
 */
Node *Node::createInt(LPCint num)
{
    Node *n;

    n = create(PP->line());
    n->type = N_INT;
    n->flags = F_CONST;
    n->mod = T_INT;
    n->l.number = num;

    return n;
}

/*
 * create a float node
 */
Node *Node::createFloat(Float *flt)
{
    Node *n;

    n = create(PP->line());
    n->type = N_FLOAT;
    n->flags = F_CONST;
    n->mod = T_FLOAT;
    NFLT_PUT(n, *flt);

    return n;
}

/*
 * create a nil node
 */
Node *Node::createNil()
{
    Node *n;

    n = create(PP->line());
    n->type = nil_node;
    n->flags = F_CONST;
    n->mod = nil.type;
    n->l.number = 0;

    return n;
}

/*
 * create a string node
 */
Node *Node::createStr(String *str)
{
    Node *n;

    n = create(PP->line());
    n->type = N_STR;
    n->flags = F_CONST;
    n->mod = T_STRING;
    n->l.string = str;
    n->l.string->ref();
    n->r.right = (Node *) NULL;

    return n;
}

/*
 * create a variable type node
 */
Node *Node::createVar(unsigned int type, int idx)
{
    Node *n;

    n = create(PP->line());
    n->type = N_VAR;
    n->mod = type;
    n->l.number = idx;

    return n;
}

/*
 * create a type node
 */
Node *Node::createType(int type, String *tclass)
{
    Node *n;

    n = create(PP->line());
    n->type = N_TYPE;
    n->mod = type;
    n->sclass = tclass;
    if (tclass != (String *) NULL) {
	tclass->ref();
    }

    return n;
}

/*
 * create a function call node
 */
Node *Node::createFcall(int mod, String *tclass, char *func, LPCint call)
{
    Node *n;

    n = create(PP->line());
    n->type = N_FUNC;
    n->mod = mod;
    n->sclass = tclass;
    if (tclass != (String *) NULL) {
	tclass->ref();
    }
    n->l.ptr = func;
    n->r.number = call;

    return n;
}

/*
 * create an operator node
 */
Node *Node::createOp(const char *op)
{
    return createStr(String::create(op, strlen(op)));
}

/*
 * create a node for a monadic operator
 */
Node *Node::createMon(int type, int mod, Node *left)
{
    Node *n;

    n = create(PP->line());
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = (Node *) NULL;

    return n;
}

/*
 * create a node for a binary operator
 */
Node *Node::createBin(int type, int mod, Node *left, Node *right)
{
    Node *n;

    n = create(PP->line());
    n->type = type;
    n->mod = mod;
    n->l.left = left;
    n->r.right = right;

    return n;
}

/*
 * convert node type to integer constant
 */
void Node::toint(LPCint i)
{
    if (type == N_STR) {
	l.string->del();
    } else if (type == N_TYPE && sclass != (String *) NULL) {
	sclass->del();
    }
    type = N_INT;
    flags = F_CONST;
    l.number = i;
}

/*
 * convert node type to string constant
 */
void Node::tostr(String *str)
{
    str->ref();
    if (type == N_STR) {
	l.string->del();
    } else if (type == N_TYPE && sclass != (String *) NULL) {
	sclass->del();
    }
    type = N_STR;
    flags = F_CONST;
    l.string = str;
}

/*
 * revert a "linked list" of nodes
 */
Node *Node::revert(Node *n)
{
    Node *m;

    if (n != (Node *) NULL && n->type == N_PAIR) {
	while ((m=n->l.left)->type == N_PAIR) {
	    /*
	     * ((a, b), c) -> (a, (b, c))
	     */
	    n->l.left = m->r.right;
	    m->r.right = n;
	    n = m;
	}
    }
    return n;
}

/*
 * cleanup after node handling
 */
void Node::clear()
{
    nchunk.items();
    nchunk.clean();
}
