New types can be added using builtin light-weight objects.  These types
have a name that starts with "/builtin", like: "/builtin/type#-1" (this
is configurable with the `BIPREFIX` define in `config.h`).

To add a type, reserve a positive number for it in `object.h`, like
```
    # define BUILTIN_TYPE	0
```
Then give it a name in `Object::builtinName()`:
```
    const char *Object::builtinName(Int type)
    {
	switch (type) {
	case BUILTIN_TYPE:
	    return BIPREFIX "type";
	default:
	    fatal("unknown builtin type %d", type);
	}
    }
```
A function to create a light-weight object of this type could look like this:
```
    # ifdef FUNCDEF
    FUNCDEF("new_type", kf_new_type, pt_new_type, 0)
    # else
    char pt_new_type[] = { C_TYPECHECKED | C_STATIC, 0, 0, 0, 6, T_OBJECT };

    /*
     * NAME:        kfun->new_type()
     * DESCRIPTION: create a new type!
     */
    int kf_new_type(Frame *f, int n, KFun *kf)
    {
	Array *a;
	Float flt;
	String *str;

	a = Array::create(f->data, 4);

	/* first element is the type as an integer */
	PUT_INTVAL(&a->elts[0], BUILTIN_TYPE);
	/* second element must be Float 0.0 */
	flt.high = 0;
	flt.low = 0;
	PUT_FLTVAL(&a->elts[1], flt);

	/* suggested third element: version number */
	PUT_INTVAL(&a->elts[2], 0);
	/* fourth element: data */
	str = String::create("some info", 9);
	PUT_STRVAL(&a->elts[3], str);

	/* push the return value on the stack */
	PUSH_LWOVAL(f, a);
	return 0;
    }
    # endif
```
Ordinary light-weight objects will have an object as the first element of
their array.  For builtin types, the first element must be an integer,
the type number; the second element is reserved and must be initialized
to `Float` 0.0.

For the third element, a version number for your type is suggested, in case
you decide to change the way your type is build and want to convert old
instances.  The fourth and later elements can hold arbitrary LPC data.

By default, builtin types have no functions callable from LPC, functions
that can be probed with `function_object()`, or information about them that
can be returned with `status()`.  If you do want them to have any of that,
edit the source code for the functions `Frame::call()`, `kf_function_object()`,
`kf_status()` and `kf_statuso_idx()`.

At compile time, a node of a builtin type should have modifier `T_CLASS`, and
a class string built from the base name, as returned by `Object::builtinName()`.
