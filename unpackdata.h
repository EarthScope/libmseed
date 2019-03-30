/***************************************************************************
 * Interface declarations for the miniSEED unpacking routines in
 * unpackdata.c
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2019 Chad Trabant, IRIS Data Management Center
 *
 * The miniSEED Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * The miniSEED Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License (GNU-LGPL) for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software. If not, see
 * <https://www.gnu.org/licenses/>
 ***************************************************************************/

#ifndef UNPACKDATA_H
#define UNPACKDATA_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"

/* Control for printing debugging information, declared in unpackdata.c */
extern int libmseed_decodedebug;

extern int msr_decode_int16 (int16_t *input, int64_t samplecount, int32_t *output,
                             int64_t outputlength, int swapflag);
extern int msr_decode_int32 (int32_t *input, int64_t samplecount, int32_t *output,
                             int64_t outputlength, int swapflag);
extern int msr_decode_float32 (float *input, int64_t samplecount, float *output,
                               int64_t outputlength, int swapflag);
extern int msr_decode_float64 (double *input, int64_t samplecount, double *output,
                               int64_t outputlength, int swapflag);
extern int msr_decode_steim1 (int32_t *input, int inputlength, int64_t samplecount,
                              int32_t *output, int64_t outputlength, char *srcname,
                              int swapflag);
extern int msr_decode_steim2 (int32_t *input, int inputlength, int64_t samplecount,
                              int32_t *output, int64_t outputlength, char *srcname,
                              int swapflag);
extern int msr_decode_geoscope (char *input, int64_t samplecount, float *output,
                                int64_t outputlength, int encoding, char *srcname,
                                int swapflag);
extern int msr_decode_cdsn (int16_t *input, int64_t samplecount, int32_t *output,
                            int64_t outputlength, int swapflag);
extern int msr_decode_sro (int16_t *input, int64_t samplecount, int32_t *output,
                           int64_t outputlength, char *srcname, int swapflag);
extern int msr_decode_dwwssn (int16_t *input, int64_t samplecount, int32_t *output,
                              int64_t outputlength, int swapflag);

#ifdef __cplusplus
}
#endif

#endif
