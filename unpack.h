/***************************************************************************
 * unpack.h:
 *
 * Interface declarations for the miniSEED unpacking routines in unpack.c
 ***************************************************************************/

#ifndef UNPACK_H
#define UNPACK_H 1

#ifdef __cplusplus
extern "C" {
#endif

extern int msr3_unpack_mseed3 (char *record, int reclen, MSRecord **ppmsr,
                               int8_t dataflag, int8_t verbose);
extern int msr3_unpack_mseed2 (char *record, int reclen, MSRecord **ppmsr,
                               int8_t dataflag, int8_t verbose);

#ifdef __cplusplus
}
#endif

#endif
