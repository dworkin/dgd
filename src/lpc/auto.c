varargs void dump(string function)
{
    if (function) {
	dump_function(this_object(), function);
    } else {
	dump_object(this_object());
    }
}

void iyy() {}
