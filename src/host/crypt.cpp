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


/*
 * DES encryption tailored to DGD, small but reasonably fast.
 */

static char Salt[256] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,32,
     16,48, 8,40,24,56, 4,36,20,52,40,24,56, 4,36,20,
     52,12,44,28,60, 2,34,18,50,10,42,26,58, 6,38,22,
     54,14,46,30,62, 1,33,17,49, 9,41, 1,33,17,49, 9,
     41,25,57, 5,37,21,53,13,45,29,61, 3,35,19,51,11,
     43,27,59, 7,39,23,55,15,47,31,63, 0, 0, 0, 0, 0,
};

static char Rot[] = {
    0,1,1,1,1,1,1,0,1,1,1,1,1,1,0,0,
};

static Uint PC2[14][16] = { {
    0x00000000L, 0x00040000L, 0x00002000L, 0x00042000L,
    0x00000001L, 0x00040001L, 0x00002001L, 0x00042001L,
    0x02000000L, 0x02040000L, 0x02002000L, 0x02042000L,
    0x02000001L, 0x02040001L, 0x02002001L, 0x02042001L,
}, {
    0x00000000L, 0x00010000L, 0x00000010L, 0x00010010L,
    0x00000400L, 0x00010400L, 0x00000410L, 0x00010410L,
    0x01000000L, 0x01010000L, 0x01000010L, 0x01010010L,
    0x01000400L, 0x01010400L, 0x01000410L, 0x01010410L,
}, {
    0x00000000L, 0x00080000L, 0x08000000L, 0x08080000L,
    0x00000100L, 0x00080100L, 0x08000100L, 0x08080100L,
    0x00000000L, 0x00080000L, 0x08000000L, 0x08080000L,
    0x00000100L, 0x00080100L, 0x08000100L, 0x08080100L,
}, {
    0x00000000L, 0x00000020L, 0x00000800L, 0x00000820L,
    0x20000000L, 0x20000020L, 0x20000800L, 0x20000820L,
    0x00000002L, 0x00000022L, 0x00000802L, 0x00000822L,
    0x20000002L, 0x20000022L, 0x20000802L, 0x20000822L,
}, {
    0x00000000L, 0x00000004L, 0x00100000L, 0x00100004L,
    0x00000000L, 0x00000004L, 0x00100000L, 0x00100004L,
    0x10000000L, 0x10000004L, 0x10100000L, 0x10100004L,
    0x10000000L, 0x10000004L, 0x10100000L, 0x10100004L,
}, {
    0x00000000L, 0x04000000L, 0x00200000L, 0x04200000L,
    0x00000000L, 0x04000000L, 0x00200000L, 0x04200000L,
    0x00000200L, 0x04000200L, 0x00200200L, 0x04200200L,
    0x00000200L, 0x04000200L, 0x00200200L, 0x04200200L,
}, {
    0x00000000L, 0x00001000L, 0x00000008L, 0x00001008L,
    0x00020000L, 0x00021000L, 0x00020008L, 0x00021008L,
    0x00000000L, 0x00001000L, 0x00000008L, 0x00001008L,
    0x00020000L, 0x00021000L, 0x00020008L, 0x00021008L,
}, {
    0x00000000L, 0x00000001L, 0x08000000L, 0x08000001L,
    0x00002000L, 0x00002001L, 0x08002000L, 0x08002001L,
    0x00000002L, 0x00000003L, 0x08000002L, 0x08000003L,
    0x00002002L, 0x00002003L, 0x08002002L, 0x08002003L,
}, {
    0x00000000L, 0x00000004L, 0x00000000L, 0x00000004L,
    0x00020000L, 0x00020004L, 0x00020000L, 0x00020004L,
    0x00000200L, 0x00000204L, 0x00000200L, 0x00000204L,
    0x00020200L, 0x00020204L, 0x00020200L, 0x00020204L,
}, {
    0x00000000L, 0x00001000L, 0x00080000L, 0x00081000L,
    0x00000000L, 0x00001000L, 0x00080000L, 0x00081000L,
    0x04000000L, 0x04001000L, 0x04080000L, 0x04081000L,
    0x04000000L, 0x04001000L, 0x04080000L, 0x04081000L,
}, {
    0x00000000L, 0x00200000L, 0x00000000L, 0x00200000L,
    0x00000010L, 0x00200010L, 0x00000010L, 0x00200010L,
    0x20000000L, 0x20200000L, 0x20000000L, 0x20200000L,
    0x20000010L, 0x20200010L, 0x20000010L, 0x20200010L,
}, {
    0x00000000L, 0x00000100L, 0x02000000L, 0x02000100L,
    0x00000020L, 0x00000120L, 0x02000020L, 0x02000120L,
    0x00000400L, 0x00000500L, 0x02000400L, 0x02000500L,
    0x00000420L, 0x00000520L, 0x02000420L, 0x02000520L,
}, {
    0x00000000L, 0x10000000L, 0x00000800L, 0x10000800L,
    0x00000008L, 0x10000008L, 0x00000808L, 0x10000808L,
    0x00100000L, 0x10100000L, 0x00100800L, 0x10100800L,
    0x00100008L, 0x10100008L, 0x00100808L, 0x10100808L,
}, {
    0x00000000L, 0x00040000L, 0x01000000L, 0x01040000L,
    0x00000000L, 0x00040000L, 0x01000000L, 0x01040000L,
    0x00010000L, 0x00050000L, 0x01010000L, 0x01050000L,
    0x00010000L, 0x00050000L, 0x01010000L, 0x01050000L,
} };

static Uint SP[8][64] = { {
    0x00101040L, 0x00000000L, 0x00001000L, 0x40101040L,
    0x40101000L, 0x40001040L, 0x40000000L, 0x00001000L,
    0x00000040L, 0x00101040L, 0x40101040L, 0x00000040L,
    0x40100040L, 0x40101000L, 0x00100000L, 0x40000000L,
    0x40000040L, 0x00100040L, 0x00100040L, 0x00001040L,
    0x00001040L, 0x00101000L, 0x00101000L, 0x40100040L,
    0x40001000L, 0x40100000L, 0x40100000L, 0x40001000L,
    0x00000000L, 0x40000040L, 0x40001040L, 0x00100000L,
    0x00001000L, 0x40101040L, 0x40000000L, 0x00101000L,
    0x00101040L, 0x00100000L, 0x00100000L, 0x00000040L,
    0x40101000L, 0x00001000L, 0x00001040L, 0x40100000L,
    0x00000040L, 0x40000000L, 0x40100040L, 0x40001040L,
    0x40101040L, 0x40001000L, 0x00101000L, 0x40100040L,
    0x40100000L, 0x40000040L, 0x40001040L, 0x00101040L,
    0x40000040L, 0x00100040L, 0x00100040L, 0x00000000L,
    0x40001000L, 0x00001040L, 0x00000000L, 0x40101000L,
}, {
    0x08010802L, 0x08000800L, 0x00000800L, 0x00010802L,
    0x00010000L, 0x00000002L, 0x08010002L, 0x08000802L,
    0x08000002L, 0x08010802L, 0x08010800L, 0x08000000L,
    0x08000800L, 0x00010000L, 0x00000002L, 0x08010002L,
    0x00010800L, 0x00010002L, 0x08000802L, 0x00000000L,
    0x08000000L, 0x00000800L, 0x00010802L, 0x08010000L,
    0x00010002L, 0x08000002L, 0x00000000L, 0x00010800L,
    0x00000802L, 0x08010800L, 0x08010000L, 0x00000802L,
    0x00000000L, 0x00010802L, 0x08010002L, 0x00010000L,
    0x08000802L, 0x08010000L, 0x08010800L, 0x00000800L,
    0x08010000L, 0x08000800L, 0x00000002L, 0x08010802L,
    0x00010802L, 0x00000002L, 0x00000800L, 0x08000000L,
    0x00000802L, 0x08010800L, 0x00010000L, 0x08000002L,
    0x00010002L, 0x08000802L, 0x08000002L, 0x00010002L,
    0x00010800L, 0x00000000L, 0x08000800L, 0x00000802L,
    0x08000000L, 0x08010002L, 0x08010802L, 0x00010800L,
}, {
    0x80000020L, 0x00802020L, 0x00000000L, 0x80802000L,
    0x00800020L, 0x00000000L, 0x80002020L, 0x00800020L,
    0x80002000L, 0x80800000L, 0x80800000L, 0x00002000L,
    0x80802020L, 0x80002000L, 0x00802000L, 0x80000020L,
    0x00800000L, 0x80000000L, 0x00802020L, 0x00000020L,
    0x00002020L, 0x00802000L, 0x80802000L, 0x80002020L,
    0x80800020L, 0x00002020L, 0x00002000L, 0x80800020L,
    0x80000000L, 0x80802020L, 0x00000020L, 0x00800000L,
    0x00802020L, 0x00800000L, 0x80002000L, 0x80000020L,
    0x00002000L, 0x00802020L, 0x00800020L, 0x00000000L,
    0x00000020L, 0x80002000L, 0x80802020L, 0x00800020L,
    0x80800000L, 0x00000020L, 0x00000000L, 0x80802000L,
    0x80800020L, 0x00002000L, 0x00800000L, 0x80802020L,
    0x80000000L, 0x80002020L, 0x00002020L, 0x80800000L,
    0x00802000L, 0x80800020L, 0x80000020L, 0x00802000L,
    0x80002020L, 0x80000000L, 0x80802000L, 0x00002020L,
}, {
    0x10080200L, 0x10000208L, 0x10000208L, 0x00000008L,
    0x00080208L, 0x10080008L, 0x10080000L, 0x10000200L,
    0x00000000L, 0x00080200L, 0x00080200L, 0x10080208L,
    0x10000008L, 0x00000000L, 0x00080008L, 0x10080000L,
    0x10000000L, 0x00000200L, 0x00080000L, 0x10080200L,
    0x00000008L, 0x00080000L, 0x10000200L, 0x00000208L,
    0x10080008L, 0x10000000L, 0x00000208L, 0x00080008L,
    0x00000200L, 0x00080208L, 0x10080208L, 0x10000008L,
    0x00080008L, 0x10080000L, 0x00080200L, 0x10080208L,
    0x10000008L, 0x00000000L, 0x00000000L, 0x00080200L,
    0x00000208L, 0x00080008L, 0x10080008L, 0x10000000L,
    0x10080200L, 0x10000208L, 0x10000208L, 0x00000008L,
    0x10080208L, 0x10000008L, 0x10000000L, 0x00000200L,
    0x10080000L, 0x10000200L, 0x00080208L, 0x10080008L,
    0x10000200L, 0x00000208L, 0x00080000L, 0x10080200L,
    0x00000008L, 0x00080000L, 0x00000200L, 0x00080208L,
}, {
    0x00000010L, 0x00208010L, 0x00208000L, 0x04200010L,
    0x00008000L, 0x00000010L, 0x04000000L, 0x00208000L,
    0x04008010L, 0x00008000L, 0x00200010L, 0x04008010L,
    0x04200010L, 0x04208000L, 0x00008010L, 0x04000000L,
    0x00200000L, 0x04008000L, 0x04008000L, 0x00000000L,
    0x04000010L, 0x04208010L, 0x04208010L, 0x00200010L,
    0x04208000L, 0x04000010L, 0x00000000L, 0x04200000L,
    0x00208010L, 0x00200000L, 0x04200000L, 0x00008010L,
    0x00008000L, 0x04200010L, 0x00000010L, 0x00200000L,
    0x04000000L, 0x00208000L, 0x04200010L, 0x04008010L,
    0x00200010L, 0x04000000L, 0x04208000L, 0x00208010L,
    0x04008010L, 0x00000010L, 0x00200000L, 0x04208000L,
    0x04208010L, 0x00008010L, 0x04200000L, 0x04208010L,
    0x00208000L, 0x00000000L, 0x04008000L, 0x04200000L,
    0x00008010L, 0x00200010L, 0x04000010L, 0x00008000L,
    0x00000000L, 0x04008000L, 0x00208010L, 0x04000010L,
}, {
    0x02000001L, 0x02040000L, 0x00000400L, 0x02040401L,
    0x02040000L, 0x00000001L, 0x02040401L, 0x00040000L,
    0x02000400L, 0x00040401L, 0x00040000L, 0x02000001L,
    0x00040001L, 0x02000400L, 0x02000000L, 0x00000401L,
    0x00000000L, 0x00040001L, 0x02000401L, 0x00000400L,
    0x00040400L, 0x02000401L, 0x00000001L, 0x02040001L,
    0x02040001L, 0x00000000L, 0x00040401L, 0x02040400L,
    0x00000401L, 0x00040400L, 0x02040400L, 0x02000000L,
    0x02000400L, 0x00000001L, 0x02040001L, 0x00040400L,
    0x02040401L, 0x00040000L, 0x00000401L, 0x02000001L,
    0x00040000L, 0x02000400L, 0x02000000L, 0x00000401L,
    0x02000001L, 0x02040401L, 0x00040400L, 0x02040000L,
    0x00040401L, 0x02040400L, 0x00000000L, 0x02040001L,
    0x00000001L, 0x00000400L, 0x02040000L, 0x00040401L,
    0x00000400L, 0x00040001L, 0x02000401L, 0x00000000L,
    0x02040400L, 0x02000000L, 0x00040001L, 0x02000401L,
}, {
    0x00020000L, 0x20420000L, 0x20400080L, 0x00000000L,
    0x00000080L, 0x20400080L, 0x20020080L, 0x00420080L,
    0x20420080L, 0x00020000L, 0x00000000L, 0x20400000L,
    0x20000000L, 0x00400000L, 0x20420000L, 0x20000080L,
    0x00400080L, 0x20020080L, 0x20020000L, 0x00400080L,
    0x20400000L, 0x00420000L, 0x00420080L, 0x20020000L,
    0x00420000L, 0x00000080L, 0x20000080L, 0x20420080L,
    0x00020080L, 0x20000000L, 0x00400000L, 0x00020080L,
    0x00400000L, 0x00020080L, 0x00020000L, 0x20400080L,
    0x20400080L, 0x20420000L, 0x20420000L, 0x20000000L,
    0x20020000L, 0x00400000L, 0x00400080L, 0x00020000L,
    0x00420080L, 0x20000080L, 0x20020080L, 0x00420080L,
    0x20000080L, 0x20400000L, 0x20420080L, 0x00420000L,
    0x00020080L, 0x00000000L, 0x20000000L, 0x20420080L,
    0x00000000L, 0x20020080L, 0x00420000L, 0x00000080L,
    0x20400000L, 0x00400080L, 0x00000080L, 0x20020000L,
}, {
    0x01000104L, 0x00000100L, 0x00004000L, 0x01004104L,
    0x01000000L, 0x01000104L, 0x00000004L, 0x01000000L,
    0x00004004L, 0x01004000L, 0x01004104L, 0x00004100L,
    0x01004100L, 0x00004104L, 0x00000100L, 0x00000004L,
    0x01004000L, 0x01000004L, 0x01000100L, 0x00000104L,
    0x00004100L, 0x00004004L, 0x01004004L, 0x01004100L,
    0x00000104L, 0x00000000L, 0x00000000L, 0x01004004L,
    0x01000004L, 0x01000100L, 0x00004104L, 0x00004000L,
    0x00004104L, 0x00004000L, 0x01004100L, 0x00000100L,
    0x00000004L, 0x01004004L, 0x00000100L, 0x00004104L,
    0x01000100L, 0x00000004L, 0x01000004L, 0x01004000L,
    0x01004004L, 0x01000000L, 0x00004000L, 0x01000104L,
    0x00000000L, 0x01004104L, 0x00004004L, 0x01000004L,
    0x01004000L, 0x01000100L, 0x01000104L, 0x00000000L,
    0x01004104L, 0x00004100L, 0x00004100L, 0x00000104L,
    0x00000104L, 0x00004004L, 0x01000000L, 0x01004100L,
} };

static char out[64] = {
    '.', '/', '0', '1', '2', '3', '4', '5',
    '6', '7', '8', '9', 'A', 'B', 'C', 'D',
    'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b',
    'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r',
    's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
};

# define EXG1(a, t, n, m)	((t) = (((a) >> (n)) ^ (a)) & (m), \
				 (a) ^= (t), (a) ^= ((t) << (n)))
# define EXG2(a, b, t, n, m)	((t) = (((a) >> (n)) ^ (b)) & (m), \
				 (b) ^= (t), (a) ^= (t) << (n))

/*
 * prepare a key for encryption or decryption
 */
static void P_setkey(Uint *keys, char *k)
{
    Uint L, R, A, B, T;
    int i;

    L  = UCHAR(*k++ << 1); L <<= 8;
    L |= UCHAR(*k++ << 1); L <<= 8;
    L |= UCHAR(*k++ << 1); L <<= 8;
    L |= UCHAR(*k++ << 1);
    R  = UCHAR(*k++ << 1); R <<= 8;
    R |= UCHAR(*k++ << 1); R <<= 8;
    R |= UCHAR(*k++ << 1); R <<= 8;
    R |= UCHAR(*k << 1);

    /* PC1 */
    EXG2(R, L, T,  4, 0x0f0f0f0fL);
    EXG1(L,    T, 14,     0xccccL);
    EXG1(L,    T,  7,   0xaa00aaL);
    EXG1(L,    T,  4, 0x0f0f0f0fL);
    EXG1(L,    T,  2, 0x33333333L);
    EXG1(L,    T,  1, 0x55555555L);
    EXG1(R,    T, 18,     0x3333L);
    EXG1(R,    T,  9,   0x550055L);
    EXG1(R,    T,  4, 0x0f0f0f0fL);
    R = (R << 4) | (L & 0xf);
    L >>= 4;

    /* PC2 */
    for (i = 15; i >= 0; --i) {
	if (Rot[i] == 0) {
	    L = (L << 1) | (L >> 27);
	    R = (R << 1) | (R >> 27);
	} else {
	    L = (L << 2) | (L >> 26);
	    R = (R << 2) | (R >> 26);
	}
	L &= 0x0fffffff;
	R &= 0x0fffffff;

	A = PC2[ 0][ L >> 24       ] |
	    PC2[ 1][(L >> 20) & 0xf] |
	    PC2[ 2][(L >> 16) & 0xf] |
	    PC2[ 3][(L >> 12) & 0xf] |
	    PC2[ 4][(L >>  8) & 0xf] |
	    PC2[ 5][(L >>  4) & 0xf] |
	    PC2[ 6][ L        & 0xf];
	B = PC2[ 7][ R >> 24       ] |
	    PC2[ 8][(R >> 20) & 0xf] |
	    PC2[ 9][(R >> 16) & 0xf] |
	    PC2[10][(R >> 12) & 0xf] |
	    PC2[11][(R >>  8) & 0xf] |
	    PC2[12][(R >>  4) & 0xf] |
	    PC2[13][ R        & 0xf];
	EXG2(B, A, T, 16, 0x3f3fL);
	*keys++ = A;
	*keys++ = B;
    }
}

/*
 * Unix password encryption.
 */
char *P_crypt(char *passwd, char *salt)
{
    static char result[14];
    Uint L, R, T, X, *key;
    int i, j;
    char *p;
    Uint keys[32], E[2];

    /* fetch password data */
    memset(result, '\0', 8);
    strncpy(result, passwd, 8);
    P_setkey(keys, result);

    /* prepare salt */
    E[0] = UCHAR(Salt[UCHAR(result[0] = *salt++)]) << 8;
    E[1] = UCHAR(Salt[UCHAR(result[1] = *salt++)]) << 4;

    /* encrypt */
    L = 0;
    R = 0;
    for (i = 25; i > 0; --i) {
	key = keys;
	for (j = 7; j >= 0; --j) {
	    T = R ^ (R >> 16);
	    X = T & E[0];
	    X ^= (X << 16) ^ R ^ *key++;
	    L ^= SP[6][X & 0x3f]; X >>= 8;
	    L ^= SP[4][X & 0x3f]; X >>= 8;
	    L ^= SP[2][X & 0x3f]; X >>= 8;
	    L ^= SP[0][X & 0x3f];
	    X = T & E[1];
	    X ^= (X << 16) ^ R;
	    X = ((X << 4) | (X >> 28)) ^ *key++;
	    L ^= SP[7][X & 0x3f]; X >>= 8;
	    L ^= SP[5][X & 0x3f]; X >>= 8;
	    L ^= SP[3][X & 0x3f]; X >>= 8;
	    L ^= SP[1][X & 0x3f];
	    T = L ^ (L >> 16);
	    X = T & E[0];
	    X ^= (X << 16) ^ L ^ *key++;
	    R ^= SP[6][X & 0x3f]; X >>= 8;
	    R ^= SP[4][X & 0x3f]; X >>= 8;
	    R ^= SP[2][X & 0x3f]; X >>= 8;
	    R ^= SP[0][X & 0x3f];
	    X = T & E[1];
	    X ^= (X << 16) ^ L;
	    X = ((X << 4) | (X >> 28)) ^ *key++;
	    R ^= SP[7][X & 0x3f]; X >>= 8;
	    R ^= SP[5][X & 0x3f]; X >>= 8;
	    R ^= SP[3][X & 0x3f]; X >>= 8;
	    R ^= SP[1][X & 0x3f];
	}
	T = L;
	L = R;
	R = T;
    }
    L = (L << 3) | (L >> 29);
    R = (R << 3) | (R >> 29);

    /* FP */
    EXG2(L, R, T,  1, 0x55555555L);
    EXG2(R, L, T,  8,   0xff00ffL);
    EXG2(R, L, T,  2, 0x33333333L);
    EXG2(L, R, T, 16,     0xffffL);
    EXG2(L, R, T,  4, 0x0f0f0f0fL);

    /* put result in static buffer */
    p = result + 13;
    *p = '\0';
    *--p = out[(R << 2) & 0x3f]; R >>= 4;
    *--p = out[R & 0x3f]; R >>= 6;
    *--p = out[R & 0x3f]; R >>= 6;
    *--p = out[R & 0x3f]; R >>= 6;
    *--p = out[R & 0x3f]; R >>= 6;
    *--p = out[((L << 4) | R) & 0x3f]; L >>= 2;
    *--p = out[L & 0x3f]; L >>= 6;
    *--p = out[L & 0x3f]; L >>= 6;
    *--p = out[L & 0x3f]; L >>= 6;
    *--p = out[L & 0x3f]; L >>= 6;
    *--p = out[L & 0x3f];

    return result;
}

/*
 * return a DES key prepared for encryption
 */
String *P_encrypt_des_key(Frame *f, String *keystr)
{
    Uint k, *key;
    char *p;
    int i;
    Uint keys[32];
    String *str;

    if (keystr->len != 8) {
	EC->error("Wrong key length");
    }
    f->addTicks(60);

    P_setkey(keys, keystr->text);
    str = String::create((char *) NULL, 32 * sizeof(Uint));
    p = str->text;
    key = keys;
    for (i = 31; i >= 0; --i) {
	k = *key++;
	*p++ = k >> 24;
	*p++ = k >> 16;
	*p++ = k >> 8;
	*p++ = k;
    }

    return str;
}

/*
 * return a DES key prepared for decryption
 */
String *P_decrypt_des_key(Frame *f, String *keystr)
{
    Uint k, *key;
    char *p;
    int i;
    Uint keys[32];
    String *str;

    if (keystr->len != 8) {
	EC->error("Wrong key length");
    }
    f->addTicks(60);

    P_setkey(keys, keystr->text);
    str = String::create((char *) NULL, 32 * sizeof(Uint));
    p = str->text;
    key = keys + 32;
    for (i = 15; i >= 0; --i) {
	key -= 2;
	k = key[0];
	*p++ = k >> 24;
	*p++ = k >> 16;
	*p++ = k >> 8;
	*p++ = k;
	k = key[1];
	*p++ = k >> 24;
	*p++ = k >> 16;
	*p++ = k >> 8;
	*p++ = k;
    }

    return str;
}

/*
 * encrypt (or decrypt) a string
 */
String *P_encrypt_des(Frame *f, String *keystr, String *mesg)
{
    Uint L, R, T, *key;
    char *p, *q;
    int i;
    ssizet len;
    Uint keys[32];
    String *str;

    if (keystr->len != 32 * sizeof(Uint)) {
	EC->error("Wrong key length");
    }
    f->addTicks(mesg->len * 5L);
    if (f->rlim->ticks < 0) {
	if (f->rlim->noticks) {
	    f->rlim->ticks = LPCINT_MAX;
	} else {
	    EC->error("Out of ticks");
	}
    }
    str = String::create((char *) NULL, ((long) mesg->len + 7) & ~7);

    p = keystr->text;
    key = keys;
    for (i = 31; i >= 0; --i) {
	T  = UCHAR(*p++); T <<= 8;
	T |= UCHAR(*p++); T <<= 8;
	T |= UCHAR(*p++); T <<= 8;
	T |= UCHAR(*p++);
	*key++ = T;
    }

    p = mesg->text;
    q = str->text;

    for (len = mesg->len; len >= 8; len -= 8) {
	L  = UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++);
	R  = UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++);

	/* IP */
	EXG2(L, R, T,  4, 0x0f0f0f0fL);
	EXG2(L, R, T, 16,     0xffffL);
	EXG2(R, L, T,  2, 0x33333333L);
	EXG2(R, L, T,  8,   0xff00ffL);
	EXG2(L, R, T,  1, 0x55555555L);

	key = keys;
	L = (L >> 3) | (L << 29);
	R = (R >> 3) | (R << 29);
	for (i = 7; i >= 0; --i) {
	    T = R ^ *key++;
	    L ^= SP[6][T & 0x3f]; T >>= 8;
	    L ^= SP[4][T & 0x3f]; T >>= 8;
	    L ^= SP[2][T & 0x3f]; T >>= 8;
	    L ^= SP[0][T & 0x3f];
	    T = ((R << 4) | (R >> 28)) ^ *key++;
	    L ^= SP[7][T & 0x3f]; T >>= 8;
	    L ^= SP[5][T & 0x3f]; T >>= 8;
	    L ^= SP[3][T & 0x3f]; T >>= 8;
	    L ^= SP[1][T & 0x3f];
	    T = L ^ *key++;
	    R ^= SP[6][T & 0x3f]; T >>= 8;
	    R ^= SP[4][T & 0x3f]; T >>= 8;
	    R ^= SP[2][T & 0x3f]; T >>= 8;
	    R ^= SP[0][T & 0x3f];
	    T = ((L << 4) | (L >> 28)) ^ *key++;
	    R ^= SP[7][T & 0x3f]; T >>= 8;
	    R ^= SP[5][T & 0x3f]; T >>= 8;
	    R ^= SP[3][T & 0x3f]; T >>= 8;
	    R ^= SP[1][T & 0x3f];
	}
	L = (L << 3) | (L >> 29);
	R = (R << 3) | (R >> 29);

	/* FP */
	EXG2(R, L, T,  1, 0x55555555L);
	EXG2(L, R, T,  8,   0xff00ffL);
	EXG2(L, R, T,  2, 0x33333333L);
	EXG2(R, L, T, 16,     0xffffL);
	EXG2(R, L, T,  4, 0x0f0f0f0fL);

	*q++ = R >> 24;
	*q++ = R >> 16;
	*q++ = R >> 8;
	*q++ = R;
	*q++ = L >> 24;
	*q++ = L >> 16;
	*q++ = L >> 8;
	*q++ = L;
    }

    if (len != 0) {
	char buffer[8];

	memset(buffer, '\0', 8);
	memcpy(buffer, p, len);
	p = buffer;

	L  = UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++); L <<= 8;
	L |= UCHAR(*p++);
	R  = UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++); R <<= 8;
	R |= UCHAR(*p++);

	/* IP */
	EXG2(L, R, T,  4, 0x0f0f0f0fL);
	EXG2(L, R, T, 16,     0xffffL);
	EXG2(R, L, T,  2, 0x33333333L);
	EXG2(R, L, T,  8,   0xff00ffL);
	EXG2(L, R, T,  1, 0x55555555L);

	key = keys;
	L = (L >> 3) | (L << 29);
	R = (R >> 3) | (R << 29);
	for (i = 7; i >= 0; --i) {
	    T = R ^ *key++;
	    L ^= SP[6][T & 0x3f]; T >>= 8;
	    L ^= SP[4][T & 0x3f]; T >>= 8;
	    L ^= SP[2][T & 0x3f]; T >>= 8;
	    L ^= SP[0][T & 0x3f];
	    T = ((R << 4) | (R >> 28)) ^ *key++;
	    L ^= SP[7][T & 0x3f]; T >>= 8;
	    L ^= SP[5][T & 0x3f]; T >>= 8;
	    L ^= SP[3][T & 0x3f]; T >>= 8;
	    L ^= SP[1][T & 0x3f];
	    T = L ^ *key++;
	    R ^= SP[6][T & 0x3f]; T >>= 8;
	    R ^= SP[4][T & 0x3f]; T >>= 8;
	    R ^= SP[2][T & 0x3f]; T >>= 8;
	    R ^= SP[0][T & 0x3f];
	    T = ((L << 4) | (L >> 28)) ^ *key++;
	    R ^= SP[7][T & 0x3f]; T >>= 8;
	    R ^= SP[5][T & 0x3f]; T >>= 8;
	    R ^= SP[3][T & 0x3f]; T >>= 8;
	    R ^= SP[1][T & 0x3f];
	}
	L = (L << 3) | (L >> 29);
	R = (R << 3) | (R >> 29);

	/* FP */
	EXG2(R, L, T,  1, 0x55555555L);
	EXG2(L, R, T,  8,   0xff00ffL);
	EXG2(L, R, T,  2, 0x33333333L);
	EXG2(R, L, T, 16,     0xffffL);
	EXG2(R, L, T,  4, 0x0f0f0f0fL);

	*q++ = R >> 24;
	*q++ = R >> 16;
	*q++ = R >> 8;
	*q++ = R;
	*q++ = L >> 24;
	*q++ = L >> 16;
	*q++ = L >> 8;
	*q++ = L;
    }

    return str;
}
