/***************************************************************************
 * Interface declarations for the miniSEED packing routines in
 * packdata.c
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

#ifndef PACKDATA_H
#define PACKDATA_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"

#define STEIM1_FRAME_MAX_SAMPLES 60
#define STEIM2_FRAME_MAX_SAMPLES 105

extern int64_t msr_encode_text (char *input, uint64_t samplecount, char *output,
                                uint64_t outputlength);
extern int64_t msr_encode_int16 (int32_t *input, uint64_t samplecount, int16_t *output,
                                 uint64_t outputlength, int swapflag);
extern int64_t msr_encode_int32 (int32_t *input, uint64_t samplecount, int32_t *output,
                                 uint64_t outputlength, int swapflag);
extern int64_t msr_encode_float32 (float *input, uint64_t samplecount, float *output,
                                   uint64_t outputlength, int swapflag);
extern int64_t msr_encode_float64 (double *input, uint64_t samplecount, double *output,
                                   uint64_t outputlength, int swapflag);
extern int64_t msr_encode_steim1 (int32_t *input, uint64_t samplecount, int32_t *output,
                                  uint64_t outputlength, int32_t diff0, uint32_t *byteswritten,
                                  int swapflag);
extern int64_t msr_encode_steim2 (int32_t *input, uint64_t samplecount, int32_t *output,
                                  uint64_t outputlength, int32_t diff0, uint32_t *byteswritten,
                                  const char *sid, int swapflag);

#ifdef __cplusplus
}
#endif

#endif
