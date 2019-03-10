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

#include "libmseed.h"

extern int msr3_unpack_mseed3 (char *record, int reclen, MS3Record **ppmsr,
                               uint32_t flags, int8_t verbose);
extern int msr3_unpack_mseed2 (char *record, int reclen, MS3Record **ppmsr,
                               uint32_t flags, int8_t verbose);

extern double ms_nomsamprate (int factor, int multiplier);
extern char *ms2_recordsid (char *record, char *sid, int sidlen);
extern const char *ms2_blktdesc (uint16_t blkttype);
uint16_t ms2_blktlen (uint16_t blkttype, const char *blkt, int8_t swapflag);

#ifdef __cplusplus
}
#endif

#endif
