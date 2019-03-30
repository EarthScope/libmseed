/***************************************************************************
 * Interface declarations for the miniSEED packing routines in
 * packdata.c
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

#ifndef PACKDATA_H
#define PACKDATA_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"

#define STEIM1_FRAME_MAX_SAMPLES 60
#define STEIM2_FRAME_MAX_SAMPLES 105

/* Control for printing debugging information, declared in packdata.c */
extern int libmseed_encodedebug;

extern int msr_encode_text (char *input, int samplecount, char *output,
                            int outputlength);
extern int msr_encode_int16 (int32_t *input, int samplecount, int16_t *output,
                             int outputlength, int swapflag);
extern int msr_encode_int32 (int32_t *input, int samplecount, int32_t *output,
                             int outputlength, int swapflag);
extern int msr_encode_float32 (float *input, int samplecount, float *output,
                               int outputlength, int swapflag);
extern int msr_encode_float64 (double *input, int samplecount, double *output,
                               int outputlength, int swapflag);
extern int msr_encode_steim1 (int32_t *input, int samplecount, int32_t *output,
                              int outputlength, int32_t diff0, uint16_t *byteswritten,
                              int swapflag);
extern int msr_encode_steim2 (int32_t *input, int samplecount, int32_t *output,
                              int outputlength, int32_t diff0, uint16_t *byteswritten,
                              char *sid, int swapflag);

#ifdef __cplusplus
}
#endif

#endif
