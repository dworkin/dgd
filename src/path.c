# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "path.h"
# include "node.h"
# include "compile.h"

/*
 * NAME:	path->resolve()
 * DESCRIPTION:	resolve a path
 */
char *path_resolve(file)
char *file;
{
    static char buf[STRINGSZ];
    register char *p, *q, *d;

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
 * NAME:	path->ed_read()
 * DESCRIPTION:	resolve an editor read file path
 */
char *path_ed_read(file)
char *file;
{
    if (i_this_object()->flags & O_DRIVER) {
	return path_file(path_resolve(file));
    } else {
	i_check_stack(1);
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new(file, (long) strlen(file)));
	call_driver_object("path_ed_read", 1);
	if (sp->type != T_STRING) {
	    i_del_value(sp++);
	    return (char *) NULL;
	}
	file = path_file(path_resolve(sp->u.string->text));
	str_del((sp++)->u.string);
	return file;
    }
}

/*
 * NAME:	path->ed_write()
 * DESCRIPTION:	resolve an editor write file path
 */
char *path_ed_write(file)
char *file;
{
    if (i_this_object()->flags & O_DRIVER) {
	return path_file(path_resolve(file));
    } else {
	i_check_stack(1);
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new(file, (long) strlen(file)));
	call_driver_object("path_ed_write", 1);
	if (sp->type != T_STRING) {
	    i_del_value(sp++);
	    return (char *) NULL;
	}
	file = path_file(path_resolve(sp->u.string->text));
	str_del((sp++)->u.string);
	return file;
    }
}

/*
 * NAME:	path->object()
 * DESCRIPTION:	resolve an object path
 */
char *path_object(file)
char *file;
{
    if (i_this_program()->ninherits == 1) {
	/* driver or auto object */
	return path_file(path_resolve(file));
    } else {
	i_check_stack(1);
	(--sp)->type = T_STRING;
	str_ref(sp->u.string = str_new(file, (long) strlen(file)));
	call_driver_object("path_object", 1);
	if (sp->type != T_STRING) {
	    i_del_value(sp++);
	    return (char *) NULL;
	}
	file = path_resolve(sp->u.string->text);
	str_del((sp++)->u.string);
	return file;
    }
}

/*
 * NAME:	path->from()
 * DESCRIPTION:	resolve a (possibly relative) path
 */
static char *path_from(from, file)
register char *from, *file;
{
    char buf[STRINGSZ];

    if (file[0] != '/' && strlen(from) + strlen(file) < STRINGSZ - 4) {
	sprintf(buf, "%s/../%s", from, file);
	return path_resolve(buf);
    }
    return path_resolve(file);
}

/*
 * NAME:	path->inherit()
 * DESCRIPTION:	resolve an inherit path
 */
char *path_inherit(from, file)
char *from, *file;
{
    if (c_autodriver()) {
	return path_from(from, file);
    }
    i_check_stack(2);
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new((char *) NULL, strlen(from) + 1L));
    sp->u.string->text[0] = '/';
    strcpy(sp->u.string->text + 1, from);
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new(file, (long) strlen(file)));
    if (!call_driver_object("path_inherit", 2)) {
	sp++;
	return path_from(from, file);
    }
    if (sp->type != T_STRING) {
	i_del_value(sp++);
	return (char *) NULL;
    }
    file = path_resolve(sp->u.string->text);
    str_del((sp++)->u.string);
    return file;
}

/*
 * NAME:	path->include()
 * DESCRIPTION:	resolve an include path
 */
char *path_include(from, file)
char *from, *file;
{
    if (c_autodriver()) {
	return path_file(path_from(from, file));
    }
    i_check_stack(2);
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new((char *) NULL, strlen(from) + 1L));
    sp->u.string->text[0] = '/';
    strcpy(sp->u.string->text + 1, from);
    (--sp)->type = T_STRING;
    str_ref(sp->u.string = str_new(file, (long) strlen(file)));
    if (!call_driver_object("path_include", 2)) {
	sp++;
	return path_file(path_from(from, file));
    }
    if (sp->type != T_STRING) {
	i_del_value(sp++);
	return (char *) NULL;
    }
    file = path_file(path_resolve(sp->u.string->text));
    str_del((sp++)->u.string);
    return file;
}
