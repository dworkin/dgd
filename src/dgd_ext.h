# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "xfloat.h"
# include "interpret.h"
# include "data.h"
# include "table.h"

/*
 * interface
 */
# define DGD_EXT_KFUN(ekf, n)	kf_ext_kfun((ekf), (n))
# define DGD_EXT_CALLBACK(r, s, d, f, c, e) \
				conf_ext_callback((r), (s), (d), (f), (c), (e))
# define DGD_ERROR(m)		error((m))

/*
 * types
 */
# define DGD_INT_T		Int
# define DGD_FLOAT_T		xfloat
# define DGD_STRING_T		string*
# define DGD_OBJECT_T		object*
# define DGD_ARRAY_T		array*
# define DGD_MAPPING_T		array*
# define DGD_LWOBJ_T		array*
# define DGD_VALUE_T		value
# define DGD_FRAME_T		frame*
# define DGD_DATASPACE_T	dataspace*
# define DGD_EXTKFUN_T		extkfunc
# define DGD_EINDEX_T		eindex

/*
 * prototype and value types
 */
# define DGD_TYPE_VOID		T_VOID
# define DGD_TYPE_NIL		T_NIL
# define DGD_TYPE_INT		T_INT
# define DGD_TYPE_FLOAT		T_FLOAT
# define DGD_TYPE_STRING	T_STRING
# define DGD_TYPE_OBJECT	T_OBJECT
# define DGD_TYPE_ARRAY		T_ARRAY
# define DGD_TYPE_MAPPING	T_MAPPING
# define DGD_TYPE_LWOBJ		T_LWOBJECT
# define DGD_TYPE_MIXED		T_MIXED

# define DGD_TYPE_ARRAY_OF(t)	((t) + (1 << REFSHIFT))
# define DGD_TYPE_VARARGS	T_VARARGS
# define DGD_TYPE_ELLIPSIS	T_ELLIPSIS

/*
 * frame
 */
# define DGD_FRAME_OBJECT(f)	OBJW((f)->oindex)
# define DGD_FRAME_LWOBJ(f)	((f)->lwobj)
# define DGD_FRAME_PROGRAM(f)	OBJW((f)->p_ctrl->oindex)
# define DGD_FRAME_DATASPACE(f)	((f)->data)
# define DGD_FRAME_ARG(f, n, i)	(*((f)->sp + (n) - ((i) + 1)))
# define DGD_FRAME_ATOMIC(f)	((f)->level != 0)

/*
 * dataspace
 */
# define DGD_DATA_GET_VAL(d)	(*d_get_extravar((d)))
# define DGD_DATA_SET_VAL(d, v)	d_set_extravar((d), &(v))

/*
 * value
 */
# define DGD_TYPEOF(v)		((v).type)

# define DGD_RETVAL_INT(v, i)	PUT_INTVAL((v), (i))
# define DGD_RETVAL_FLT(v, f)	PUT_FLTVAL((v), (f))
# define DGD_RETVAL_STR(v, s)	PUT_STRVAL((v), (s))
# define DGD_RETVAL_OBJ(v, o)	PUT_OBJVAL((v), (o))
# define DGD_RETVAL_ARR(v, a)	PUT_ARRVAL((v), (a))
# define DGD_RETVAL_MAP(v, m)	PUT_MAPVAL((v), (m))

/*
 * nil
 */
# define DGD_NIL_VALUE		nil_value

/*
 * int
 */
# define DGD_INT_GETVAL(v)	((v).u.number)
# define DGD_INT_PUTVAL(v, i)	PUT_INTVAL(&(v), (i))

/*
 * float
 */
# define DGD_FLOAT_GETVAL(v, f)		GET_FLT(&(v), (f))
# define DGD_FLOAT_PUTVAL(v, f)		PUT_FLTVAL(&(v), (f))
# define DGD_FLOAT_GET(f, s, e, m)	((((f).high | (f).low) == 0) ? \
					  ((s) = 0, (e) = 0, (m) = 0) : \
					  ((s) = (f).high >> 15, \
					   (e) = (((f).high >> 4) & 0x7ff) - \
						 1023, \
					   (m) = 0x10 | ((f).high & 0xf), \
					   (m) <<= 32, (m) |= (f).low))
# define DGD_FLOAT_PUT(f, s, e, m)	((f).high = (((m) == 0) ? 0 : \
						      ((s) << 15) | \
						      (((e) + 1023) << 4) | \
						      (((m) >> 32) & 0xf)), \
					 (f).low = (m))

/*
 * string
 */
# define DGD_STRING_GETVAL(v)		((v).u.string)
# define DGD_STRING_PUTVAL(v, s)	PUT_STRVAL_NOREF(&(v), (s))
# define DGD_STRING_NEW(t, n)		str_new((t), (long) (n))
# define DGD_STRING_TEXT(s)		((s)->text)
# define DGD_STRING_LENGTH(s)		((s)->len)

/*
 * object
 */
# define DGD_OBJECT_GETVAL(v)		OBJW((v).oindex)
# define DGD_OBJECT_PUTVAL(v, o)	PUT_OBJVAL(&(v), (o))
# define DGD_OBJECT_CHECKVAL(v, o)	((v).u.objcnt == (o)->count)
# define DGD_OBJECT_DATASPACE(o)	o_data((o))
# define DGD_OBJECT_NAME(buf, o)	o_name((buf), (o))
# define DGD_OBJECT_ISSPECIAL(o)	(((o)->flags & O_SPECIAL) != 0)
# define DGD_OBJECT_ISMARKED(o)		(((o)->flags & O_SPECIAL) == O_SPECIAL)
# define DGD_OBJECT_MARK(o)		((o)->flags |= O_SPECIAL)
# define DGD_OBJECT_UNMARK(o)		((o)->flags &= ~O_SPECIAL)
# define DGD_OBJECT_GET_EINDEX(o)	((o)->etabi)
# define DGD_OBJECT_SET_EINDEX(o, e)	((o)->etabi = (e))

/*
 * array
 */
# define DGD_ARRAY_GETVAL(v)		((v).u.array)
# define DGD_ARRAY_PUTVAL(v, a)		PUT_ARRVAL_NOREF(&(v), (a))
# define DGD_ARRAY_NEW(d, n)		arr_ext_new((d), (long) (n))
# define DGD_ARRAY_ELTS(a)		d_get_elts((a))
# define DGD_ARRAY_SIZE(a)		((a)->size)
# define DGD_ARRAY_INDEX(a, i)		(d_get_elts((a))[(i)])
# define DGD_ARRAY_ASSIGN(d, a, i, v)	d_assign_elt((d), (a), \
						    &d_get_elts((a))[(i)], &(v))
/*
 * mapping
 */
# define DGD_MAPPING_GETVAL(v)		((v).u.array)
# define DGD_MAPPING_PUTVAL(v, m)	PUT_MAPVAL_NOREF(&(v), (m))
# define DGD_MAPPING_NEW(d)		map_new((d), 0L)
# define DGD_MAPPING_ELTS(m)		(map_compact((m)), d_get_elts((m)))
# define DGD_MAPPING_SIZE(m)		map_size((m))
# define DGD_MAPPING_INDEX(m, i)	(*map_index((m)->primary->data, (m), \
						    &(i), (value *) NULL))
# define DGD_MAPPING_ASSIGN(d, m, i, v)	map_index((d), (m), &(i), &(v))

/*
 * light-weight object
 */
# define DGD_LWOBJ_GETVAL(v)		((v).u.array)
# define DGD_LWOBJ_PUTVAL(v, o)		PUT_LWOBJ_NOREF(&(v), (o))
# define DGD_LWOBJ_CHECKVAL(o)		(d_get_elts((o)), \
					 OBJR((o)->elts->oindex)->count == \
							    (o)->elts->u.objcnt)
# define DGD_LWOBJ_COPY(d, o)		lwo_copy((d), (o))
