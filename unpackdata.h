/***************************************************************************
 * unpackdata.h:
 *
 * Interface declarations for the miniSEED unpacking routines in
 * unpackdata.c
 ***************************************************************************/

#ifndef UNPACKDATA_H
#define UNPACKDATA_H 1

#ifdef __cplusplus
extern "C" {
#endif

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
