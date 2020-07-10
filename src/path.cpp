/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2020 DGD Authors (see the commit log for details)
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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "path.h"
# include "node.h"
# include "compile.h"

/*
 * resolve a path
 */
char *Path::resolve(char *buf, char *file)
{
    char *p, *q, *d;

    strncpy(buf, file, STRINGSZ - 1);
    buf[STRINGSZ - 1] = '\0';
    d = p = q = buf;
    for (;;) {
	if (*p == '/' || *p == '\0') {
	    /* reached a directory separator */
	    if (q - 1 == d && d[0] == '.') {
		/* . */
		q = d;
	    } else if (q - 2 == d && d[0] == '.' && d[1] == '.') {
		/* .. */
		q = d;
		if (q != buf) {
		    for (--q; q != buf && *--q != '/'; ) ;
		}
	    }
	    if (q != buf) {
		if (q[-1] == '/') {
		    /* // or path/ */
		    --q;
		}
		*q++ = *p;
	    }
	    d = q;
	    if (*p == '\0') {
		break;
	    }
	    p++;
	} else {
	    *q++ = *p++;
	}
    }

    if (q == buf) {
	/* "" -> "." */
	*q++ = '.';
	*q = '\0';
    }
    return buf;
}

/*
 * check and resolve a string path
 */
char *Path::string(char *buf, char *file, unsigned int len)
{
    if (len >= STRINGSZ || strlen(file) != len) {
	return (char *) NULL;
    }
    return resolve(buf, file);
}

/*
 * resolve a (possibly relative) path
 */
char *Path::from(char *buf, char *from, char *file)
{
    char buf2[STRINGSZ];

    if (file[0] != '/' && strlen(from) + strlen(file) < STRINGSZ - 4) {
	sprintf(buf2, "%s/../%s", from, file);
	file = buf2;
    }
    return resolve(buf, file);
}

/*
 * resolve an editor read file path
 */
char *Path::edRead(char *buf, char *file)
{
    Frame *f;

    f = cframe;
    if (OBJR(f->oindex)->flags & O_DRIVER) {
	return resolve(buf, file);
    } else {
	PUSH_STRVAL(f, String::create(file, strlen(file)));
	DGD::callDriver(f, "path_read", 1);
	if (f->sp->type != T_STRING) {
	    (f->sp++)->del();
	    return (char *) NULL;
	}
	resolve(buf, f->sp->string->text);
	(f->sp++)->string->del();
	return buf;
    }
}

/*
 * resolve an editor write file path
 */
char *Path::edWrite(char *buf, char *file)
{
    Frame *f;

    f = cframe;
    if (OBJR(f->oindex)->flags & O_DRIVER) {
	return resolve(buf, file);
    } else {
	PUSH_STRVAL(f, String::create(file, strlen(file)));
	DGD::callDriver(f, "path_write", 1);
	if (f->sp->type != T_STRING) {
	    (f->sp++)->del();
	    return (char *) NULL;
	}
	resolve(buf, f->sp->string->text);
	(f->sp++)->string->del();
	return buf;
    }
}

/*
 * resolve an include path
 */
char *Path::include(char *buf, char *from, char *file, String ***strs,
		    int *nstr)
{
    Frame *f;
    int i;
    Value *v;
    String **str;

    *strs = NULL;
    *nstr = 0;
    if (Compile::autodriver()) {
	return Path::from(buf, from, file);
    }

    f = cframe;
    PUSH_STRVAL(f, String::create(from, strlen(from)));
    PUSH_STRVAL(f, String::create(file, strlen(file)));
    if (!DGD::callDriver(f, "include_file", 2)) {
	f->sp++;
	return Path::from(buf, from, file);
    }

    if (f->sp->type == T_STRING) {
	/* simple path */
	resolve(buf, f->sp->string->text);
	(f->sp++)->string->del();
	return buf;
    } else if (f->sp->type == T_ARRAY) {
	/*
	 * Array of strings.  Check that the array does indeed contain only
	 * strings, then return it.
	 */
	i = f->sp->array->size;
	if (i != 0) {
	    v = Dataspace::elts(f->sp->array);
	    while ((v++)->type == T_STRING) {
		if (--i == 0) {
		    *nstr = i = f->sp->array->size;
		    str = ALLOC(String*, i);
		    do {
			*str = (--v)->string;
			(*str++)->ref();
		    } while (--i != 0);
		    *strs = str;
		    (f->sp++)->array->del();

		    /* return the untranslated path, as well */
		    return Path::from(buf, from, file);
		}
	    }
	}
    }

    (f->sp++)->del();
    return (char *) NULL;
}
