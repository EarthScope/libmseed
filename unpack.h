/***************************************************************************
 * Interface declarations for the miniSEED unpacking routines in unpack.c
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
