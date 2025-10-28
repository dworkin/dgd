/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
# include "asn.h"

class Asi {
public:
    Asi(Uint *num, Uint size) : num(num), size(size) { }
    Asi() {
	num = NULL;
	size = 0;
    }

    void copy(Asi &x);
    bool add(Asi &x);
    bool sub(Asi &x);
    void lshift(Uint lshift);
    void rshift(Uint rshift);
    int cmp(Asi &a);
    void mult(Asi &x, Asi &y, Asi &t);
    void sqr(Asi &x, Asi &t);
    Uint *div(Asi &x, Asi &y, Asi &t);
    bool modinv(Asi &x, Asi &y);
    void power(Asi &a, Asi &b, Asi &mod, Asi &t);
    bool strtonum(String *str);
    String *numtostr(bool minus);

    Uint *num;
    Uint size;

private:
    void mult1(Uint a, Uint b);
    void multInner(Asi &x, Asi &y, Asi &t);
    bool multRow(Asi &x, Uint y);
    void sqr1(Uint a);
    Uint div1(Uint a);
    void monpro(Asi &x, Asi &y, Asi &n, Asi &t, Uint n0);
    void powqmod(Asi &a, Asi &b, Asi &mod, Asi &t);
    void pow2mod(Asi &a, Asi &b, Uint size, Asi &t);

    static Uint wordinv(Uint n);
};

/*
 * copy number
 */
void Asi::copy(Asi &x)
{
    size = x.size;
    memmove(num, x.num, size * sizeof(Uint));
}

/*
 * add x (size >= x.size)
 */
bool Asi::add(Asi &x)
{
    Uint *a, *b, sz, tmp, carry;

    a = num;
    b = x.num;
    sz = x.size;
    carry = 0;
    do {
	tmp = *a + carry;
	carry = (tmp < carry) + ((*a++ = tmp + *b++) < tmp);
    } while (--sz != 0);

    for (sz = size - x.size; sz != 0 && carry; --sz) {
	carry = (++(*a++) == 0);
    }

    return (bool) carry;
}

/*
 * sub x (size >= x.size)
 */
bool Asi::sub(Asi &x)
{
    Uint *a, *b, sz, tmp, borrow;

    a = num;
    b = x.num;
    sz = x.size;
    borrow = 0;
    do {
	borrow = ((tmp = *a - borrow) > *a);
	borrow += ((*a++ = tmp - *b++) > tmp);
    } while (--sz != 0);

    for (sz = size - x.size; sz != 0 && borrow; --sz) {
	borrow = ((*a++)-- == 0);
    }

    return (bool) borrow;
}

/*
 * left shift, lshift != 0
 */
void Asi::lshift(Uint lshift)
{
    Uint offset, *a, sz, rshift, tmp, bits;

    offset = (lshift + 31) >> 5;
    lshift &= 0x1f;
    a = num;
    sz = size - offset;

    if (lshift == 0) {
	a += sz;
	for (tmp = sz; tmp > 0; --tmp) {
	    --a;
	    a[offset] = *a;
	}
	bits = 0;
    } else {
	rshift = 32 - lshift;
	a += sz;
	bits = *a << lshift;
	while (sz != 0) {
	    tmp = *--a;
	    a[offset] = bits | (tmp >> rshift);
	    bits = tmp << lshift;
	    --sz;
	}
    }

    a += offset;
    do {
	*--a = bits;
	bits = 0;
    } while (--offset != 0);
}

/*
 * right shift, rshift != 0
 */
void Asi::rshift(Uint rshift)
{
    Uint offset, *a, sz, lshift, tmp, bits;

    offset = (rshift + 31) >> 5;
    rshift &= 0x1f;
    a = num;
    sz = size - offset;

    if (rshift == 0) {
	for (tmp = sz; tmp > 0; --tmp) {
	    *a = a[offset];
	    a++;
	}
	bits = 0;
    } else {
	lshift = 32 - rshift;
	bits = a[offset - 1] >> rshift;
	while (sz != 0) {
	    tmp = a[offset];
	    *a++ = bits | (tmp << lshift);
	    bits = tmp >> rshift;
	    --sz;
	}
    }

    do {
	*a++ = bits;
	bits = 0;
    } while (--offset != 0);
}

/*
 * compare with x (size >= x.size)
 */
int Asi::cmp(Asi &x)
{
    Uint *a, sz, *b;

    sz = size;
    a = num + sz;
    while (sz > x.size) {
	if (*--a != 0) {
	    return 1;
	}
	--sz;
    }
    b = x.num + sz;

    do {
	if (*--a < *--b) {
	    return -1;
	} else if (*a > *b) {
	    return 1;
	}
    } while (--sz != 0);

    return 0;
}

/*
 * a * b
 */
void Asi::mult1(Uint a, Uint b)
{
    uint64_t t;

    t = (uint64_t) (a) * (b);
    num[0] = t;
    num[1] = t >> 32;
    size = 2;
}

/*
 * compute x * y (x.size - y.size <= 1)
 * t.size = (x.size + y.size) << 1
 */
void Asi::multInner(Asi &x, Asi &y, Asi &t)
{
    if (y.size == 1) {
	/* x.size <= 2, y.size == 1 */
	mult1(x.num[0], y.num[0]);
	if (x.size > 1) {
	    /* x.size == 2, y.size == 1 */
	    t.mult1(x.num[1], y.num[0]);
	    if ((num[1] += t.num[0]) < t.num[0]) {
		t.num[1]++;
	    }
	    num[2] = t.num[1];
	}
    } else {
	Asi x0(x.num, x.size >> 1);
	Asi x1(x.num + x0.size, x.size - x0.size);
	Asi y0(y.num, x.size >> 1);
	Asi y1(y.num + y0.size, y.size - y0.size);
	Asi z1;
	bool minus;

	/* z0 = x0 - x1, z1 = y1 - y0 */
	if (x1.cmp(x0) <= 0) {
	    /* x0 - x1 */
	    copy(x0);
	    sub(x1);
	    minus = FALSE;
	} else {
	    /* -(x1 - x0) */
	    copy(x1);
	    sub(x0);
	    minus = TRUE;
	}
	z1 = Asi(num + size, 0);
	if (y0.size <= y1.size) {
	     if (y1.cmp(y0) >= 0) {
		/* y1 - y0 */
		z1.copy(y1);
		z1.sub(y0);
	    } else {
		/* -(y0 - y1) */
		z1.copy(y0);
		z1.sub(y1);
		minus ^= TRUE;
	    }
	} else if (y0.cmp(y1) <= 0) {
	    /* y1 - y0 */
	    z1.copy(y1);
	    z1.sub(y0);
	} else {
	    /* -(y0 - y1) */
	    z1.copy(y0);
	    z1.sub(y1);
	    minus ^= TRUE;
	}

	/* t3:t2 = z0 * z1 */
	Asi t23(t.num + ((x.size + y.size) << 1) - size - z1.size, 0);
	if (size >= z1.size) {
	    t23.multInner(*this, z1, t);
	} else {
	    t23.multInner(z1, *this, t);
	}

	/* z1:z0 = x0 * y0, z3:z2 = x1 * y1 */
	multInner(x0, y0, t);
	Asi z23(num + size, 0);
	z23.multInner(x1, y1, t);

	/* t1:t0 = z3:z2 + z1:z0 + t3:t2 */
	t.copy(z23);
	t.num[t.size++] = 0;
	t.add(*this);
	if (minus) {
	    t.sub(t23);
	} else {
	    t.add(t23);
	}

	/* z3:z2:z1 += t1:t0 */
	Asi(num + x0.size, x0.size + z23.size).add(t);
    }
    size = x.size + y.size;
}

/*
 * compute x * y
 * t.size = (x.size + y.size) * 3
 */
void Asi::mult(Asi &x, Asi &y, Asi &t1)
{
    Asi a(x.num, x.size);
    Asi b(y.num, y.size);
    Asi z(num, size = a.size + b.size);
    Asi t2(t1.num + size, 0);

    memset(num, '\0', size * sizeof(Uint));
    while (a.size != b.size) {
	if (a.size < b.size) {
	    Asi t = a;
	    a = b;
	    b = t;
	}
	if (a.size - b.size == 1) {
	    break;
	}

	Asi c(a.num, b.size);
	t1.multInner(c, b, t2);
	z.size = t1.size;
	z.add(t1);
	a.num += b.size;
	a.size -= b.size;
	z.num += b.size;
    }
    t1.multInner(a, b, t2);
    z.size = t1.size;
    z.add(t1);
}

/*
 * add x * word
 */
bool Asi::multRow(Asi &x, Uint y)
{
    Uint *a, *b, sz, s, carry;
    Uint tmp[2];
    Asi t(tmp, 2);

    a = num;
    b = x.num;
    sz = x.size;
    s = 0;
    carry = 0;
    do {
	t.mult1(*b++, y);
	if ((s += t.num[0]) < t.num[0]) {
	    t.num[1]++;
	}
	carry = ((s += carry) < carry);
	carry += ((*a++ += s) < s);
	s = t.num[1];
    } while (--sz != 0);

    carry = ((s += carry) < carry);
    return (bool) (carry + ((*a += s) < s));
}

void Asi::sqr1(Uint a)
{
    uint64_t t;

    t = (uint64_t) a * a;
    num[0] = t;
    num[1] = t >> 32;
    size = 2;
}

/*
 * x * x
 * t.size = x.size << 2
 */
void Asi::sqr(Asi &x, Asi &t)
{
    if (x.size == 1) {
	sqr1(x.num[0]);
    } else {
	Asi x0(x.num, x.size >> 1);
	Asi x1(x.num + x0.size, x.size - x0.size);

	/* y0 = x0 - x1 */
	if (x1.cmp(x0) <= 0) {
	    /* x0 - x1 */
	    copy(x0);
	    sub(x1);
	} else {
	    /* -(x1 - x0) */
	    copy(x1);
	    sub(x0);
	}

	/* t3:t2 = y0 * y0 */
	Asi t23(t.num + (x.size << 2) - (size << 1), size);
	t23.sqr(*this, t);

	/* y1:y0 = x0 * x0, y3:y2 = x1 * x1 */
	sqr(x0, t);
	Asi y23(num + size, x1.size << 1);
	y23.sqr(x1, t);

	/* t1:t0 = y3:y2 + y1:y0 - t3:t2 */
	t.copy(y23);
	t.num[t.size++] = 0;
	t.add(*this);
	t.sub(t23);

	/* y3:y2:y1 += t1:t0 */
	Asi y1(num + x0.size, x0.size + y23.size);
	y1.add(t);
    }
    size = x.size << 1;
}

Uint Asi::div1(Uint a)
{
    return (((uint64_t) num[1] << 32) | num[0]) / a;
}

/*
 * z1 = x / y, z0 = x % y (x >= y, y != 0)
 * t.size = (y.size << 1) + 1
 */
Uint *Asi::div(Asi &x, Asi &y, Asi &t)
{
    Uint d, q, shift;
    Asi a(num, size);
    Asi b(t.num, y.size);
    Asi t2(t.num + b.size, b.size + 1);

    /* copy values */
    if (&x != this) {
	a.copy(x);
    }
    a.num[a.size] = 0;
    b.copy(y);

    /*
     * left shift until most significant bit of b is 1
     */
    for (shift = 0, d = b.num[b.size - 1]; !(d & 0x80000000L); shift++, d <<= 1)
	;
    if (shift != 0) {
	b.lshift(shift);
	a.size++;
	a.lshift(shift);
	if (a.num[a.size - 1] == 0) {
	    --a.size;
	}
    }

    size = a.size;
    a.size = b.size;
    if (size == b.size) {
	/* a >= b */
	a.sub(b);
	num[size] = 1;
    } else {
	a.num += size - b.size;
	if (a.cmp(b) >= 0) {
	    a.sub(b);
	    num[size] = 1;
	} else {
	    num[size] = 0;
	}
	--a.num;
	a.size++;

	/*
	 * perform actual division
	 */
	d = b.num[b.size - 1];
	do {
	    if (a.num[b.size] == d) {
		q = 0xffffffffL;
	    } else {
		q = Asi(a.num + b.size - 1, 2).div1(d);
	    }
	    memset(t2.num, '\0', t2.size * sizeof(Uint));
	    t2.multRow(b, q);
	    if (t2.cmp(a) > 0) {
		t2.sub(b);
		--q;
		if (t2.cmp(a) > 0) {
		    t2.sub(b);
		    --q;
		}
	    }
	    a.sub(t2);
	    num[--size] = q;
	    --a.num;
	} while (size > b.size);
    }

    if (shift != 0) {
	/* compensate for left shift */
	rshift(shift);
    }
    return num + size;
}

/*
 * Compute the multiplicative inverse of a modulo b.
 * From "Applied Cryptography" by Bruce Schneier, Second Edition, page 247.
 */
bool Asi::modinv(Asi &x, Asi &y)
{
    Asi a, b;
    bool inverse;

    if (!((x.num[0] | y.num[0]) & 1)) {
	return FALSE;	/* GCD >= 2 */
    }

    if (x.size > y.size || (x.size == y.size && x.cmp(y) > 0)) {
	/* a > b: a <=> b */
	a = y;
	b = x;
	inverse = TRUE;
    } else {
	a = x;
	b = y;
	inverse = FALSE;
    }

    /* b1 * b - a1 * a = b */
    Asi b1(ALLOCA(Uint, a.size + b.size), 1);		/* b1 = 1 */
    memset(b1.num, '\0', (a.size + b.size) * sizeof(Uint));
    b1.num[0] = 1;
    b1.size = 1;
    Asi a1(ALLOCA(Uint, a.size + b.size), 1);		/* a1 = 0 */
    memset(a1.num, '\0', (a.size + b.size) * sizeof(Uint));
    a1.size = 1;
    Asi g1(ALLOCA(Uint, b.size), 0);			/* g1 = b */
    g1.copy(b);

    /* b2 * b - a2 * a = a */
    Asi b2(ALLOCA(Uint, a.size + b.size), 0);		/* b2 = a */
    b2.copy(a);
    memset(b2.num + a.size, '\0', b.size * sizeof(Uint));
    Asi a2(ALLOCA(Uint, a.size + b.size), 0);		/* a2 = b - 1 */
    a2.copy(b);
    memset(a2.num + b.size, '\0', a.size * sizeof(Uint));
    a2.sub(b1);
    while (a2.num[a2.size - 1] == 0) {
	if (--a2.size == 0) {
	    a2.size++;
	    break;
	}
    }
    Asi g2(ALLOCA(Uint, a.size), 0);			/* g2 = a */
    g2.copy(a);

    do {
	do {
	    if (!(g1.num[0] & 1)) {
		if ((a1.num[0] | b1.num[0]) & 1) {
		    if (a1.size < b.size) {
			a1.size = b.size;
		    }
		    if (a1.add(b)) {
			a1.num[a1.size++] = 1;
		    }
		    if (b1.size < a.size) {
			b1.size = a.size;
		    }
		    if (b1.add(a)) {
			b1.num[b1.size++] = 1;
		    }
		}
		a1.rshift(1);
		if (a1.num[a1.size - 1] == 0 && --a1.size == 0) {
		    a1.size++;
		}
		b1.rshift(1);
		if (b1.num[b1.size - 1] == 0 && --b1.size == 0) {
		    b1.size++;
		}
		g1.rshift(1);
		if (g1.num[g1.size - 1] == 0 && --g1.size == 0) {
		    g1.size++;
		}
	    }
	    if (!(g2.num[0] & 1) || g1.size < g2.size || g1.cmp(g2) < 0) {
		Asi t;

		/* a1 <=> a2, b1 <=> b2, g1 <=> g2 */
		t = a1;
		a1 = a2;
		a2 = t;
		t = b1;
		b1 = b2;
		b2 = t;
		t = g1;
		g1 = g2;
		g2 = t;
	    }
	} while (!(g1.num[0] & 1));

	while (a1.size < a2.size || a1.cmp(a2) < 0 ||
	       b1.size < b2.size || b1.cmp(b2) < 0) {
	    if (a1.size < b.size) {
		a1.size = b.size;
	    }
	    if (a1.add(b)) {
		a1.num[a1.size++] = 1;
	    }
	    if (b1.size < a.size) {
		b1.size = a.size;
	    }
	    if (b1.add(a)) {
		b1.num[b1.size++] = 1;
	    }
	}

	a1.sub(a2);
	while (a1.num[a1.size - 1] == 0) {
	    if (--a1.size == 0) {
		a1.size++;
		break;
	    }
	}
	b1.sub(b2);
	while (b1.num[b1.size - 1] == 0) {
	    if (--b1.size == 0) {
		b1.size++;
		break;
	    }
	}
	g1.sub(g2);
	while (g1.num[g1.size - 1] == 0) {
	    if (--g1.size == 0) {
		g1.size++;
		break;
	    }
	}
    } while (g2.size != 1 || g2.num[0] != 0);

    while (a1.size >= b.size && a1.cmp(b) >= 0 &&
	   b1.size >= a.size && b1.cmp(a) >= 0) {
	a1.sub(b);
	while (a1.num[a1.size - 1] == 0) {
	    if (--a1.size == 0) {
		a1.size++;
		break;
	    }
	}
	b1.sub(a);
	while (b1.num[b1.size - 1] == 0) {
	    if (--b1.size == 0) {
		b1.size++;
		break;
	    }
	}
    }

    if (!inverse) {
	b1.copy(b);
	b1.sub(a1);
	while (b1.num[b1.size - 1] == 0) {
	    if (--b1.size == 0) {
		b1.size++;
		break;
	    }
	}
    }
    copy(b1);
    inverse = (g1.size == 1 && g1.num[0] == 1);
    AFREE(g2.num);
    AFREE(a2.num);
    AFREE(b2.num);
    AFREE(g1.num);
    AFREE(a1.num);
    AFREE(b1.num);

    return inverse;
}

/*
 * The algorithms for fast exponentiation are based on:
 * "High-Speed RSA Implementation" by Çetin Koya Koç, Version 2.0,
 * RSA Laboratories
 */

/*
 * compute an inverse modulo the word size (for odd n)
 */
Uint Asi::wordinv(Uint n)
{
    Uint n1, mask, i;

    n1 = 1;
    mask = 0x3;
    i = 0x2;
    do {
	if (((n * n1) & mask) > i) {
	    n1 += i;
	}
	i <<= 1;
	mask |= i;
    } while (i != 0);

    return n1;
}

/*
 * compute the Montgomery product of a and b
 * t.size = (size + 1) << 1
 */
void Asi::monpro(Asi &x, Asi &y, Asi &n, Asi &t, Uint n0)
{
    Uint i, j, m, *d, carry;

    if (x.num == y.num) {
	sqr(x, t);
    } else {
	multInner(x, y, t);
    }
    num[size] = 0;
    Asi c(num, n.size + 1);
    for (i = n.size; i > 0; --i) {
	m = c.num[0] * n0;
	if (c.multRow(n, m)) {
	    d = c.num + n.size;
	    j = i;
	    carry = 1;
	    do {
		carry = ((*++d += carry) < carry);
	    } while (carry && --j != 0);
	}
	c.num++;
    }
    if (c.cmp(n) >= 0) {
	c.sub(n);
    }
}

# define WINDOWSZ	6

/*
 * compute a ** b % mod (a > 1, b > 1, (mod & 1) != 0)
 * t.size = (mod.size + 1) << 1
 */
void Asi::powqmod(Asi &a, Asi &b, Asi &mod, Asi &t)
{
    Uint bit, e, window, wsize, zsize, n0;
    Asi tab[1 << WINDOWSZ];

    /* allocate */
    Asi x(ALLOCA(Uint, (mod.size << 1) + 1), 0);
    Asi xx(x.num + mod.size, 0);
    Asi y(ALLOCA(Uint, (mod.size << 1) + a.size + 1), mod.size + a.size);
    Asi yy(y.num + mod.size, 0);

    /* xx = a * R % mod */
    memset(y.num, '\0', mod.size * sizeof(Uint));
    yy.copy(a);
    y.div(y, mod, t);
    xx.copy(y);

    /* tab[] = { powers of xx } */
    tab[0] = xx;	/* sentinel */
    tab[1] = Asi(ALLOCA(Uint, mod.size), 0);
    tab[1].copy(xx);

    n0 = -wordinv(mod.num[0]);
    window = wsize = zsize = 0;
    size = b.size;
    e = b.num[size - 1];
    for (bit = 0x80000000L; (e & bit) == 0; bit >>= 1) ;
    bit >>= 1;	/* skip most significant bit of top word */

    for (;;) {
	while (bit != 0) {
	    if (wsize == WINDOWSZ || (zsize != 0 && !(e & bit))) {
		y.monpro(xx, tab[window], mod, t, n0);
		window = wsize = zsize = 0;
	    } else {
		window <<= 1;
		if (tab[window].num == (Uint *) NULL) {
		    y.monpro(tab[window >> 1], tab[window >> 1], mod, t, n0);
		    tab[window] = Asi(ALLOCA(Uint, mod.size), 0);
		    tab[window].copy(yy);
		}
		yy.copy(xx);
	    }

	    x.monpro(yy, yy, mod, t, n0);

	    if (e & bit) {
		window |= 1;
		if (tab[window].num == (Uint *) NULL) {
		    y.monpro(tab[window - 1], tab[1], mod, t, n0);
		    tab[window] = Asi(ALLOCA(Uint, mod.size), 0);
		    tab[window].copy(yy);
		}
		zsize = 0;
		wsize++;
	    } else if (wsize != 0) {
		zsize++;
		wsize++;
	    }

	    bit >>= 1;
	}

	if (--size == 0) {
	    break;
	}
	/* next word in exponent */
	e = b.num[size - 1];
	bit = 0x80000000L;
    }

    if (wsize != 0) {
	/*
	 * still a remaining window of bits to deal with
	 */
	y.monpro(xx, tab[window], mod, t, n0);
	xx.copy(yy);
    }

    /* c = xx * (R ** -1) */
    memset(tab[1].num, '\0', mod.size * sizeof(Uint));
    tab[1].num[0] = 1;
    y.monpro(xx, tab[1], mod, t, n0);
    copy(yy);

    for (window = 1 << WINDOWSZ; --window != 0; ) {
	if (tab[window].num != (Uint *) NULL) {
	    AFREE(tab[window].num);
	}
    }
    AFREE(y.num);
    AFREE(x.num);
}

/*
 * compute a ** b, (all operations in size words)
 * t.size = (size + 1) << 1
 */
void Asi::pow2mod(Asi &a, Asi &b, Uint size, Asi &t)
{
    Uint e, bit, sizeb;
    Asi x(ALLOCA(Uint, size), size);
    Asi y(ALLOCA(Uint, size << 1), 0);
    Asi z(ALLOCA(Uint, size << 1), 0);

    /* x = a reduced to size words */
    if (a.size >= size) {
	memcpy(x.num, a.num, size * sizeof(Uint));
    } else {
	memcpy(x.num, a.num, a.size * sizeof(Uint));
	memset(x.num + a.size, '\0', (size - a.size) * sizeof(Uint));
    }

    /* remove leading zeroes from b */
    for (sizeb = size; b.num[sizeb - 1] == 0; --sizeb) {
	if (sizeb == 1) {
	    /* a ** 0 = 1 */
	    memset(num, '\0', size * sizeof(Uint));
	    num[0] = 1;
	    this->size = size;
	    return;
	}
    }

    y.copy(x);
    e = b.num[sizeb - 1];
    for (bit = 0x80000000L; (e & bit) == 0; bit >>= 1) ;
    bit >>= 1;	/* skip most significant bit of top word */

    for (;;) {
	while (bit != 0) {
	    z.sqr(y, t);
	    z.size = size;
	    if (e & bit) {
		y.multInner(z, x, t);
		y.size = size;
	    } else {
		y.copy(z);
	    }
	    bit >>= 1;
	}

	if (--sizeb == 0) {
	    break;
	}
	/* next word in exponent */
	e = b.num[sizeb - 1];
	bit = 0x80000000L;
    }

    copy(y);

    AFREE(z.num);
    AFREE(y.num);
    AFREE(x.num);
}

/*
 * compute a ** b % mod (a >= 0, b >= 0)
 * t.size == (mod.size + 2) << 1
 */
void Asi::power(Asi &a, Asi &b, Asi &mod, Asi &t)
{
    if (b.size == 1 && b.num[0] == 0) {
	/* a ** 0 = 1 */
	num[0] = 1;
	size = 1;
	return;
    }
    if (a.size == 1) {
	if (a.num[0] == 0) {
	    /* 0 ** b = 0 */
	    num[0] = 0;
	    size = 1;
	    return;
	}
	if (a.num[0] == 1) {
	    /* 1 ** b = 1 */
	    num[0] = 1;
	    size = 1;
	    return;
	}
    }

    if (mod.num[0] & 1) {
	/*
	 * modulo odd number
	 */
	powqmod(a, b, mod, t);
    } else {
	Uint i;
	bool minus;

	/*
	 * modulo even number
	 */
	Asi x(ALLOCA(Uint, mod.size), 0);
	Asi y(ALLOCA(Uint, mod.size + 1), 0);
	Asi z(ALLOCA(Uint, mod.size << 1), 0);
	Asi q(ALLOCA(Uint, mod.size), 0);
	Asi qinv(ALLOCA(Uint, mod.size), 0);

	/* j = (size << 5) + i = number of least significant zero bits */
	for (size = 0; mod.num[size] == 0; size++) ;
	for (i = 0; !(mod.num[size] & (1 << i)); i++) ;

	/* q = mod >> j */
	q.copy(mod);
	q.rshift((size << 5) + i);
	q.size -= size;
	if (q.num[q.size - 1] == 0) {
	    q.size--;
	}

	/* size = number of words, i = mask */
	if (i != 0) {
	    i = 0xffffffffL >> (32 - i);
	    size++;
	} else {
	    i = 0xffffffffL;
	}

	/* y = a ** b % 2 ** j */
	y.pow2mod(a, b, size, t);
	y.num[size - 1] &= i;
	y.size = size;

	if (q.size != 1 || q.num[0] != 1) {
	    x.powqmod(a, b, q, t);

	    /*
	     * y = x + q * ((y - x) * q ** -1 % 2 ** j)
	     */
	    if (size >= q.size && y.cmp(x) >= 0) {
		/* y - x */
		y.sub(x);
		minus = FALSE;
	    } else {
		/* x - y */
		t.copy(x);
		if (size > t.size) {
		    memset(t.num + t.size, '\0',
			   (size - t.size) * sizeof(Uint));
		}
		t.size = size;
		t.sub(y);
		y.copy(t);
		minus = TRUE;
	    }
	    /* q ** -1 */
	    if (size == 1) {
		qinv.num[0] = wordinv(q.num[0]);
		qinv.size = 1;
	    } else {
		memset(z.num, '\0', size * sizeof(Uint));
		if (i == 0xffffffffL) {
		    z.num[size] = 1;
		    z.size = size + 1;
		} else {
		    z.num[size - 1] = i + 1;
		    z.size = size;
		}
		qinv.modinv(q, z);
	    }
	    /* (y - x) * q ** -1 % 2 ** j */
	    z.multInner(y, qinv, t);
	    z.num[size - 1] &= i;
	    z.size = size;
	    /* q * ((y - x) * q ** -1 % 2 ** j) */
	    y.mult(q, z, t);
	    y.size = mod.size;
	    if (minus) {
		/* x - q * ((y - x) * q ** -1 % 2 ** j) */
		if (y.cmp(x) <= 0) {
		    memset(x.num + x.size, '\0',
			   (y.size - x.size) * sizeof(Uint));
		    x.size = y.size;
		    x.sub(y);
		    y.copy(x);
		} else {
		    t.copy(y);
		    t.sub(x);
		    y.copy(mod);
		    y.sub(t);
		}
	    } else {
		/* x + q * ((y - x) * q ** -1 % 2 ** j) */
		y.add(x);
		if (y.cmp(mod) >= 0) {
		    y.sub(mod);
		}
	    }
	}

	copy(y);

	AFREE(qinv.num);
	AFREE(q.num);
	AFREE(z.num);
	AFREE(y.num);
	AFREE(x.num);
    }
}

/*
 * convert a string to an ASI
 */
bool Asi::strtonum(String *str)
{
    ssizet len;
    char *text;
    Uint *tmp, bits;

    len = str->len;
    text = str->text;
    if (len == 0) {
	/*
	 * empty string: 0
	 */
	num[0] = 0;
	num[1] = 0;
	size = 1;
	return FALSE;
    } else if ((text[0] & 0x80)) {
	/*
	 * negative
	 */
	while (*text == '\xff') {
	    text++;
	    if (--len == 0) {
		/* -1 */
		num[0] = 1;
		num[1] = 0;
		size = 1;
		return TRUE;
	    }
	}
	size = (len + 3) / sizeof(Uint);
	memset(num, '\0', (size + 1) * sizeof(Uint));

	tmp = ALLOCA(Uint, size);
	text += len;
	while (len >= 4) {
	    text -= 4;
	    *tmp++ = (UCHAR(text[0]) << 24) |
		     (UCHAR(text[1]) << 16) |
		     (UCHAR(text[2]) << 8) |
		     UCHAR(text[3]);
	    len -= 4;
	}
	if (len != 0) {
	    text -= len;
	    bits = 0xffffffffL;
	    do {
		bits = (bits << 8) | UCHAR(*text++);
	    } while (--len != 0);
	    *tmp++ = bits;
	}
	tmp -= size;
	Asi t(tmp, size);
	sub(t);
	AFREE(tmp);

	return TRUE;

    } else {
	/*
	 * positive
	 */
	while (*text == '\0') {
	    text++;
	    if (--len == 0) {
		/* 0 */
		num[0] = 0;
		num[1] = 0;
		size = 1;
		return FALSE;
	    }
	}
	size = (len + 3) / sizeof(Uint);
	num[size] = 0;

	tmp = num;
	text += len;
	while (len >= 4) {
	    text -= 4;
	    *tmp++ = (UCHAR(text[0]) << 24) |
		     (UCHAR(text[1]) << 16) |
		     (UCHAR(text[2]) << 8) |
		     UCHAR(text[3]);
	    len -= 4;
	}
	if (len != 0) {
	    text -= len;
	    bits = 0;
	    do {
		bits = (bits << 8) | UCHAR(*text++);
	    } while (--len != 0);
	    *tmp++ = bits;
	}

	return FALSE;
    }
}

/*
 * convert an ASI to a string
 */
String *Asi::numtostr(bool minus)
{
    Uint *n, sz;
    ssizet len;
    Uint bits;
    char *text;
    Uint *tmp;
    bool prefix;
    String *str;

    n = num;
    sz = size;

    /*
     * skip leading zeroes
     */
    while (n[sz - 1] == 0) {
	if (--sz == 0) {
	    /* +0, -0 */
	    return String::create("\0", 1);
	}
    }

    prefix = FALSE;
    if (minus) {
	tmp = ALLOCA(Uint, sz);
	memset(tmp, '\0', sz * sizeof(Uint));
	Asi t(tmp, sz);
	t.sub(*this);
	copy(t);

	n += --sz;
	bits = *n;
	if (sz == 0 && bits == 0xffffffffL) {
	    len = 0;
	} else {
	    /* skip leading 0xff bytes */
	    for (len = 24; UCHAR(bits >> len) == 0xff; len -= 8) ;
	    if (!((bits >> len) & 0x80)) {
		prefix = TRUE;
	    }
	}
    } else {
	/* skip leading 0x00 bytes */
	n += --sz;
	bits = *n;
	for (len = 24; (bits >> len) == 0; len -= 8) ;
	if ((bits >> len) & 0x80) {
	    prefix = TRUE;
	}
    }
    len = (len >> 3) + 1;

    str = String::create((char *) NULL, sz * sizeof(Uint) + len + prefix);
    text = str->text;
    if (prefix) {
	/* extra sign indicator */
	*text++ = (minus) ? 0xff : 0x00;
    }
    do {
	*text++ = bits >> (--len << 3);
    } while (len != 0);
    while (sz != 0) {
	bits = *--n;
	*text++ = bits >> 24;
	*text++ = bits >> 16;
	*text++ = bits >> 8;
	*text++ = bits;
	--sz;
    }

    if (minus) {
	AFREE(tmp);
    }
    return str;
}

/*
 * count ticks for operation, return TRUE if out of ticks
 */
bool ASN::ticks(Frame *f, LPCuint ticks)
{
    f->addTicks(ticks);
    if (f->rlim->ticks < 0) {
	if (f->rlim->noticks) {
	    f->rlim->ticks = LPCINT_MAX;
	} else {
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * add two ASNs
 */
String *ASN::add(Frame *f, String *s1, String *s2, String *s3)
{
    Asi c;
    bool minusa, minusb, minusc;
    String *str;

    Asi mod(ALLOCA(Uint, (s3->len >> 2) + 2), 0);
    if (mod.strtonum(s3) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 4), 0);
    minusa = a.strtonum(s1);
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 4), 0);
    minusb = b.strtonum(s2);
    f->addTicks(4 + ((a.size + b.size + mod.size) >> 1));

    if (minusa != minusb) {
	if (a.size > b.size || (a.size == b.size && a.cmp(b) >= 0)) {
	    /* a + -b, -a + b */
	    c = a;
	    c.sub(b);
	    minusc = minusa;
	} else {
	    /* -b + a, b + -a */
	    c = b;
	    c.sub(a);
	    minusc = minusb;
	}
    } else if (a.size >= b.size) {
	/* a + b, -a + -b */
	c = a;
	c.size++;
	c.add(b);
	minusc = minusa;
    } else {
	/* b + a, -b + -a */
	c = b;
	c.size++;
	c.add(a);
	minusc = minusb;
    }

    if (c.size >= mod.size && c.cmp(mod) >= 0) {
	c.sub(mod);
	if (c.cmp(mod) >= 0) {
	    if (ticks(f, mod.size * (c.size - mod.size + 10))) {
		AFREE(b.num);
		AFREE(a.num);
		AFREE(mod.num);
		EC->error("Out of ticks");
	    }
	    Asi t(ALLOCA(Uint, (mod.size << 1) + 1), 0);
	    c.div(c, mod, t);
	    AFREE(t.num);
	}
    }
    str = c.numtostr(minusc);
    AFREE(b.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * subtract one ASN from another
 */
String *ASN::sub(Frame *f, String *s1, String *s2, String *s3)
{
    Asi c;
    bool minusa, minusb, minusc;
    String *str;

    Asi mod(ALLOCA(Uint, (s3->len >> 2) + 2), 0);
    if (mod.strtonum(s3) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 4), 0);
    minusa = a.strtonum(s1);
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 4), 0);
    minusb = b.strtonum(s2);
    f->addTicks(4 + ((a.size + b.size + mod.size) >> 1));

    if (minusa == minusb) {
	if (a.size > b.size || (a.size == b.size && a.cmp(b) >= 0)) {
	    /* a - b, -a - -b */
	    c = a;
	    c.sub(b);
	    minusc = minusa;
	} else {
	    /* b - a, -b - -a */
	    c = b;
	    c.sub(a);
	    minusc = !minusb;
	}
    } else if (a.size >= b.size) {
	/* a - -b, -a - b */
	c = a;
	c.size++;
	c.add(b);
	minusc = minusa;
    } else {
	/* b - -a, -b - a */
	c = b;
	c.size++;
	c.add(a);
	minusc = !minusb;
    }

    if (c.size >= mod.size && c.cmp(mod) >= 0) {
	c.sub(mod);
	if (c.cmp(mod) >= 0) {
	    if (ticks(f, mod.size * (c.size - mod.size + 10))) {
		AFREE(b.num);
		AFREE(a.num);
		AFREE(mod.num);
		EC->error("Out of ticks");
	    }
	    Asi t(ALLOCA(Uint, (mod.size << 1) + 1), 0);
	    c.div(c, mod, t);
	    AFREE(t.num);
	}
    }
    str = c.numtostr(minusc);
    AFREE(b.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * compare one ASN with another
 */
int ASN::cmp(Frame *f, String *s1, String *s2)
{
    bool minusa, minusb;
    int cmp;

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    minusb = b.strtonum(s2);
    f->addTicks(4 + ((a.size + b.size) >> 1));

    if (minusa != minusb) {
	if (minusa) {
	    cmp = -1;
	} else {
	    cmp = 1;
	}
    } else {
	if (a.size != b.size) {
	    if (a.size < b.size) {
		cmp = -1;
	    } else {
		cmp = 1;
	    }
	} else {
	    cmp = a.cmp(b);
	}
	if (minusa) {
	    cmp = -cmp;
	}
    }

    AFREE(b.num);
    AFREE(a.num);
    return cmp;
}

/*
 * multiply one ASN with another
 */
String *ASN::mult(Frame *f, String *s1, String *s2, String *s3)
{
    Uint size;
    bool minusa, minusb;
    String *str;

    Asi mod(ALLOCA(Uint, (s3->len >> 2) + 2), 0);
    if (mod.strtonum(s3) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    minusb = b.strtonum(s2);
    if (ticks(f, 4 + a.size * b.size)) {
	AFREE(b.num);
	AFREE(a.num);
	AFREE(mod.num);
	EC->error("Out of ticks");
    }

    size = a.size + b.size;
    Asi c(ALLOCA(Uint, size + 1), size);
    memset(c.num, '\0', c.size * sizeof(Uint));
    Asi t(ALLOCA(Uint, (c.size << 1) + c.size), 0);
    c.mult(a, b, t);

    if (c.size >= mod.size && c.cmp(mod) >= 0) {
	if (ticks(f, mod.size * (c.size - mod.size + 10))) {
	    AFREE(t.num);
	    AFREE(c.num);
	    AFREE(b.num);
	    AFREE(a.num);
	    AFREE(mod.num);
	    EC->error("Out of ticks");
	}
	c.div(c, mod, t);
    }
    str = c.numtostr(minusa ^ minusb);
    AFREE(t.num);
    AFREE(c.num);

    AFREE(b.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * divide one ASN by another
 */
String *ASN::div(Frame *f, String *s1, String *s2, String *s3)
{
    bool minusa, minusb;
    String *str;

    Asi mod(ALLOCA(Uint, (s3->len >> 2) + 2), 0);
    if (mod.strtonum(s3) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    minusb = b.strtonum(s2);
    if (b.size == 1 && b.num[0] == 0) {
	AFREE(b.num);
	AFREE(mod.num);
	EC->error("Division by zero");
    }
    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);
    f->addTicks(4 + ((a.size + b.size) >> 1));

    Asi c(ALLOCA(Uint, a.size + 2), 0);
    Asi t(ALLOCA(Uint, (b.size + mod.size) << 1), 0); /* more than enough */
    if (a.size >= b.size && a.cmp(b) >= 0) {
	if (ticks(f, b.size * (a.size - b.size + 10))) {
	    AFREE(t.num);
	    AFREE(c.num);
	    AFREE(a.num);
	    AFREE(b.num);
	    AFREE(mod.num);
	    EC->error("Out of ticks");
	}
	Asi d(c.div(a, b, t), a.size - b.size + 1);
	if (d.size >= mod.size && d.cmp(mod) >= 0) {
	    if (ticks(f, mod.size * (d.size - mod.size + 10))) {
		AFREE(t.num);
		AFREE(c.num);
		AFREE(a.num);
		AFREE(b.num);
		AFREE(mod.num);
		EC->error("Out of ticks");
	    }
	    c.div(d, mod, t);
	} else {
	    c.copy(d);
	}
    } else {
	c.num[0] = 0;
	c.size = 1;
    }
    str = c.numtostr(minusa ^ minusb);
    AFREE(t.num);
    AFREE(c.num);

    AFREE(a.num);
    AFREE(b.num);
    AFREE(mod.num);
    return str;
}

/*
 * take the modulus of an ASN
 */
String *ASN::mod(Frame *f, String *s1, String *s2)
{
    bool minusa;
    String *str;

    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    if (b.strtonum(s2) || (b.size == 1 && b.num[0] == 0)) {
	AFREE(b.num);
	EC->error("Invalid modulus");
    }
    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);

    Asi c(ALLOCA(Uint, a.size + 2), 0);
    Asi t(ALLOCA(Uint, (b.size << 1) + 1), 0);
    if (a.size >= b.size && a.cmp(b) > 0) {
	if (ticks(f, b.size * (a.size - b.size + 10))) {
	    AFREE(t.num);
	    AFREE(c.num);
	    AFREE(a.num);
	    AFREE(b.num);
	    EC->error("Out of ticks");
	}
	c.div(a, b, t);
    } else {
	f->addTicks(4 + a.size + (b.size >> 1));
	c.copy(a);
    }
    str = c.numtostr(minusa);
    AFREE(t.num);
    AFREE(c.num);

    AFREE(a.num);
    AFREE(b.num);
    return str;
}

/*
 * compute a power of an ASN
 */
String *ASN::pow(Frame *f, String *s1, String *s2, String *s3)
{
    LPCuint ticks1, ticks2;
    bool minusa, minusb;
    String *str;

    Asi mod(ALLOCA(Uint, (s3->len >> 2) + 2), 0);
    if (mod.strtonum(s3) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    minusb = b.strtonum(s2);
    ticks1 = mod.size * mod.size;
    ticks2 = ticks1 * b.size;
    if (ticks2 / b.size != ticks1) {
	AFREE(b.num);
	AFREE(a.num);
	AFREE(mod.num);
	EC->error("Out of ticks");
    }
    ticks1 = ticks2 << 5;
    if (ticks1 >> 5 != ticks2 || (LPCint) ticks1 < 0 || ticks(f, ticks1)) {
	AFREE(b.num);
	AFREE(a.num);
	AFREE(mod.num);
	EC->error("Out of ticks");
    }

    Asi c(ALLOCA(Uint, mod.size), mod.size);
    Asi t(ALLOCA(Uint, mod.size << 2), 0);
    if (minusb) {
	/* a ** -b = (a ** -1) ** b */
	if (ticks(f, a.size * (mod.size + 10))) {
	    AFREE(t.num);
	    AFREE(c.num);
	    AFREE(b.num);
	    AFREE(a.num);
	    AFREE(mod.num);
	    EC->error("Out of ticks");
	}
	if (!c.modinv(a, mod)) {
	    AFREE(t.num);
	    AFREE(c.num);
	    AFREE(b.num);
	    AFREE(a.num);
	    AFREE(mod.num);
	    EC->error("No inverse");
	}
	c.power(c, b, mod, t);
    } else {
	c.power(a, b, mod, t);
    }
    str = c.numtostr(minusa & (bool) b.num[0]);
    AFREE(t.num);
    AFREE(c.num);

    AFREE(b.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * modular inverse of an ASN
 */
String *ASN::modinv(Frame *f, String *s1, String *s2)
{
    bool minusa;
    String *str;

    Asi mod(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    if (mod.strtonum(s2) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    Asi a(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
    minusa = a.strtonum(s1);
    if (ticks(f, 4 + a.size * mod.size)) {
	AFREE(a.num);
	AFREE(mod.num);
	EC->error("Out of ticks");
    }
    Asi b(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    if (!b.modinv(a, mod)) {
	AFREE(b.num);
	AFREE(a.num);
	AFREE(mod.num);
	EC->error("No inverse");
    }

    str = b.numtostr(minusa);
    AFREE(b.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * left shift an ASN
 */
String *ASN::lshift(Frame *f, String *s1, LPCint shift, String *s2)
{
    Uint size;
    Asi a, t;
    bool minusa;
    String *str;

    if (shift < 0) {
	EC->error("Negative left shift");
    }
    Asi mod(ALLOCA(Uint, (s2->len >> 2) + 2), 0);
    if (mod.strtonum(s2) || (mod.size == 1 && mod.num[0] == 0)) {
	AFREE(mod.num);
	EC->error("Invalid modulus");
    }

    size = (s1->len >> 2) + 2 + ((shift + 31) >> 5);
    f->addTicks(4 + size + (mod.size >> 1));
    if (size <= mod.size << 2) {
	/*
	 * perform actual left shift
	 */
	a = Asi(ALLOCA(Uint, size), 0);
	t = Asi(ALLOCA(Uint, (mod.size << 1) + 1), 0);
	minusa = a.strtonum(s1);
	if (shift != 0) {
	    a.size += (shift + 31) >> 5;
	    a.lshift(shift);
	}
    } else {
	/*
	 * multiply with 2 ** shift
	 */
	size = (s1->len >> 2) + 4 + mod.size;
	a = Asi(ALLOCA(Uint, size), 1);
	if (size < mod.size << 2) {
	    size = mod.size << 2;
	}
	t = Asi(ALLOCA(Uint, size), 0);
	Asi b(ALLOCA(Uint, mod.size), 1);
	Asi c(ALLOCA(Uint, (s1->len >> 2) + 2), 0);
	minusa = c.strtonum(s1);
	a.num[0] = 2;
	b.num[0] = shift;
	b.power(a, b, mod, t);
	a.mult(b, c, t);
	AFREE(c.num);
	AFREE(b.num);
    }

    if (a.size >= mod.size && a.cmp(mod) >= 0) {
	if (ticks(f, mod.size * (a.size - mod.size + 10))) {
	    AFREE(t.num);
	    AFREE(a.num);
	    AFREE(mod.num);
	    EC->error("Out of ticks");
	}
	a.div(a, mod, t);
    }
    str = a.numtostr(minusa);
    AFREE(t.num);
    AFREE(a.num);
    AFREE(mod.num);
    return str;
}

/*
 * right shift the ASN
 */
String *ASN::rshift(Frame *f, String *s, LPCint shift)
{
    bool minusa;
    String *str;

    if (shift < 0) {
	EC->error("Negative right shift");
    }
    Asi a(ALLOCA(Uint, (s->len >> 2) + 2), 0);
    minusa = a.strtonum(s);
    f->addTicks(4 + a.size);
    if (shift >> 5 >= a.size) {
	a.num[0] = 0;
	a.size = 1;
    } else if (shift != 0) {
	a.rshift(shift);
    }
    str = a.numtostr(minusa);
    AFREE(a.num);

    return str;
}

/*
 * logical and of two strings
 */
String *ASN::_and(Frame *f, String *s1, String *s2)
{
    char *p, *q, *r;
    ssizet i, j;
    String *str;

    if (s1->len < s2->len) {
	i = s1->len;
	j = s2->len - i;
	q = s1->text;
	r = s2->text;
    } else {
	i = s2->len;
	j = s1->len - i;
	q = s2->text;
	r = s1->text;
    }
    f->addTicks(4 + ((i + j) >> 4));
    str = String::create((char *) NULL, (long) i + j);
    p = str->text;
    if (q[0] & 0x80) {
	while (j != 0) {
	    *p++ = *r++;
	    --j;
	}
    } else {
	r += j;
	while (j != 0) {
	    *p++ = '\0';
	    --j;
	}
    }
    while (i != 0) {
	*p++ = *q++ & *r++;
	--i;
    }

    return str;
}

/*
 * logical or of two strings
 */
String *ASN::_or(Frame *f, String *s1, String *s2)
{
    char *p, *q, *r;
    ssizet i, j;
    String *str;

    if (s1->len < s2->len) {
	i = s1->len;
	j = s2->len - i;
	q = s1->text;
	r = s2->text;
    } else {
	i = s2->len;
	j = s1->len - i;
	q = s2->text;
	r = s1->text;
    }
    f->addTicks(4 + ((i + j) >> 4));
    str = String::create((char *) NULL, (long) i + j);
    p = str->text;
    if (q[0] & 0x80) {
	r += j;
	while (j != 0) {
	    *p++ = '\xff';
	    --j;
	}
    } else {
	while (j != 0) {
	    *p++ = *r++;
	    --j;
	}
    }
    while (i != 0) {
	*p++ = *q++ | *r++;
	--i;
    }

    return str;
}

/*
 * logical xor of two strings
 */
String *ASN::_xor(Frame *f, String *s1, String *s2)
{
    char *p, *q, *r;
    ssizet i, j;
    String *str;

    if (s1->len < s2->len) {
	i = s1->len;
	j = s2->len - i;
	q = s1->text;
	r = s2->text;
    } else {
	i = s2->len;
	j = s1->len - i;
	q = s2->text;
	r = s1->text;
    }
    f->addTicks(4 + ((i + j) >> 4));
    str = String::create((char *) NULL, (long) i + j);
    p = str->text;
    if (q[0] & 0x80) {
	while (j != 0) {
	    *p++ = ~*r++;
	    --j;
	}
    } else {
	while (j != 0) {
	    *p++ = *r++;
	    --j;
	}
    }
    while (i != 0) {
	*p++ = *q++ ^ *r++;
	--i;
    }

    return str;
}
