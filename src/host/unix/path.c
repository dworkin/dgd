char *path_resolve(file)
char *file;
{
    static char buf[STRINGSZ];
    register char *p, *q, *d;

    strncpy(buf, file, STRINGSZ - 1);
    buf[STRINGSZ - 1] = '\0';
    d = p = q = buf;
    while (*p != '\0') {
	if (*p == '/') {
	    *q = '\0';
	    if (d == q || strcmp(d, ".") == 0) {
		q = d;
	    } else if (strcmp(d, "..") == 0) {
		q = d;
		if (q != buf) {
		    for (--q; q > buf; ) {
			if (*--q == '/') {
			    q++;
			    break;
			}
		    }
		}
	    } else {
		*q++ = '/';
	    }
	    d = q;
	    p++;
	} else {
	    *q++ = *p++;
	}
    }

    *q = '\0';
    return buf;
}
