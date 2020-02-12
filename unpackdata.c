/************************************************************************
 * Routines for decoding INT16, INT32, FLOAT32, FLOAT64, STEIM1,
 * STEIM2, GEOSCOPE (24bit and gain ranged), CDSN, SRO and DWWSSN
 * encoded data.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2023 Chad Trabant, EarthScope Data Services
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "libmseed.h"
#include "unpackdata.h"

/* Control for printing debugging information */
int libmseed_decodedebug = -1;

/* Extract bit range.  Byte order agnostic & defined when used with unsigned values */
#define EXTRACTBITRANGE(VALUE, STARTBIT, LENGTH) (((VALUE) >> (STARTBIT)) & ((1U << (LENGTH)) - 1))

#define MAX12 0x7FFul    /* maximum 12 bit positive # */
#define MAX14 0x1FFFul   /* maximum 14 bit positive # */
#define MAX16 0x7FFFul   /* maximum 16 bit positive # */
#define MAX24 0x7FFFFFul /* maximum 24 bit positive # */

#define LM_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__
#define LM_CAT(a, b) LM_PRIMITIVE_CAT (a, b)

#define LM_IIF(c) LM_PRIMITIVE_CAT (LM_IIF_, c)
/* run the 2nd parameter */
#define LM_IIF_0(t, ...) __VA_ARGS__
/* run the 1st parameter */
#define LM_IIF_1(t, ...) t

#define LM_COMPL(b) LM_PRIMITIVE_CAT (LM_COMPL_, b)
#define LM_COMPL_0 1
#define LM_COMPL_1 0

#define LM_DEC(x) LM_PRIMITIVE_CAT (LM_DEC_, x)
#define LM_DEC_1 0
#define LM_DEC_2 1
#define LM_DEC_3 2
#define LM_DEC_4 3
#define LM_DEC_5 4
#define LM_DEC_6 5
#define LM_DEC_7 6
#define LM_DEC_8 7
#define LM_DEC_9 8
#define LM_DEC_10 9
#define LM_DEC_11 10
#define LM_DEC_12 11
#define LM_DEC_13 12
#define LM_DEC_14 13
#define LM_DEC_15 14
#define LM_DEC_16 15

/* detection */
#define LM_CHECK_N(x, n, ...) n
#define LM_CHECK(...) LM_CHECK_N (__VA_ARGS__, 0, )
#define LM_PROBE(x) x, 1,

#define LM_NOT(x) LM_CHECK (LM_PRIMITIVE_CAT (LM_NOT_, x))
#define LM_NOT_0 LM_PROBE (~)

#define LM_BOOL(x) LM_COMPL (LM_NOT (x))
#define LM_IF(c) LM_IIF (LM_BOOL (c))

#define LM_EAT(...)
#define LM_EXPAND(...) __VA_ARGS__
#define LM_WHEN(c) LM_IF (c) (LM_EXPAND, LM_EAT)

/* recursion */
/* deferred expression */
#define LM_EMPTY()
#define LM_DEFER(id) id LM_EMPTY ()
#define LM_OBSTRUCT(...) __VA_ARGS__ LM_DEFER (LM_EMPTY) ()
#define LM_EXPAND(...) __VA_ARGS__

#define LM_EVAL(...) \
  LM_EVAL1 (LM_EVAL1 (LM_EVAL1 (__VA_ARGS__)))
#define LM_EVAL1(...) \
  LM_EVAL2 (LM_EVAL2 (LM_EVAL2 (__VA_ARGS__)))
#define LM_EVAL2(...) \
  LM_EVAL3 (LM_EVAL3 (LM_EVAL3 (__VA_ARGS__)))
#define LM_EVAL3(...) __VA_ARGS__

#define LM_REPEAT(count, macro, ...)                   \
  LM_WHEN (count)                                      \
  (LM_OBSTRUCT (LM_REPEAT_INDIRECT) () (               \
      LM_DEC (count), macro,                           \
      __VA_ARGS__)LM_OBSTRUCT (macro) (LM_DEC (count), \
                                       __VA_ARGS__))
#define LM_REPEAT_INDIRECT() LM_REPEAT

/************************************************************************
 * msr_decode_int16:
 *
 * Decode 16-bit integer data and place in supplied buffer as 32-bit
 * integers.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_int16 (int16_t *input, int64_t samplecount, int32_t *output,
                  int64_t outputlength, int swapflag)
{
  int16_t sample;
  int idx;

  if (samplecount <= 0)
    return 0;

  if (!input || !output || outputlength <= 0)
    return -1;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (int32_t); idx++)
  {
    sample = input[idx];

    if (swapflag)
      ms_gswap2 (&sample);

    output[idx] = (int32_t)sample;

    outputlength -= sizeof (int32_t);
  }

  return idx;
} /* End of msr_decode_int16() */

/************************************************************************
 * msr_decode_int32:
 *
 * Decode 32-bit integer data and place in supplied buffer as 32-bit
 * integers.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_int32 (int32_t *input, int64_t samplecount, int32_t *output,
                  int64_t outputlength, int swapflag)
{
  int32_t sample;
  int idx;

  if (samplecount <= 0)
    return 0;

  if (!input || !output || outputlength <= 0)
    return -1;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (int32_t); idx++)
  {
    sample = input[idx];

    if (swapflag)
      ms_gswap4 (&sample);

    output[idx] = sample;

    outputlength -= sizeof (int32_t);
  }

  return idx;
} /* End of msr_decode_int32() */

/************************************************************************
 * msr_decode_float32:
 *
 * Decode 32-bit float data and place in supplied buffer as 32-bit
 * floats.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_float32 (float *input, int64_t samplecount, float *output,
                    int64_t outputlength, int swapflag)
{
  float sample;
  int idx;

  if (samplecount <= 0)
    return 0;

  if (!input || !output || outputlength <= 0)
    return -1;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (float); idx++)
  {
    memcpy (&sample, &input[idx], sizeof (float));

    if (swapflag)
      ms_gswap4 (&sample);

    output[idx] = sample;

    outputlength -= sizeof (float);
  }

  return idx;
} /* End of msr_decode_float32() */

/************************************************************************
 * msr_decode_float64:
 *
 * Decode 64-bit float data and place in supplied buffer as 64-bit
 * floats, aka doubles.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_float64 (double *input, int64_t samplecount, double *output,
                    int64_t outputlength, int swapflag)
{
  double sample;
  int idx;

  if (samplecount <= 0)
    return 0;

  if (!input || !output || outputlength <= 0)
    return -1;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (double); idx++)
  {
    memcpy (&sample, &input[idx], sizeof (double));

    if (swapflag)
      ms_gswap8 (&sample);

    output[idx] = sample;

    outputlength -= sizeof (double);
  }

  return idx;
} /* End of msr_decode_float64() */

/************************************************************************
 * msr_decode_steim1:
 *
 * Decode Steim1 encoded miniSEED data and place in supplied buffer
 * as 32-bit integers.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_steim1 (int32_t *input, int inputlength, int64_t samplecount,
                   int32_t *output, int64_t outputlength, char *srcname,
                   int swapflag)
{
  int32_t *outputptr = output; /* Pointer to next output sample location */
  uint32_t frame[16];          /* Frame, 16 x 32-bit quantities = 64 bytes */
  int32_t X0    = 0;           /* Forward integration constant, aka first sample */
  int32_t Xn    = 0;           /* Reverse integration constant, aka last sample */
  int maxframes = inputlength / 64;
  int frameidx;
  int startnibble;
  int nibble;
  int widx;
  int diffcount;
  int idx;

  union dword
  {
    int8_t d8[4];
    int16_t d16[2];
    int32_t d32;
  } * word;

  if (inputlength <= 0)
    return 0;

  if (!input || !output || outputlength <= 0 || maxframes <= 0)
    return -1;

  if (libmseed_decodedebug > 0)
    ms_log (0, "Decoding %d Steim1 frames, swapflag: %d, srcname: %s\n",
            maxframes, swapflag, (srcname) ? srcname : "");

  for (frameidx = 0; frameidx < maxframes && samplecount > 0; frameidx++)
  {
    /* Copy frame, each is 16x32-bit quantities = 64 bytes */
    memcpy (frame, input + (16 * frameidx), 64);

    /* Save forward integration constant (X0) and reverse integration constant (Xn)
       and set the starting nibble index depending on frame. */
    if (frameidx == 0)
    {
      if (swapflag)
      {
        ms_gswap4 (&frame[1]);
        ms_gswap4 (&frame[2]);
      }

      X0 = frame[1];
      Xn = frame[2];

      startnibble = 3; /* First frame: skip nibbles, X0, and Xn */

      if (libmseed_decodedebug > 0)
        ms_log (0, "Frame %d: X0=%d  Xn=%d\n", frameidx, X0, Xn);
    }
    else
    {
      startnibble = 1; /* Subsequent frames: skip nibbles */

      if (libmseed_decodedebug > 0)
        ms_log (0, "Frame %d\n", frameidx);
    }

    /* Swap 32-bit word containing the nibbles */
    if (swapflag)
      ms_gswap4 (&frame[0]);

    /* Decode each 32-bit word according to nibble */
    for (widx = startnibble; widx < 16 && samplecount > 0; widx++)
    {
      /* W0: the first 32-bit contains 16 x 2-bit nibbles for each word */
      nibble = EXTRACTBITRANGE (frame[0], (30 - (2 * widx)), 2);

      word      = (union dword *)&frame[widx];
      diffcount = 0;

      switch (nibble)
      {
      case 0: /* 00: Special flag, no differences */
        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 00=special\n", widx);
        break;

      case 1: /* 01: Four 1-byte differences */
        diffcount = 4;

        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 01=4x8b  %d  %d  %d  %d\n",
                  widx, word->d8[0], word->d8[1], word->d8[2], word->d8[3]);
        break;

      case 2: /* 10: Two 2-byte differences */
        diffcount = 2;

        if (swapflag)
        {
          ms_gswap2 (&word->d16[0]);
          ms_gswap2 (&word->d16[1]);
        }

        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 10=2x16b  %d  %d\n", widx, word->d16[0], word->d16[1]);
        break;

      case 3: /* 11: One 4-byte difference */
        diffcount = 1;
        if (swapflag)
          ms_gswap4 (&word->d32);

        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 11=1x32b  %d\n", widx, word->d32);
        break;
      } /* Done with decoding 32-bit word based on nibble */

      /* Apply accumulated differences to calculate output samples */
      if (diffcount > 0)
      {
        for (idx = 0; idx < diffcount && samplecount > 0; idx++, outputptr++)
        {
          if (outputptr == output) /* Ignore first difference, instead store X0 */
            *outputptr = X0;
          else if (diffcount == 4) /* Otherwise store difference from previous sample */
            *outputptr = *(outputptr - 1) + word->d8[idx];
          else if (diffcount == 2)
            *outputptr = *(outputptr - 1) + word->d16[idx];
          else if (diffcount == 1)
            *outputptr = *(outputptr - 1) + word->d32;

          samplecount--;
        }
      }
    } /* Done looping over nibbles and 32-bit words */
  }   /* Done looping over frames */

  /* Check data integrity by comparing last sample to Xn (reverse integration constant) */
  if (outputptr != output && *(outputptr - 1) != Xn)
  {
    ms_log (1, "%s: Warning: Data integrity check for Steim1 failed, Last sample=%d, Xn=%d\n",
            srcname, *(outputptr - 1), Xn);
  }

  return (outputptr - output);
} /* End of msr_decode_steim1() */

/* Steim2 helper macroes */
#define LM_PRINT_FORMAT_INTEGER(...) "%d  "

#define LM_PRINT_DIFF(i, _) , diff[i]

/* clang-format off */
#define LM_STEIM2_DECODE_DEBUG_PRINT(NIBBLE_STR, DNIB_STR, DIFFCNT, STARTBIT)                                                                  \
  ms_log (0, "  W%02d: " NIBBLE_STR "," DNIB_STR "=" #DIFFCNT "x" #STARTBIT "b  " LM_EVAL (LM_REPEAT (DIFFCNT, LM_PRINT_FORMAT_INTEGER)) "\n", \
          widx                                                                                                                                 \
          LM_EVAL (LM_REPEAT (DIFFCNT, LM_PRINT_DIFF, DIFFCNT)))
/* clang-format on */

#define LM_STEIM2_EXTRACT_TO_DIFF_ARR(IDX, DIFFCNT, STARTBIT, SEMASK)                    \
  do                                                                                     \
  {                                                                                      \
    diff[IDX] = EXTRACTBITRANGE (frame[widx], (DIFFCNT - 1 - IDX) * STARTBIT, STARTBIT); \
    diff[IDX] = (diff[IDX] ^ SEMASK) - SEMASK;                                           \
  } while (0);

#define LM_STEIM2_EXTRACT_DIFF(NIBBLE_STR, DNIB_STR, DIFFCNT, STARTBIT)                     \
  do                                                                                        \
  {                                                                                         \
    diffcount = DIFFCNT;                                                                    \
    semask    = 1ul << (STARTBIT - 1);                                                      \
    LM_EVAL (LM_REPEAT (DIFFCNT, LM_STEIM2_EXTRACT_TO_DIFF_ARR, DIFFCNT, STARTBIT, semask)) \
    if (libmseed_decodedebug > 0)                                                           \
      LM_STEIM2_DECODE_DEBUG_PRINT (NIBBLE_STR, DNIB_STR, DIFFCNT, STARTBIT);               \
  } while (0)

#define LM_STEIM2_EXTRACT_DNIB                   \
  do                                             \
  {                                              \
    if (swapflag)                                \
      ms_gswap4 (&frame[widx]);                  \
    dnib = EXTRACTBITRANGE (frame[widx], 30, 2); \
  } while (0)

/************************************************************************
 * msr_decode_steim2:
 *
 * Decode Steim2 encoded miniSEED data and place in supplied buffer
 * as 32-bit integers.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_steim2 (int32_t *input, int inputlength, int64_t samplecount,
                   int32_t *output, int64_t outputlength, char *srcname,
                   int swapflag)
{
  int32_t *outputptr = output; /* Pointer to next output sample location */
  uint32_t frame[16];          /* Frame, 16 x 32-bit quantities = 64 bytes */
  int32_t X0 = 0;              /* Forward integration constant, aka first sample */
  int32_t Xn = 0;              /* Reverse integration constant, aka last sample */
  int32_t diff[7];
  int32_t semask;
  int maxframes = inputlength / 64;
  int frameidx;
  int startnibble;
  int nibble;
  int widx;
  int diffcount;
  int dnib;
  int idx;

  union dword
  {
    int8_t d8[4];
    int32_t d32;
  } * word;

  if (inputlength <= 0)
    return 0;

  if (!input || !output || outputlength <= 0 || maxframes <= 0)
    return -1;

  if (libmseed_decodedebug > 0)
    ms_log (0, "Decoding %d Steim2 frames, swapflag: %d, srcname: %s\n",
            maxframes, swapflag, (srcname) ? srcname : "");

  for (frameidx = 0; frameidx < maxframes && samplecount > 0; frameidx++)
  {
    /* Copy frame, each is 16x32-bit quantities = 64 bytes */
    memcpy (frame, input + (16 * frameidx), 64);

    if (frameidx != 0)
    {
      startnibble = 1; /* Subsequent frames: skip nibbles */

      if (libmseed_decodedebug > 0)
        ms_log (0, "Frame %d\n", frameidx);
    }
    else
    {
      if (swapflag)
      {
        ms_gswap4 (&frame[1]);
        ms_gswap4 (&frame[2]);
      }

      X0 = frame[1];
      Xn = frame[2];

      startnibble = 3; /* First frame: skip nibbles, X0, and Xn */

      if (libmseed_decodedebug > 0)
        ms_log (0, "Frame %d: X0=%d  Xn=%d\n", frameidx, X0, Xn);
    }

    /* Swap 32-bit word containing the nibbles */
    if (swapflag)
      ms_gswap4 (&frame[0]);

    /* Decode each 32-bit word according to nibble */
    for (widx = startnibble; widx < 16 && samplecount > 0; widx++)
    {
      /* W0: the first 32-bit quantity contains 16 x 2-bit nibbles */
      nibble = EXTRACTBITRANGE (frame[0], (30 - (2 * widx)), 2);

      if (nibble == 0) /* nibble=00: Special flag, no differences */
      {
        diffcount = 0;

        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 00=special\n", widx);
      }
      else if (nibble == 1) /* nibble=01: Four 1-byte differences */
      {
        diffcount = 4;

        word = (union dword *)&frame[widx];
        for (idx = 0; idx < diffcount; idx++)
        {
          diff[idx] = word->d8[idx];
        }

        if (libmseed_decodedebug > 0)
          ms_log (0, "  W%02d: 01=4x8b  %d  %d  %d  %d\n", widx, diff[0], diff[1], diff[2], diff[3]);
      }
      else if (nibble == 2) /* nibble=10: Must consult dnib, the high order two bits */
      {
        LM_STEIM2_EXTRACT_DNIB;

        switch (dnib)
        {
        case 1: /* nibble=10, dnib=01: One 30-bit difference */
          /* Sign extension from bit 30 */
          LM_STEIM2_EXTRACT_DIFF ("10", "01", 1, 30);
          break;

        case 2: /* nibble=10, dnib=10: Two 15-bit differences */
          /* Sign extension from bit 15 */
          LM_STEIM2_EXTRACT_DIFF ("10", "10", 2, 15);
          break;

        case 3: /* nibble=10, dnib=11: Three 10-bit differences */
          /* Sign extension from bit 10 */
          LM_STEIM2_EXTRACT_DIFF ("10", "11", 3, 10);
          break;
        default: /* nibble=10, dnib=00: Error, undefined value */
          ms_log (2, "%s: Impossible Steim2 dnib=00 for nibble=10\n", srcname);

          return -1;
          break;
        }
      }
      else if (nibble == 3) /* nibble=11: Must consult dnib, the high order two bits */
      {
        LM_STEIM2_EXTRACT_DNIB;

        switch (dnib)
        {
        case 0: /* nibble=11, dnib=00: Five 6-bit differences */
          /* Sign extension from bit 6 */
          LM_STEIM2_EXTRACT_DIFF ("11", "00", 5, 6);
          break;

        case 1: /* nibble=11, dnib=01: Six 5-bit differences */
          /* Sign extension from bit 5 */
          LM_STEIM2_EXTRACT_DIFF ("11", "01", 6, 5);
          break;

        case 2: /* nibble=11, dnib=10: Seven 4-bit differences */
          /* Sign extension from bit 4 */
          LM_STEIM2_EXTRACT_DIFF ("11", "10", 7, 4);
          break;

        default: /* nibble=11, dnib=11: Error, undefined value */
          ms_log (2, "%s: Impossible Steim2 dnib=11 for nibble=11\n", srcname);

          return -1;
          break;
        }
      } /* Done with decoding 32-bit word based on nibble */

      /* Apply differences to calculate output samples */
      if (diffcount > 0)
      {
        for (idx = 0; idx < diffcount && samplecount > 0; idx++, outputptr++)
        {
          if (outputptr != output) /* Store difference from previous sample */
            *outputptr = *(outputptr - 1) + diff[idx];
          else /* Otherwise ignore first difference, instead store X0 */
            *outputptr = X0;

          samplecount--;
        }
      }
    } /* Done looping over nibbles and 32-bit words */
  }   /* Done looping over frames */

  /* Check data integrity by comparing last sample to Xn (reverse integration constant) */
  if (outputptr != output && *(outputptr - 1) != Xn)
  {
    ms_log (1, "%s: Warning: Data integrity check for Steim2 failed, Last sample=%d, Xn=%d\n",
            srcname, *(outputptr - 1), Xn);
  }

  return (outputptr - output);
} /* End of msr_decode_steim2() */

/* Defines for GEOSCOPE encoding */
#define GEOSCOPE_MANTISSA_MASK 0x0FFFul /* mask for mantissa */
#define GEOSCOPE_GAIN3_MASK 0x7000ul    /* mask for gainrange factor */
#define GEOSCOPE_GAIN4_MASK 0xf000ul    /* mask for gainrange factor */
#define GEOSCOPE_SHIFT 12               /* # bits in mantissa */

/************************************************************************
 * msr_decode_geoscope:
 *
 * Decode GEOSCOPE gain ranged data (demultiplexed only) encoded
 * miniSEED data and place in supplied buffer as 32-bit floats.
 *
 * Return number of samples in output buffer on success, -1 on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ************************************************************************/
int
msr_decode_geoscope (char *input, int64_t samplecount, float *output,
                     int64_t outputlength, int encoding,
                     char *srcname, int swapflag)
{
  int idx = 0;
  int mantissa;  /* mantissa from SEED data */
  int gainrange; /* gain range factor */
  int exponent;  /* total exponent */
  int k;
  uint64_t exp2val;
  int16_t sint;
  double dsample = 0.0;

  union
  {
    uint8_t b[4];
    uint32_t i;
  } sample32;

  if (!input || !output)
    return -1;

  if (samplecount <= 0 || outputlength <= 0)
    return -1;

  /* Make sure we recognize this as a GEOSCOPE encoding format */
  if (encoding != DE_GEOSCOPE24 &&
      encoding != DE_GEOSCOPE163 &&
      encoding != DE_GEOSCOPE164)
  {
    ms_log (2, "%s: unrecognized GEOSCOPE encoding: %d\n",
            srcname, encoding);
    return -1;
  }

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (float); idx++)
  {
    switch (encoding)
    {
    case DE_GEOSCOPE24:
      sample32.i = 0;
      if (swapflag)
        for (k = 0; k < 3; k++)
          sample32.b[2 - k] = input[k];
      else
        for (k = 0; k < 3; k++)
          sample32.b[1 + k] = input[k];

      mantissa = sample32.i;

      /* Take 2's complement for mantissa for overflow */
      if ((unsigned long)mantissa > MAX24)
        mantissa -= 2 * (MAX24 + 1);

      /* Store */
      dsample = (double)mantissa;

      break;
    case DE_GEOSCOPE163:
      memcpy (&sint, input, sizeof (int16_t));
      if (swapflag)
        ms_gswap2 (&sint);

      /* Recover mantissa and gain range factor */
      mantissa  = (sint & GEOSCOPE_MANTISSA_MASK);
      gainrange = (sint & GEOSCOPE_GAIN3_MASK) >> GEOSCOPE_SHIFT;

      /* Exponent is just gainrange for GEOSCOPE */
      exponent = gainrange;

      /* Calculate sample as mantissa / 2^exponent */
      exp2val = (uint64_t)1 << exponent;
      dsample = ((double)(mantissa - 2048)) / exp2val;

      break;
    case DE_GEOSCOPE164:
      memcpy (&sint, input, sizeof (int16_t));
      if (swapflag)
        ms_gswap2 (&sint);

      /* Recover mantissa and gain range factor */
      mantissa  = (sint & GEOSCOPE_MANTISSA_MASK);
      gainrange = (sint & GEOSCOPE_GAIN4_MASK) >> GEOSCOPE_SHIFT;

      /* Exponent is just gainrange for GEOSCOPE */
      exponent = gainrange;

      /* Calculate sample as mantissa / 2^exponent */
      exp2val = (uint64_t)1 << exponent;
      dsample = ((double)(mantissa - 2048)) / exp2val;

      break;
    }

    /* Save sample in output array */
    output[idx] = (float)dsample;
    outputlength -= sizeof (float);

    /* Increment edata pointer depending on size */
    switch (encoding)
    {
    case DE_GEOSCOPE24:
      input += 3;
      break;
    case DE_GEOSCOPE163:
    case DE_GEOSCOPE164:
      input += 2;
      break;
    }
  }

  return idx;
} /* End of msr_decode_geoscope() */

/* Defines for CDSN encoding */
#define CDSN_MANTISSA_MASK 0x3FFFul  /* mask for mantissa */
#define CDSN_GAINRANGE_MASK 0xC000ul /* mask for gainrange factor */
#define CDSN_SHIFT 14                /* # bits in mantissa */

/************************************************************************
 * msr_decode_cdsn:
 *
 * Decode CDSN gain ranged data encoded miniSEED data and place in
 * supplied buffer as 32-bit integers.
 *
 * Notes from original rdseed routine:
 * CDSN data are compressed according to the formula
 *
 * sample = M * (2 exp G)
 *
 * where
 *    sample = seismic data sample
 *    M      = mantissa; biased mantissa B is written to tape
 *    G      = exponent of multiplier (i.e. gain range factor);
 *                     key K is written to tape
 *    exp    = exponentiation operation
 *    B      = M + 8191, biased mantissa, written to tape
 *    K      = key to multiplier exponent, written to tape
 *                     K may have any of the values 0 - 3, as follows:
 *                     0 => G = 0, multiplier = 2 exp 0 = 1
 *                     1 => G = 2, multiplier = 2 exp 2 = 4
 *                     2 => G = 4, multiplier = 2 exp 4 = 16
 *                     3 => G = 7, multiplier = 2 exp 7 = 128
 *    Data are stored on tape in two bytes as follows:
 *            fedc ba98 7654 3210 = bit number, power of two
 *            KKBB BBBB BBBB BBBB = form of SEED data
 *            where K = key to multiplier exponent and B = biased mantissa
 *
 *    Masks to recover key to multiplier exponent and biased mantissa
 *    from tape are:
 *            fedc ba98 7654 3210 = bit number = power of two
 *            0011 1111 1111 1111 = 0x3fff     = mask for biased mantissa
 *            1100 0000 0000 0000 = 0xc000     = mask for gain range key
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_cdsn (int16_t *input, int64_t samplecount, int32_t *output,
                 int64_t outputlength, int swapflag)
{
  int32_t idx = 0;
  int32_t mantissa;  /* mantissa */
  int32_t gainrange; /* gain range factor */
  int32_t mult = -1; /* multiplier for gain range */
  uint16_t sint;
  int32_t sample;

  if (samplecount <= 0)
    return 0;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (int32_t); idx++)
  {
    memcpy (&sint, &input[idx], sizeof (int16_t));
    if (swapflag)
      ms_gswap2 (&sint);

    /* Recover mantissa and gain range factor */
    mantissa  = (sint & CDSN_MANTISSA_MASK);
    gainrange = (sint & CDSN_GAINRANGE_MASK) >> CDSN_SHIFT;

    /* Determine multiplier from the gain range factor and format definition
     * because shift operator is used later, these are powers of two */
    if (gainrange == 0)
      mult = 0;
    else if (gainrange == 1)
      mult = 2;
    else if (gainrange == 2)
      mult = 4;
    else if (gainrange == 3)
      mult = 7;

    /* Unbias the mantissa */
    mantissa -= MAX14;

    /* Calculate sample from mantissa and multiplier using left shift
     * mantissa << mult is equivalent to mantissa * (2 exp (mult)) */
    sample = ((uint32_t)mantissa << mult);

    /* Save sample in output array */
    output[idx] = sample;
    outputlength -= sizeof (int32_t);
  }

  return idx;
} /* End of msr_decode_cdsn() */

/* Defines for SRO encoding */
#define SRO_MANTISSA_MASK 0x0FFFul  /* mask for mantissa */
#define SRO_GAINRANGE_MASK 0xF000ul /* mask for gainrange factor */
#define SRO_SHIFT 12                /* # bits in mantissa */

/************************************************************************
 * msr_decode_sro:
 *
 * Decode SRO gain ranged data encoded miniSEED data and place in
 * supplied buffer as 32-bit integers.
 *
 * Notes from original rdseed routine:
 * SRO data are represented according to the formula
 *
 * sample = M * (b exp {[m * (G + agr)] + ar})
 *
 * where
 *     sample = seismic data sample
 *     M      = mantissa
 *     G      = gain range factor
 *     b      = base to be exponentiated = 2 for SRO
 *     m      = multiplier  = -1 for SRO
 *     agr    = term to be added to gain range factor = 0 for SRO
 *     ar     = term to be added to [m * (gr + agr)]  = 10 for SRO
 *     exp    = exponentiation operation
 *     Data are stored in two bytes as follows:
 *     	fedc ba98 7654 3210 = bit number, power of two
 *     	GGGG MMMM MMMM MMMM = form of SEED data
 *     	where G = gain range factor and M = mantissa
 *     Masks to recover gain range and mantissa:
 *     	fedc ba98 7654 3210 = bit number = power of two
 *     	0000 1111 1111 1111 = 0x0fff     = mask for mantissa
 *     	1111 0000 0000 0000 = 0xf000     = mask for gain range
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_sro (int16_t *input, int64_t samplecount, int32_t *output,
                int64_t outputlength, char *srcname, int swapflag)
{
  int32_t idx = 0;
  int32_t mantissa;   /* mantissa */
  int32_t gainrange;  /* gain range factor */
  int32_t add2gr;     /* added to gainrage factor */
  int32_t mult;       /* multiplier for gain range */
  int32_t add2result; /* added to multiplied gain rage */
  int32_t exponent;   /* total exponent */
  uint16_t sint;
  int32_t sample;

  if (samplecount <= 0)
    return 0;

  add2gr     = 0;
  mult       = -1;
  add2result = 10;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (int32_t); idx++)
  {
    memcpy (&sint, &input[idx], sizeof (int16_t));
    if (swapflag)
      ms_gswap2 (&sint);

    /* Recover mantissa and gain range factor */
    mantissa  = (sint & SRO_MANTISSA_MASK);
    gainrange = (sint & SRO_GAINRANGE_MASK) >> SRO_SHIFT;

    /* Take 2's complement for mantissa */
    if ((unsigned long)mantissa > MAX12)
      mantissa -= 2 * (MAX12 + 1);

    /* Calculate exponent, SRO exponent = 0..10 */
    exponent = (mult * (gainrange + add2gr)) + add2result;

    if (exponent < 0 || exponent > 10)
    {
      ms_log (2, "%s: SRO gain ranging exponent out of range: %d\n", srcname, exponent);
      return MS_GENERROR;
    }

    /* Calculate sample as mantissa * 2^exponent */
    sample = mantissa * ((uint64_t)1 << exponent);

    /* Save sample in output array */
    output[idx] = sample;
    outputlength -= sizeof (int32_t);
  }

  return idx;
} /* End of msr_decode_sro() */

/************************************************************************
 * msr_decode_dwwssn:
 *
 * Decode DWWSSN encoded miniSEED data and place in supplied buffer
 * as 32-bit integers.
 *
 * Return number of samples in output buffer on success, -1 on error.
 ************************************************************************/
int
msr_decode_dwwssn (int16_t *input, int64_t samplecount, int32_t *output,
                   int64_t outputlength, int swapflag)
{
  int32_t idx = 0;
  int32_t sample;
  uint16_t sint;

  if (samplecount < 0)
    return 0;

  for (idx = 0; idx < samplecount && outputlength >= (int)sizeof (int32_t); idx++)
  {
    memcpy (&sint, &input[idx], sizeof (uint16_t));
    if (swapflag)
      ms_gswap2 (&sint);
    sample = (int32_t)sint;

    /* Take 2's complement for sample */
    if ((unsigned long)sample > MAX16)
      sample -= 2 * (MAX16 + 1);

    /* Save sample in output array */
    output[idx] = sample;
    outputlength -= sizeof (int32_t);
  }

  return idx;
} /* End of msr_decode_dwwssn() */
