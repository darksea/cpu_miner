/*
 * Copyright 2013 gerko.deroo@kangaderoo.nl
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <immintrin.h>
#include "cpuminer-config.h"
#include "miner.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>


static const uint32_t sha256_h_sidm[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t sha256_k_sidm[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const uint32_t keypad_sidm[12] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00000280
};
static const uint32_t innerpad_sidm[11] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x000004a0
};
static const uint32_t outerpad_sidm[8] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0x00000300
};


static const uint32_t finalblk_sidm[16 * 4] __attribute__((aligned(16))) = {
	0x00000001, 0x00000001, 0x00000001, 0x00000001,
	0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0x00000620, 0x00000620, 0x00000620, 0x00000620
};


static inline void xor_salsa_sidm(__m128i *calc_18, __m128i *calc_13, __m128i *calc_9, __m128i *calc_7,
 								  const __m128i *calc_1, const __m128i *calc_4, const __m128i *calc_3, const __m128i *calc_2)
{
	int i;
	__m128i _calc;
	__m128i _shift_left;
	__m128i row1 = _mm_xor_si128(*calc_18, *calc_1);;
	__m128i row2 = _mm_xor_si128(*calc_7, *calc_2);;
	__m128i row3 = _mm_xor_si128(*calc_9, *calc_3);;
	__m128i row4 = _mm_xor_si128(*calc_13, *calc_4);;

	*calc_18 = _mm_xor_si128(*calc_18, *calc_1);
	*calc_7 = _mm_xor_si128(*calc_7, *calc_2);
	*calc_9 = _mm_xor_si128(*calc_9, *calc_3);
	*calc_13 = _mm_xor_si128(*calc_13, *calc_4);

	for (i = 0; i < 8; i += 2) {
		/* first row */
 		_calc = _mm_add_epi32(row1, row4);
		_shift_left = _mm_slli_epi32(_calc, 7);
		_calc = _mm_srli_epi32(_calc,(32 - 7));
		row2 = _mm_xor_si128(row2, _calc);
		row2 = _mm_xor_si128(row2, _shift_left);

		/* second row */
		_calc = _mm_add_epi32(row2, row1);
		_shift_left = _mm_slli_epi32(_calc, 9);
		_calc = _mm_srli_epi32(_calc,(32 - 9));
		row3 = _mm_xor_si128(row3, _calc);
		row3 = _mm_xor_si128(row3, _shift_left);

		/* third row */
		_calc = _mm_add_epi32(row3, row2);
		_shift_left = _mm_slli_epi32(_calc, 13);
		_calc = _mm_srli_epi32(_calc,(32 - 13));
		row4 = _mm_xor_si128(row4, _calc);
		row4 = _mm_xor_si128(row4, _shift_left);

		/* fourth row */
		_calc = _mm_add_epi32(row4, row3);
		_shift_left = _mm_slli_epi32(_calc, 18);
		_calc = _mm_srli_epi32(_calc,(32 - 18));
		row1 = _mm_xor_si128(row1, _calc);
		row1 = _mm_xor_si128(row1, _shift_left);

	// transpose_matrix(row1, row2, row3, row4, row_to_column);
		row2 = _mm_shuffle_epi32(row2,0x93);
		row3 = _mm_shuffle_epi32(row3,0x4e);
		row4 = _mm_shuffle_epi32(row4,0x39);
	// end transpose

		// switch *calc_13 and * calc_7 usage compared to rows
		/* first column */
		_calc = _mm_add_epi32(row1, row2);
		_shift_left = _mm_slli_epi32(_calc, 7);
		_calc = _mm_srli_epi32(_calc,(32 - 7));
		row4 = _mm_xor_si128(row4, _calc);
		row4 = _mm_xor_si128(row4, _shift_left);

		/* second column */
		_calc = _mm_add_epi32(row4, row1);
		_shift_left = _mm_slli_epi32(_calc, 9);
		_calc = _mm_srli_epi32(_calc,(32 - 9));
		row3 = _mm_xor_si128(row3, _calc);
		row3 = _mm_xor_si128(row3, _shift_left);

		/* third column */
		_calc = _mm_add_epi32(row3, row4);
		_shift_left = _mm_slli_epi32(_calc, 13);
		_calc = _mm_srli_epi32(_calc,(32 - 13));
		row2 = _mm_xor_si128(row2, _calc);
		row2 = _mm_xor_si128(row2, _shift_left);

		/* fourth column */
		_calc = _mm_add_epi32(row2, row3);
		_shift_left = _mm_slli_epi32(_calc, 18);
		_calc = _mm_srli_epi32(_calc,(32 - 18));
		row1 = _mm_xor_si128(row1, _calc);
		row1 = _mm_xor_si128(row1, _shift_left);

	// transpose_matrix(row1, row2, row3, row4, row_to_column);
		row2 = _mm_shuffle_epi32(row2,0x39);
		row3 = _mm_shuffle_epi32(row3,0x4e);
		row4 = _mm_shuffle_epi32(row4,0x93);
	// end transpose
	}
	*calc_18 = _mm_add_epi32(*calc_18,row1);
	*calc_7 = _mm_add_epi32(*calc_7, row2);
	*calc_9 = _mm_add_epi32(*calc_9, row3);
	*calc_13 = _mm_add_epi32(*calc_13, row4);
}

static inline void scrypt_core_sidm(uint32_t *X /*, uint32_t *V*/)
{
	uint32_t i, j, k;

	__m128i *SourcePtr = (__m128i*) X;
	uint32_t row1[4] __attribute__((aligned(16))); // = {X[0], X[1], X[2], X[3]};
	uint32_t row2[4] __attribute__((aligned(16))); // = {X[4], X[5], X[6], X[7]};
	uint32_t row3[4] __attribute__((aligned(16))); // = {X[8], X[9], X[10], X[11]};
	uint32_t row4[4] __attribute__((aligned(16))); // = {X[12], X[13], X[14], X[15]};

	uint32_t row11[4] __attribute__((aligned(16))); // = {X[16], X[17], X[18], X[19]};
	uint32_t row21[4] __attribute__((aligned(16))); // = {X[20], X[21], X[22], X[23]};
	uint32_t row31[4] __attribute__((aligned(16))); // = {X[24], X[25], X[26], X[27]};
	uint32_t row41[4] __attribute__((aligned(16))); // = {X[28], X[29], X[30], X[31]};

	__m128i scratch[1024 * 8];

	__m128i *calc_1 = (__m128i*) row1;
	__m128i *calc_2 = (__m128i*) row2;
	__m128i *calc_3 = (__m128i*) row3;
	__m128i *calc_4 = (__m128i*) row4;

	*calc_1 = SourcePtr[0]; //(__m128i*) row1;
	*calc_2 = SourcePtr[1]; //(__m128i*) row1;
	*calc_3 = SourcePtr[2]; //(__m128i*) row1;
	*calc_4 = SourcePtr[3]; //(__m128i*) row1;

	__m128i *calc_11 = (__m128i*) row11;
	__m128i *calc_21 = (__m128i*) row21;
	__m128i *calc_31 = (__m128i*) row31;
	__m128i *calc_41 = (__m128i*) row41;

	*calc_11 = SourcePtr[4]; //(__m128i*) row1;
	*calc_21 = SourcePtr[5]; //(__m128i*) row1;
	*calc_31 = SourcePtr[6]; //(__m128i*) row1;
	*calc_41 = SourcePtr[7]; //(__m128i*) row1;

	__m128i _calc5;
	__m128i _calc6;
	__m128i _calc7;
	__m128i _calc8;

    // __m128i *scratchPrt = (__m128i*) scratch;

	/* transpose the data from *X */
	_calc5 =_mm_blend_epi16(*calc_11, *calc_31, 0xf0);
	_calc6 =_mm_blend_epi16(*calc_21, *calc_41, 0x0f);
	_calc7 =_mm_blend_epi16(*calc_31, *calc_11, 0xf0);
	_calc8 =_mm_blend_epi16(*calc_41, *calc_21, 0x0f);
	*calc_11 = _mm_blend_epi16(_calc5, _calc8, 0xcc);
	*calc_21 = _mm_blend_epi16(_calc6, _calc5, 0xcc);
	*calc_31 = _mm_blend_epi16(_calc7, _calc6, 0xcc);
	*calc_41 = _mm_blend_epi16(_calc8, _calc7, 0xcc);

	_calc5 =_mm_blend_epi16(*calc_1, *calc_3, 0xf0);
	_calc6 =_mm_blend_epi16(*calc_2, *calc_4, 0x0f);
	_calc7 =_mm_blend_epi16(*calc_3, *calc_1, 0xf0);
	_calc8 =_mm_blend_epi16(*calc_4, *calc_2, 0x0f);
	*calc_1 = _mm_blend_epi16(_calc5, _calc8, 0xcc);
	*calc_2 = _mm_blend_epi16(_calc6, _calc5, 0xcc);
	*calc_3 = _mm_blend_epi16(_calc7, _calc6, 0xcc);
	*calc_4 = _mm_blend_epi16(_calc8, _calc7, 0xcc);

	for (i = 0; i < 1024; i++) {
		scratch[i * 8 + 0] = *calc_1;
		scratch[i * 8 + 1] = *calc_2;
		scratch[i * 8 + 2] = *calc_3;
		scratch[i * 8 + 3] = *calc_4;
		scratch[i * 8 + 4] = *calc_11;
		scratch[i * 8 + 5] = *calc_21;
		scratch[i * 8 + 6] = *calc_31;
		scratch[i * 8 + 7] = *calc_41;

		xor_salsa_sidm(calc_1, calc_2, calc_3, calc_4,calc_11,calc_21,calc_31,calc_41);
		xor_salsa_sidm(calc_11,calc_21,calc_31,calc_41,calc_1, calc_2, calc_3, calc_4);
	}
	for (i = 0; i < 1024; i++) {
//		__m128i *scratchprt = scratch[ 8 * (_mm_extract_epi16(*calc_11,0x00) & 1023)];
		j = 8 * (_mm_extract_epi16(*calc_11,0x00) & 1023);
//		j = 8 * (row11[0] & 1023);
//      this section still compiles to too much instructions (about 14 in excess)
//      called 1024 times......
		*calc_1 = _mm_xor_si128(*calc_1, scratch[j+0]);
		*calc_2 = _mm_xor_si128(*calc_2, scratch[j+1]);
		*calc_3 = _mm_xor_si128(*calc_3, scratch[j+2]);
		*calc_4 = _mm_xor_si128(*calc_4, scratch[j+3]);
		*calc_11 = _mm_xor_si128(*calc_11, scratch[j+4]);
		*calc_21 = _mm_xor_si128(*calc_21, scratch[j+5]);
		*calc_31 = _mm_xor_si128(*calc_31, scratch[j+6]);
		*calc_41 = _mm_xor_si128(*calc_41, scratch[j+7]);

		xor_salsa_sidm(calc_1, calc_2, calc_3, calc_4,calc_11,calc_21,calc_31,calc_41);
		xor_salsa_sidm(calc_11,calc_21,calc_31,calc_41,calc_1, calc_2, calc_3, calc_4);
	}

	_calc5 =_mm_blend_epi16(*calc_11, *calc_31, 0xf0);
	_calc6 =_mm_blend_epi16(*calc_21, *calc_41, 0x0f);
	_calc7 =_mm_blend_epi16(*calc_31, *calc_11, 0xf0);
	_calc8 =_mm_blend_epi16(*calc_41, *calc_21, 0x0f);
	*calc_11 = _mm_blend_epi16(_calc5, _calc8, 0xcc);
	*calc_21 = _mm_blend_epi16(_calc6, _calc5, 0xcc);
	*calc_31 = _mm_blend_epi16(_calc7, _calc6, 0xcc);
	*calc_41 = _mm_blend_epi16(_calc8, _calc7, 0xcc);

	_calc5 =_mm_blend_epi16(*calc_1, *calc_3, 0xf0);
	_calc6 =_mm_blend_epi16(*calc_2, *calc_4, 0x0f);
	_calc7 =_mm_blend_epi16(*calc_3, *calc_1, 0xf0);
	_calc8 =_mm_blend_epi16(*calc_4, *calc_2, 0x0f);
	*calc_1 = _mm_blend_epi16(_calc5, _calc8, 0xcc);
	*calc_2 = _mm_blend_epi16(_calc6, _calc5, 0xcc);
	*calc_3 = _mm_blend_epi16(_calc7, _calc6, 0xcc);
	*calc_4 = _mm_blend_epi16(_calc8, _calc7, 0xcc);

    SourcePtr[0] = *calc_1; //(__m128i*) row1;
    SourcePtr[1] = *calc_2; //(__m128i*) row1;
    SourcePtr[2] = *calc_3; //(__m128i*) row1;
    SourcePtr[3] = *calc_4; //(__m128i*) row1;

    SourcePtr[4] = *calc_11; //(__m128i*) row1;
    SourcePtr[5] = *calc_21; //(__m128i*) row1;
    SourcePtr[6] = *calc_31; //(__m128i*) row1;
    SourcePtr[7] = *calc_41; //(__m128i*) row1;


//	X[0] =  row1[0];  X[1]= row1[1];  X[2]= row1[2];  X[3]= row1[3];
//	X[4] =  row2[0];  X[5]= row2[1];  X[6]= row2[2];  X[7]= row2[3];
//	X[8] =  row3[0];  X[9]= row3[1]; X[10]= row3[2]; X[11]= row3[3];
//	X[12] = row4[0]; X[13]= row4[1]; X[14]= row4[2]; X[15]= row4[3];

//	X[16] = row11[0]; X[17]= row11[1]; X[18]= row11[2]; X[19]= row11[3];
//	X[20] = row21[0]; X[21]= row21[1]; X[22]= row21[2]; X[23]= row21[3];
//	X[24] = row31[0]; X[25]= row31[1]; X[26]= row31[2]; X[27]= row31[3];
//	X[28] = row41[0]; X[29]= row41[1]; X[30]= row41[2]; X[31]= row41[3];

}



//#define S0(x)           (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
__m128i funct_S0(const __m128i *x)
{
        __m128i rot2 = _mm_slli_epi32(*x, 2);
        __m128i _calc = _mm_srli_epi32(*x,(32 - 2));

        __m128i rot13 = _mm_slli_epi32(*x, 13);
        rot13 = _mm_xor_si128(rot13, _calc);
        _calc = _mm_srli_epi32(*x,(32 - 13));

        __m128i rot22 = _mm_slli_epi32(*x, 22);
        rot22 = _mm_xor_si128(rot22, _calc);

        _calc = _mm_srli_epi32(*x,(32 - 22));
        _calc = _mm_xor_si128(rot22, _calc);
        _calc = _mm_xor_si128(_calc, rot2);
        _calc = _mm_xor_si128(_calc, rot13);

        return _calc;
}

//#define S1(x)           (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
__m128i funct_S1(const __m128i *x)
{
        __m128i rot6 = _mm_slli_epi32(*x, 6);
        __m128i _calc = _mm_srli_epi32(*x,(32 - 6));

        __m128i rot11 = _mm_slli_epi32(*x, 11);
        rot11 = _mm_xor_si128(rot11, _calc);
        _calc = _mm_srli_epi32(*x,(32 - 11));

        __m128i rot25 = _mm_slli_epi32(*x, 25);
        rot25 = _mm_xor_si128(rot25, _calc);

        _calc = _mm_srli_epi32(*x,(32 - 25));
        _calc = _mm_xor_si128(rot25, _calc);
        _calc = _mm_xor_si128(_calc, rot6);
        _calc = _mm_xor_si128(_calc, rot11);

        return _calc;
}

//#define s0(x)           (ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3))
__m128i funct_s0(const __m128i *x)
{
        __m128i rot7 = _mm_slli_epi32(*x, 7);
        __m128i _calc = _mm_srli_epi32(*x,(32 - 7));

        __m128i rot18 = _mm_slli_epi32(*x, 18);
        rot18 = _mm_xor_si128(rot18, _calc);
        _calc = _mm_srli_epi32(*x,(32 - 18));

        __m128i shift3 = _mm_srli_epi32(*x, 3);
        _calc = _mm_xor_si128(shift3, _calc);

        _calc = _mm_xor_si128(_calc, rot7);
        _calc = _mm_xor_si128(_calc, rot18);

        return _calc;
}

//#define s1(x)           (ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10))
__m128i funct_s1(const __m128i *x)
{
        __m128i rot17 = _mm_slli_epi32(*x, 17);
        __m128i _calc = _mm_srli_epi32(*x,(32 - 17));

        __m128i rot19 = _mm_slli_epi32(*x, 19);
        rot19 = _mm_xor_si128(rot19, _calc);
        _calc = _mm_srli_epi32(*x,(32 - 19));

        __m128i shift10 = _mm_srli_epi32(*x, 10);
        _calc = _mm_xor_si128(shift10, _calc);

        _calc = _mm_xor_si128(_calc, rot17);
        _calc = _mm_xor_si128(_calc, rot19);

        return _calc;
}

// #define Ch(x, y, z)     ((x & (y ^ z)) ^ z)
__m128i funct_Ch(__m128i *x, __m128i *y, __m128i *z)
{
        __m128i _calc = _mm_xor_si128(*y,*z);
        _calc = _mm_and_si128(_calc, *x);
        _calc = _mm_xor_si128(_calc, *z);
        return _calc;
}

// #define Maj(x, y, z)    ((x & (y | z)) | (y & z))
__m128i funct_Maj(__m128i *x, __m128i *y, __m128i *z)
{
        __m128i _calc = _mm_or_si128(*y,*z);
        _calc = _mm_and_si128(_calc, *x);
        __m128i _calc2 = _mm_and_si128(*y,*z);
        _calc = _mm_or_si128(_calc, _calc2);
        return _calc;
}

/* Elementary functions used by SHA256 */
//#define Ch(x, y, z)     ((x & (y ^ z)) ^ z)
//#define Maj(x, y, z)    ((x & (y | z)) | (y & z))
//#define ROTR(x, n)      ((x >> n) | (x << (32 - n)))
//#define S0(x)           (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
//#define S1(x)           (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
//#define s0(x)           (ROTR(x, 7) ^ ROTR(x, 18) ^ (x >> 3))
//#define s1(x)           (ROTR(x, 17) ^ ROTR(x, 19) ^ (x >> 10))

/* SHA256 round function */
//#define RND(a, b, c, d, e, f, g, h, k) \
//	do { \
//		t0 = h + S1(e) + Ch(e, f, g) + k; \
//		t1 = S0(a) + Maj(a, b, c); \
//		d += t0; \
//		h  = t0 + t1; \
//	} while (0)

void funct_RND(__m128i *a, __m128i *b, __m128i *c, __m128i *d, __m128i *e, __m128i *f, __m128i *g, __m128i *h,
		          __m128i *k)
{
	__m128i t0;
	__m128i t1;
	__m128i _calc = funct_S1(e);
	__m128i _calc1 = funct_Ch(e, f, g);
	_calc = _mm_add_epi32(_calc,_calc1);
	_calc = _mm_add_epi32(_calc,*h);
	t0 = _mm_add_epi32(_calc,*k);
	_calc = funct_Maj(a,b,c);
	_calc1 = funct_S0(a);
	t1 = _mm_add_epi32(_calc,_calc1);
	*d = _mm_add_epi32(*d, t0);
	*h = _mm_add_epi32(t1, t0);
}

/* Adjusted round function for rotating state */
//#define RNDr(S, W, i) \
//	RND(S[(64 - i) % 8], S[(65 - i) % 8], \
//	    S[(66 - i) % 8], S[(67 - i) % 8], \
//	    S[(68 - i) % 8], S[(69 - i) % 8], \
//	    S[(70 - i) % 8], S[(71 - i) % 8], \
//	    W[i] + sha256_k_sidm[i])



//void sha256_transform_sidm(uint32_t *state, const uint32_t *block, int swap)
void sha256_transform_sidm(__m128i *state, const __m128i *block, int swap)
{
/*
	uint32_t W[64] __attribute__((aligned(16)));
	uint32_t S[8] __attribute__((aligned(16)));
	uint32_t t0 __attribute__((aligned(16)));
	uint32_t t1 __attribute__((aligned(16)));
*/
	uint32_t W[64*4] __attribute__((aligned(16)));
	uint32_t S[8*4] __attribute__((aligned(16)));
//	uint32_t t0[4] __attribute__((aligned(16)));
//	uint32_t t1[4] __attribute__((aligned(16)));

	const __m128i vm = _mm_setr_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3); // for the swap function

	int i;

	__m128i *WPrt = (__m128i*) W;
	__m128i *SPrt = (__m128i*) S;
//	__m128i *T0Prt = (__m128i*) t0;
//	__m128i *T1Prt = (__m128i*) t1;
	__m128i _calc;

	/* 1. Prepare message schedule W. */
	if (swap) {
		for (i = 0; i < 16; i++)
//			W[i] = swab32(block[i]);
			WPrt[i] = _mm_shuffle_epi8(block[i],vm);
	} else{
		for (i = 0; i < 16; i++)
//			W[i] = swab32(block[i]);
			WPrt[i] = block[i];
//		memcpy(W, block, 64*4);
	}

	for (i = 16; i < 64; i += 2) {
		_calc = WPrt[i-2];
		WPrt[i] = funct_s1(&_calc);
		_calc = WPrt[i-15];
		_calc = funct_s0(&_calc);
		WPrt[i] = _mm_add_epi32(WPrt[i], WPrt[i-7]);
		WPrt[i] = _mm_add_epi32(WPrt[i], _calc);
		WPrt[i] = _mm_add_epi32(WPrt[i], WPrt[i-16]);

		_calc = WPrt[i-1];
		WPrt[i+1] = funct_s1(&_calc);
		_calc = WPrt[i-14];
		_calc = funct_s0(&_calc);
		WPrt[i] = _mm_add_epi32(WPrt[i], WPrt[i-6]);
		WPrt[i] = _mm_add_epi32(WPrt[i], _calc);
		WPrt[i] = _mm_add_epi32(WPrt[i], WPrt[i-15]);
	}

	/* 2. Initialize working variables. */
	for (i = 0; i < 8; i++)
		SPrt[i] = state[i];
// 	memcpy(S, state, 32);

	/* 3. Mix. */
//	/* Adjusted round function for rotating state */
//	#define RNDr(S, W, i) \
//		RND(S[(64 - i) % 8], S[(65 - i) % 8], \
//		    S[(66 - i) % 8], S[(67 - i) % 8], \
//		    S[(68 - i) % 8], S[(69 - i) % 8], \
//		    S[(70 - i) % 8], S[(71 - i) % 8], \
//		    W[i] + sha256_k_sidm[i])

	for(i=0;i<64;i++){
		_calc = _mm_set_epi32(sha256_k_sidm[i],sha256_k_sidm[i],sha256_k_sidm[i],sha256_k_sidm[i]);
		_calc = _mm_add_epi32(_calc, WPrt[i]);
		funct_RND(SPrt+((64-i)%8),SPrt+((65-i)%8),SPrt+((66-i)%8),SPrt+((67-i)%8),
				  SPrt+((68-i)%8),SPrt+((69-i)%8),SPrt+((70-i)%8),SPrt+((61-i)%8),
				  &_calc);
	}
/*
	RNDr(S, W,  0);
	RNDr(S, W,  1);
	RNDr(S, W,  2);
	RNDr(S, W,  3);
	RNDr(S, W,  4);
	RNDr(S, W,  5);
	RNDr(S, W,  6);
	RNDr(S, W,  7);
	RNDr(S, W,  8);
	RNDr(S, W,  9);
	RNDr(S, W, 10);
	RNDr(S, W, 11);
	RNDr(S, W, 12);
	RNDr(S, W, 13);
	RNDr(S, W, 14);
	RNDr(S, W, 15);
	RNDr(S, W, 16);
	RNDr(S, W, 17);
	RNDr(S, W, 18);
	RNDr(S, W, 19);
	RNDr(S, W, 20);
	RNDr(S, W, 21);
	RNDr(S, W, 22);
	RNDr(S, W, 23);
	RNDr(S, W, 24);
	RNDr(S, W, 25);
	RNDr(S, W, 26);
	RNDr(S, W, 27);
	RNDr(S, W, 28);
	RNDr(S, W, 29);
	RNDr(S, W, 30);
	RNDr(S, W, 31);
	RNDr(S, W, 32);
	RNDr(S, W, 33);
	RNDr(S, W, 34);
	RNDr(S, W, 35);
	RNDr(S, W, 36);
	RNDr(S, W, 37);
	RNDr(S, W, 38);
	RNDr(S, W, 39);
	RNDr(S, W, 40);
	RNDr(S, W, 41);
	RNDr(S, W, 42);
	RNDr(S, W, 43);
	RNDr(S, W, 44);
	RNDr(S, W, 45);
	RNDr(S, W, 46);
	RNDr(S, W, 47);
	RNDr(S, W, 48);
	RNDr(S, W, 49);
	RNDr(S, W, 50);
	RNDr(S, W, 51);
	RNDr(S, W, 52);
	RNDr(S, W, 53);
	RNDr(S, W, 54);
	RNDr(S, W, 55);
	RNDr(S, W, 56);
	RNDr(S, W, 57);
	RNDr(S, W, 58);
	RNDr(S, W, 59);
	RNDr(S, W, 60);
	RNDr(S, W, 61);
	RNDr(S, W, 62);
	RNDr(S, W, 63);
*/
	/* 4. Mix local working variables into global state */
	for (i = 0; i < 8; i++)
		state[i] = _mm_add_epi32(state[i], SPrt[i]);
//		state[i] += S[i];
}

//void sha256_init_sidm(uint32_t *state)
//{
//	memcpy(state, sha256_h_sidm, 32);
//}

static inline void HMAC_SHA256_80_init_sidm(const __m128i *key,
		__m128i *tstate, __m128i *ostate)
{
	uint32_t ihash[8 * 4] __attribute__((aligned(16)));
	uint32_t pad[16 * 4] __attribute__((aligned(16)));
	int i;

	__m128i *ihashPtr = (__m128i*) ihash;
	__m128i *padPtr = (__m128i*) pad;
	__m128i _calc;

	/* tstate is assumed to contain the midstate of key */
//	memcpy(pad, key + 16, 16);
	padPtr[0] = key[16];
	padPtr[1] = key[17];
	padPtr[2] = key[18];
	padPtr[3] = key[19];

	for (i=0;i<12;i++){
		padPtr[i+4] = _mm_set_epi32(keypad_sidm[i],keypad_sidm[i],keypad_sidm[i],keypad_sidm[i]);
	}

//	memcpy(pad + 4, keypad_sidm, 48);
	sha256_transform_sidm(tstate, padPtr, 0);
//	memcpy(ihash, tstate, 32);
	for (i=0;i<8;i++){
		ihashPtr[i] = tstate[i];
	}

//	sha256_init_sidm(ostate);
	for (i=0; i<8; i++){
//		*ptr = (__m128i*) & midstate[i * 4];
		ostate[i] = _mm_set_epi32(sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i]);
	}

	_calc = _mm_set_epi32(0x5c5c5c5c,0x5c5c5c5c,0x5c5c5c5c,0x5c5c5c5c);

	for (i = 0; i < 8; i++)
//		pad[i] = ihash[i] ^ 0x5c5c5c5c;
		padPtr[i] = _mm_xor_si128(ihashPtr[i], _calc);
	for (i=8; i < 16; i++)
//		pad[i] = 0x5c5c5c5c;
		padPtr[i] = _calc;

	sha256_transform_sidm(ostate, padPtr, 0);

	for (i=0; i<8; i++){
//		*ptr = (__m128i*) & midstate[i * 4];
		tstate[i] = _mm_set_epi32(sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i]);
	}
//	sha256_init_sidm(tstate);
	_calc = _mm_set_epi32(0x36363636,0x36363636,0x36363636,0x36363636);

	for (i = 0; i < 8; i++)
//		pad[i] = ihash[i] ^ 0x36363636;
		padPtr[i] = _mm_xor_si128(ihashPtr[i], _calc);
	for (; i < 16; i++)
//		pad[i] = 0x36363636;
		padPtr[i] = _calc;
	sha256_transform_sidm(tstate, padPtr, 0);
}

static inline void PBKDF2_SHA256_80_128_sidm(const __m128i *tstate,
	const __m128i *ostate, const __m128i *salt, __m128i *output)
{
	uint32_t istate[8 * 4] __attribute__((aligned(32)));
	uint32_t ostate2[8 * 4] __attribute__((aligned(32)));
	uint32_t ibuf[16 * 4] __attribute__((aligned(32)));
	uint32_t obuf[16 * 4] __attribute__((aligned(32)));

	int i, j;
	__m128i *istatePtr = (__m128i*)istate;
	__m128i *ostate2Ptr = (__m128i*)ostate2;
	__m128i *ibufPtr = (__m128i*)ibuf;
	__m128i *obufPtr = (__m128i*)obuf;

	const __m128i vm = _mm_setr_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3); // for the swap function

	memcpy(istate, tstate, 32);
	for (i=0;i<8;i++)
		istatePtr[i] = tstate[i];
//	sha256_transform_sidm(istate, salt, 0);

	ibufPtr[0] = salt[16];
	ibufPtr[1] = salt[17];
	ibufPtr[2] = salt[18];
	ibufPtr[3] = salt[19];
//	memcpy(ibuf, salt + 16, 16);
	for (i=0;i<11;i++){
		ibufPtr[i+5] = _mm_set_epi32(innerpad_sidm[i],innerpad_sidm[i],innerpad_sidm[i],innerpad_sidm[i]);
	}
//	memcpy(ibuf + 5, innerpad_sidm, 44);
	for (i=0;i<8;i++){
		obufPtr[i+8] = _mm_set_epi32(outerpad_sidm[i],outerpad_sidm[i],outerpad_sidm[i],outerpad_sidm[i]);
	}
//	memcpy(obuf + 8, outerpad_sidm, 32);

	for (i = 0; i < 4; i++) {
		for (i=0;i<8;i++){
			obufPtr[i] = istatePtr[i];
		}
//		memcpy(obuf, istate, 32);
		ibufPtr[4] = _mm_set_epi32(i+1, i+1, i+1, i+1);
//		ibuf[4] = i + 1;
		sha256_transform_sidm(obufPtr, ibufPtr, 0);

		for (i=0;i<8;i++){
			ostate2Ptr[i] = ostate[i];
		}
//		memcpy(ostate2, ostate, 32);

		sha256_transform_sidm(ostate2Ptr, obufPtr, 0);
		for (j = 0; j < 8; j++)
//			output[8 * i + j] = swab32(ostate2[j]);
			obufPtr[8 * i + j] = _mm_shuffle_epi8(ostate2Ptr[j],vm);
	}
}

static inline void PBKDF2_SHA256_128_32_sidm(__m128i *tstate, __m128i *ostate,
	const __m128i *salt, __m128i *output)
{
	uint32_t buf[16 * 4] __attribute__((aligned(16)));
	int i;

	const __m128i vm = _mm_setr_epi8(12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3); // for the swap function

	__m128i *bufPtr = (__m128i*) buf;
    __m128i *finalblkPtr = (__m128i*) finalblk_sidm;
	sha256_transform_sidm(tstate, salt, 1);
	sha256_transform_sidm(tstate, salt + 16, 1);
	sha256_transform_sidm(tstate, finalblkPtr, 0);

	for (i = 0; i < 8; i++)
		bufPtr[i] = tstate[i];
//	memcpy(buf, tstate, 32);
	for (i = 0; i < 8; i++)
		bufPtr[i+8] = _mm_set_epi32(outerpad_sidm[i],outerpad_sidm[i],outerpad_sidm[i],outerpad_sidm[i]);
//	memcpy(buf + 8, outerpad_sidm, 32);

	sha256_transform_sidm(ostate, bufPtr, 0);
	for (i = 0; i < 8; i++)
		output[i] = _mm_shuffle_epi8(ostate[i],vm);
//		output[i] = swab32(ostate[i]);
}

static void scrypt_1024_1_1_256_sidm(const uint32_t *input, uint32_t *output,
	uint32_t *midstate)
{
	uint32_t tstate[8 * 4] __attribute__((aligned(16)));
	uint32_t ostate[8 * 4]__attribute__((aligned(16)));
	uint32_t X[32 * 4] __attribute__((aligned(16)));
	uint32_t XInv[32 * 4] __attribute__((aligned(16)));
	int i, j;
	__m128i *inputPtr = (__m128i*) input;
	__m128i *outputPtr = (__m128i*) output;
	__m128i *tstatePtr = (__m128i*) tstate;
	__m128i *ostatePtr = (__m128i*) ostate;
	__m128i *midstatePtr = (__m128i*) midstate;
	__m128i *XPtr = (__m128i*) X;

//	memcpy(tstate, midstate, 32);
	for (i = 0; i < 8; i++)
		tstatePtr[i] = midstatePtr[i];

	HMAC_SHA256_80_init_sidm(inputPtr, tstatePtr, ostatePtr);
	PBKDF2_SHA256_80_128_sidm(tstatePtr, ostatePtr, inputPtr, XPtr);

	// need to transpose X 4-colums to 4-rows
	for (j=0;j<4;j++){
		for (i=0;i<32;i++){
			XInv[i*4+j] = X[i+(j*32)];
		}
	}

	scrypt_core_sidm(XInv + 0);
	scrypt_core_sidm(XInv + 32);
	scrypt_core_sidm(XInv + 64);
	scrypt_core_sidm(XInv + 96);
	// need to transpose X 4-rows to 4-colums

	for (j=0;j<4;j++){
		for (i=0;i<32;i++){
			X[i+(j*32)] = XInv[i*4+j];
		}
	}

	PBKDF2_SHA256_128_32_sidm(tstatePtr, ostatePtr, XPtr, outputPtr);
}


int scanhash_scrypt_sidm(int thr_id, uint32_t *pdata,
	const uint32_t *ptarget, uint32_t max_nonce, unsigned long *hashes_done)
{
	uint32_t data[4 * 20] __attribute__((aligned(16)));
	uint32_t hash[4 * 8]  __attribute__((aligned(16)));
	uint32_t midstate[8 * 4] __attribute__((aligned(16)));
	uint32_t n = pdata[19] - 1;
	const uint32_t Htarg = ptarget[7];
	int throughput = 4;
	int i;
	__m128i calc;
	__m128i *dataPtr = (__m128i*) &data[0];
	__m128i *midstatePtr = (__m128i*) &midstate[0];
	/* move the data in four columns */
	for (i=0; i<20; i++){
		dataPtr[i] = _mm_set_epi32(pdata[i],pdata[i],pdata[i],pdata[i]);
	}
//	for (i = 0; i < throughput; i++)
//		memcpy(data + i * 20, pdata, 80);
	for (i=0; i<8; i++){
//		*ptr = (__m128i*) & midstate[i * 4];
		midstatePtr[i] = _mm_set_epi32(sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i],sha256_h_sidm[i]);
	}
//	sha256_init_sidm(midstate);
	sha256_transform_sidm(midstatePtr, dataPtr, 0);

	do {
		for (i = 0; i < throughput; i++)
//			data[i * 20 + 19] = ++n;
			data[19 * 4 + i] = ++n;

			//scrypt_1024_1_1_256(data, hash, midstate, scratchbuf);
	    scrypt_1024_1_1_256_sidm(data, hash, midstate);
		for (i = 0; i < throughput; i++) {
//			if (hash[i * 8 + 7] <= Htarg && fulltest(hash + i * 8, ptarget)) {
			if (hash[7 * 4 + i] <= Htarg && fulltest(hash + i, ptarget)) {
				*hashes_done = n - pdata[19] + 1;
//				pdata[19] = data[i * 20 + 19];
				pdata[19] = data[19 * 4 + i];
				return 1;
			}
		}
	} while (n < max_nonce && !work_restart[thr_id].restart);

	*hashes_done = n - pdata[19] + 1;
	pdata[19] = n;
	return 0;
}


