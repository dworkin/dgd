# include <string.h>

# define IDX(a, i, n)	((void *) ((char *) (a) + (i) * (n)))

/*
 * NAME:	qsort()
 * DESCRIPTION:	sort an array
 */
void qsort(void *arr, size_t size, size_t sz,
	   int (*cmp)(const void *a, const void *b))
{
    char elt[128];
    void *val;
    int n, i, j;

    i = 1;
    for (;;) {
	if (i >= size) {
	    return;
	}
	if (cmp(IDX(arr, i - 1, sz), IDX(arr, i, sz)) > 0) {
	    break;
	}
	i++;
    }

    for (n = 1; n < size; n <<= 1) ;

    for (n >>= 1; n > 0; --n) {
	memcpy(elt, IDX(arr, n - 1, sz), sz);
	for (i = n, j = n << 1; j <= size; i = j, j <<= 1) {
	    val = IDX(arr, j - 1, sz);
	    if (j < size && cmp(IDX(arr, j, sz), val) > 0) {
		val = IDX(arr, j++, sz);
	    }
	    if (cmp(elt, val) > 0) {
		break;
	    }
	    memcpy(IDX(arr, i - 1, sz), val, sz);
	}
	memcpy(IDX(arr, i - 1, sz), elt, sz);
    }

    for (n = size - 1; n > 0; --n) {
	memcpy(elt, IDX(arr, n, sz), sz);
	memcpy(IDX(arr, n, sz), arr, sz);
	for (i = 1, j = 2; j <= n; i = j, j <<= 1) {
	    val = IDX(arr, j - 1, sz);
	    if (j < n && cmp(IDX(arr, j, sz), val) > 0) {
		val = IDX(arr, j++, sz);
	    }
	    if (cmp(elt, val) > 0) {
		break;
	    }
	    memcpy(IDX(arr, i - 1, sz), val, sz);
	}
	memcpy(IDX(arr, i - 1, sz), elt, sz);
    }
}
