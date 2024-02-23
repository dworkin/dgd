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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "data.h"
# include "interpret.h"
# include "path.h"
# include "ppcontrol.h"
# include "node.h"
# include "compile.h"

static PathImpl PMI;
Path *PM = &PMI;

/*
 * resolve an editor read file path
 */
char *PathImpl::edRead(char *buf, char *file)
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
char *PathImpl::edWrite(char *buf, char *file)
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
 * attempt to include a path
 */
char *PathImpl::include(char *buf, char *from, char *file)
{
    Frame *f;
    int i;
    Value *v;
    String *str;

    if (Compile::autodriver()) {
	if (PP->include(PathImpl::from(buf, from, file), (char *) NULL, 0)) {
	    return buf;
	}
    } else {
	f = cframe;
	PUSH_STRVAL(f, String::create(from, strlen(from)));
	PUSH_STRVAL(f, String::create(file, strlen(file)));
	if (!DGD::callDriver(f, "include_file", 2)) {
	    if (PP->include(PathImpl::from(buf, from, file), (char *) NULL, 0))
	    {
		return buf;
	    }
	} else if (f->sp->type == T_STRING) {
	    /* simple path */
	    resolve(buf, f->sp->string->text);
	    if (PP->include(buf, (char *) NULL, 0)) {
		(f->sp++)->string->del();
		return buf;
	    }
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
			PathImpl::from(buf, from, file);

			i = f->sp->array->size;
			str = (--v)->string;
			PP->include(buf, str->text, str->len);

			while (--i != 0) {
			    str = (--v)->string;
			    PP->push(str->text, str->len);
			}
			(f->sp++)->del();

			/* return the untranslated path, as well */
			return buf;
		    }
		}
	    }
	}

	(f->sp++)->del();
    }

    return (char *) NULL;
}
