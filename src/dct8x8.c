#include "vcodec_common.h"

#define SQRT_2 1.41421356

void vcodec_dct8(const uint8_t *p_in, int32_t *p_out) {
    const int32_t c1_1 = (int32_t)(0.9807852800 * (1 << 13)); // a = cos(pi/16)
    const int32_t c1_2 = (int32_t)(0.1913417160 * (1 << 13)); // a + b = cos(pi/16) + sin(pi/16)
    const int32_t c1_3 = (int32_t)(-0.785694958 * (1 << 13)); // b - a = sin(pi/16) - cos(pi/16)

    const int32_t c3_1 = (int32_t)(0.831469612 * (1 << 13));  // a = cos(3*pi/16)
    const int32_t c3_2 = (int32_t)(1.38703985 * (1 << 13));   // a + b = cos(3*pi/16) + sin(3*pi/16)
    const int32_t c3_3 = (int32_t)(-0.275899379 * (1 << 13)); // b - a = sin(3*pi/16) - cos(3*pi/16)

    const int32_t sqrt2_c1_1 = (int32_t)(1.38703985 * (1 << 13));  // a = sqrt(2) * cos(pi/16)
    const int32_t sqrt2_c1_2 = (int32_t)(1.66293922 * (1 << 13));  // a + b = sqrt(2) * (cos(pi/16) + sin(pi/16))
    const int32_t sqrt2_c1_3 = (int32_t)(-1.11114047 * (1 << 13)); // b - a = sqrt(2) * (sin(pi/16) - cos(pi/16))

    int32_t stage1[8];
    stage1[0] = p_in[0] + p_in[7];
    stage1[1] = p_in[1] + p_in[6];
    stage1[2] = p_in[2] + p_in[5];
    stage1[3] = p_in[3] + p_in[4];
    stage1[4] = p_in[3] - p_in[4];
    stage1[5] = p_in[2] - p_in[5];
    stage1[6] = p_in[1] - p_in[6];
    stage1[7] = p_in[0] - p_in[7];

    int32_t stage2[8];
    stage2[0] = stage1[0] + stage1[3];
    stage2[1] = stage1[1] + stage1[2];
    stage2[2] = stage1[1] - stage1[2];
    stage2[3] = stage1[0] - stage1[3];

    const int32_t tmp47 = c3_1 * (stage1[4] + stage1[7]);
    stage2[4] =  c3_3 * stage1[7] + tmp47;
    stage2[7] = -c3_2 * stage1[4] + tmp47;

    const int32_t tmp56 = c1_1 * (stage1[5] + stage1[6]);
    stage2[5] =  c1_3 * stage1[6] + tmp56;
    stage2[6] = -c1_2 * stage1[5] + tmp56;

    int32_t stage3[8];
    stage3[0] = stage2[0] + stage2[1];
    stage3[1] = stage2[0] - stage2[1];

    const int32_t tmp23 = sqrt2_c1_1 * (stage2[2] + stage2[3]);
    stage3[2] =  sqrt2_c1_3 * stage2[3] + tmp23;
    stage3[3] = -sqrt2_c1_2 * stage2[2] + tmp23;

    stage3[4] = stage2[4] + stage2[6];
    stage3[5] = stage2[7] - stage2[5];
    stage3[6] = stage2[4] - stage2[6];
    stage3[7] = stage2[7] + stage2[5];

    p_out[0] = stage3[0];
    p_out[1] = stage3[7] + stage3[4];
    p_out[2] = stage3[2];
    p_out[3] = stage3[5] * SQRT_2;
    p_out[4] = stage3[1];
    p_out[5] = stage3[6] * SQRT_2;
    p_out[6] = stage3[3];
    p_out[7] = stage3[7] - stage3[4];
}

typedef int DCTELEM;		/* 16 or 32 bits is fine */
typedef int32_t INT32;

typedef unsigned char JSAMPLE;
typedef JSAMPLE *JSAMPROW;	/* ptr to one image row of pixel samples. */
typedef JSAMPROW *JSAMPARRAY;	/* ptr to some rows (a 2-D sample array) */

#define SHIFT_TEMPS
#define RIGHT_SHIFT(x,shft)	((x) >> (shft))
#define GETJSAMPLE(value)  ((int) (value))
#define MULTIPLY16C16(var,const)  ((var) * (const))
#define MULTIPLY(var,const)  MULTIPLY16C16(var,const)

#define FIX_0_298631336  ((INT32)  2446)	/* FIX(0.298631336) */
#define FIX_0_390180644  ((INT32)  3196)	/* FIX(0.390180644) */
#define FIX_0_541196100  ((INT32)  4433)	/* FIX(0.541196100) */
#define FIX_0_765366865  ((INT32)  6270)	/* FIX(0.765366865) */
#define FIX_0_899976223  ((INT32)  7373)	/* FIX(0.899976223) */
#define FIX_1_175875602  ((INT32)  9633)	/* FIX(1.175875602) */
#define FIX_1_501321110  ((INT32)  12299)	/* FIX(1.501321110) */
#define FIX_1_847759065  ((INT32)  15137)	/* FIX(1.847759065) */
#define FIX_1_961570560  ((INT32)  16069)	/* FIX(1.961570560) */
#define FIX_2_053119869  ((INT32)  16819)	/* FIX(2.053119869) */
#define FIX_2_562915447  ((INT32)  20995)	/* FIX(2.562915447) */
#define FIX_3_072711026  ((INT32)  25172)	/* FIX(3.072711026) */

#define CONST_BITS 13
#define PASS1_BITS  2
#define DCTSIZE 8
#define MAXJSAMPLE	255
#define CENTERJSAMPLE	128
#define ONE	((INT32) 1)
#define CONST_SCALE (ONE << CONST_BITS)

void jpeg_fdct_islow(DCTELEM * data, uint8_t *sample_data)
{
  INT32 tmp0, tmp1, tmp2, tmp3;
  INT32 tmp10, tmp11, tmp12, tmp13;
  INT32 z1;
  DCTELEM *dataptr;
  int ctr;
  SHIFT_TEMPS

  /* Pass 1: process rows.
   * Note results are scaled up by sqrt(8) compared to a true DCT;
   * furthermore, we scale the results by 2**PASS1_BITS.
   * cK represents sqrt(2) * cos(K*pi/16).
   */

  dataptr = data;
  for (ctr = 0; ctr < DCTSIZE; ctr++) {
    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "c1" should be "c6".
     */

    tmp0 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 0]) + GETJSAMPLE(sample_data[ctr * DCTSIZE + 7]);
    tmp1 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 1]) + GETJSAMPLE(sample_data[ctr * DCTSIZE + 6]);
    tmp2 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 2]) + GETJSAMPLE(sample_data[ctr * DCTSIZE + 5]);
    tmp3 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 3]) + GETJSAMPLE(sample_data[ctr * DCTSIZE + 4]);

    tmp10 = tmp0 + tmp3;
    tmp12 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp13 = tmp1 - tmp2;

    tmp0 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 0]) - GETJSAMPLE(sample_data[ctr * DCTSIZE + 7]);
    tmp1 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 1]) - GETJSAMPLE(sample_data[ctr * DCTSIZE + 6]);
    tmp2 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 2]) - GETJSAMPLE(sample_data[ctr * DCTSIZE + 5]);
    tmp3 = GETJSAMPLE(sample_data[ctr * DCTSIZE + 3]) - GETJSAMPLE(sample_data[ctr * DCTSIZE + 4]);

    /* Apply unsigned->signed conversion. */
    dataptr[0] = (DCTELEM) ((tmp10 + tmp11 - 8 * CENTERJSAMPLE) << PASS1_BITS);
    dataptr[4] = (DCTELEM) ((tmp10 - tmp11) << PASS1_BITS);

    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_541196100);       /* c6 */
    /* Add fudge factor here for final descale. */
    z1 += ONE << (CONST_BITS-PASS1_BITS-1);

    dataptr[2] = (DCTELEM)
      RIGHT_SHIFT(z1 + MULTIPLY(tmp12, FIX_0_765366865), /* c2-c6 */
		  CONST_BITS-PASS1_BITS);
    dataptr[6] = (DCTELEM)
      RIGHT_SHIFT(z1 - MULTIPLY(tmp13, FIX_1_847759065), /* c2+c6 */
		  CONST_BITS-PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * i0..i3 in the paper are tmp0..tmp3 here.
     */

    tmp12 = tmp0 + tmp2;
    tmp13 = tmp1 + tmp3;

    z1 = MULTIPLY(tmp12 + tmp13, FIX_1_175875602);       /*  c3 */
    /* Add fudge factor here for final descale. */
    z1 += ONE << (CONST_BITS-PASS1_BITS-1);

    tmp12 = MULTIPLY(tmp12, - FIX_0_390180644);          /* -c3+c5 */
    tmp13 = MULTIPLY(tmp13, - FIX_1_961570560);          /* -c3-c5 */
    tmp12 += z1;
    tmp13 += z1;

    z1 = MULTIPLY(tmp0 + tmp3, - FIX_0_899976223);       /* -c3+c7 */
    tmp0 = MULTIPLY(tmp0, FIX_1_501321110);              /*  c1+c3-c5-c7 */
    tmp3 = MULTIPLY(tmp3, FIX_0_298631336);              /* -c1+c3+c5-c7 */
    tmp0 += z1 + tmp12;
    tmp3 += z1 + tmp13;

    z1 = MULTIPLY(tmp1 + tmp2, - FIX_2_562915447);       /* -c1-c3 */
    tmp1 = MULTIPLY(tmp1, FIX_3_072711026);              /*  c1+c3+c5-c7 */
    tmp2 = MULTIPLY(tmp2, FIX_2_053119869);              /*  c1+c3-c5+c7 */
    tmp1 += z1 + tmp13;
    tmp2 += z1 + tmp12;

    dataptr[1] = (DCTELEM) RIGHT_SHIFT(tmp0, CONST_BITS-PASS1_BITS);
    dataptr[3] = (DCTELEM) RIGHT_SHIFT(tmp1, CONST_BITS-PASS1_BITS);
    dataptr[5] = (DCTELEM) RIGHT_SHIFT(tmp2, CONST_BITS-PASS1_BITS);
    dataptr[7] = (DCTELEM) RIGHT_SHIFT(tmp3, CONST_BITS-PASS1_BITS);

    dataptr += DCTSIZE;		/* advance pointer to next row */
  }

  /* Pass 2: process columns.
   * We remove the PASS1_BITS scaling, but leave the results scaled up
   * by an overall factor of 8.
   * cK represents sqrt(2) * cos(K*pi/16).
   */

  dataptr = data;
  for (ctr = DCTSIZE-1; ctr >= 0; ctr--) {
    /* Even part per LL&M figure 1 --- note that published figure is faulty;
     * rotator "c1" should be "c6".
     */

    tmp0 = dataptr[DCTSIZE*0] + dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] + dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] + dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] + dataptr[DCTSIZE*4];

    /* Add fudge factor here for final descale. */
    tmp10 = tmp0 + tmp3 + (ONE << (PASS1_BITS-1));
    tmp12 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp13 = tmp1 - tmp2;

    tmp0 = dataptr[DCTSIZE*0] - dataptr[DCTSIZE*7];
    tmp1 = dataptr[DCTSIZE*1] - dataptr[DCTSIZE*6];
    tmp2 = dataptr[DCTSIZE*2] - dataptr[DCTSIZE*5];
    tmp3 = dataptr[DCTSIZE*3] - dataptr[DCTSIZE*4];

    dataptr[DCTSIZE*0] = (DCTELEM) RIGHT_SHIFT(tmp10 + tmp11, PASS1_BITS);
    dataptr[DCTSIZE*4] = (DCTELEM) RIGHT_SHIFT(tmp10 - tmp11, PASS1_BITS);

    z1 = MULTIPLY(tmp12 + tmp13, FIX_0_541196100);       /* c6 */
    /* Add fudge factor here for final descale. */
    z1 += ONE << (CONST_BITS+PASS1_BITS-1);

    dataptr[DCTSIZE*2] = (DCTELEM)
      RIGHT_SHIFT(z1 + MULTIPLY(tmp12, FIX_0_765366865), /* c2-c6 */
		  CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*6] = (DCTELEM)
      RIGHT_SHIFT(z1 - MULTIPLY(tmp13, FIX_1_847759065), /* c2+c6 */
		  CONST_BITS+PASS1_BITS);

    /* Odd part per figure 8 --- note paper omits factor of sqrt(2).
     * i0..i3 in the paper are tmp0..tmp3 here.
     */

    tmp12 = tmp0 + tmp2;
    tmp13 = tmp1 + tmp3;

    z1 = MULTIPLY(tmp12 + tmp13, FIX_1_175875602);       /*  c3 */
    /* Add fudge factor here for final descale. */
    z1 += ONE << (CONST_BITS+PASS1_BITS-1);

    tmp12 = MULTIPLY(tmp12, - FIX_0_390180644);          /* -c3+c5 */
    tmp13 = MULTIPLY(tmp13, - FIX_1_961570560);          /* -c3-c5 */
    tmp12 += z1;
    tmp13 += z1;

    z1 = MULTIPLY(tmp0 + tmp3, - FIX_0_899976223);       /* -c3+c7 */
    tmp0 = MULTIPLY(tmp0, FIX_1_501321110);              /*  c1+c3-c5-c7 */
    tmp3 = MULTIPLY(tmp3, FIX_0_298631336);              /* -c1+c3+c5-c7 */
    tmp0 += z1 + tmp12;
    tmp3 += z1 + tmp13;

    z1 = MULTIPLY(tmp1 + tmp2, - FIX_2_562915447);       /* -c1-c3 */
    tmp1 = MULTIPLY(tmp1, FIX_3_072711026);              /*  c1+c3+c5-c7 */
    tmp2 = MULTIPLY(tmp2, FIX_2_053119869);              /*  c1+c3-c5+c7 */
    tmp1 += z1 + tmp13;
    tmp2 += z1 + tmp12;

    dataptr[DCTSIZE*1] = (DCTELEM) RIGHT_SHIFT(tmp0, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*3] = (DCTELEM) RIGHT_SHIFT(tmp1, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*5] = (DCTELEM) RIGHT_SHIFT(tmp2, CONST_BITS+PASS1_BITS);
    dataptr[DCTSIZE*7] = (DCTELEM) RIGHT_SHIFT(tmp3, CONST_BITS+PASS1_BITS);

    dataptr++;			/* advance pointer to next column */
  }
}
