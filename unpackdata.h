/***************************************************************************
 * Interface declarations for the miniSEED unpacking routines in
 * unpackdata.c
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2024 Chad Trabant, EarthScope Data Services
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
 ***************************************************************************/

#ifndef UNPACKDATA_H
#define UNPACKDATA_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"

extern int64_t msr_decode_int16 (int16_t *input, uint64_t samplecount, int32_t *output,
                                 uint64_t outputlength, int swapflag);
extern int64_t msr_decode_int32 (int32_t *input, uint64_t samplecount, int32_t *output,
                                 uint64_t outputlength, int swapflag);
extern int64_t msr_decode_float32 (float *input, uint64_t samplecount, float *output,
                                   uint64_t outputlength, int swapflag);
extern int64_t msr_decode_float64 (double *input, uint64_t samplecount, double *output,
                                   uint64_t outputlength, int swapflag);
extern int64_t msr_decode_steim1 (int32_t *input, uint64_t inputlength, uint64_t samplecount,
                                  int32_t *output, uint64_t outputlength, const char *srcname,
                                  int swapflag);
extern int64_t msr_decode_steim2 (int32_t *input, uint64_t inputlength, uint64_t samplecount,
                                  int32_t *output, uint64_t outputlength, const char *srcname,
                                  int swapflag);
extern int64_t msr_decode_geoscope (char *input, uint64_t samplecount, float *output,
                                    uint64_t outputlength, int encoding, const char *srcname,
                                    int swapflag);
extern int64_t msr_decode_cdsn (int16_t *input, uint64_t samplecount, int32_t *output,
                                uint64_t outputlength, int swapflag);
extern int64_t msr_decode_sro (int16_t *input, uint64_t samplecount, int32_t *output,
                               uint64_t outputlength, const char *srcname, int swapflag);
extern int64_t msr_decode_dwwssn (int16_t *input, uint64_t samplecount, int32_t *output,
                                  uint64_t outputlength, int swapflag);

#ifdef __cplusplus
}
#endif

#endif
