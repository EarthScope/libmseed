/***************************************************************************
 * Internal state definitions for the miniSEED Library
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

#ifndef INTERNALSTATE_H
#define INTERNALSTATE_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include "libmseed.h"

/* Generator-style packing context for MS3Record (opaque in public header) */
struct MS3RecordPacker
{
  const MS3Record *msr;        /* Source/template record (not owned) */
  uint32_t flags;              /* Packing flags */
  int8_t verbose;              /* Logging level */

  char *rawrec;                /* Allocated record buffer */
  char *encoded;               /* Encoded data buffer */
  uint32_t maxreclen;          /* Max record length */
  int64_t packed_samples;      /* Total samples packed so far */
  uint32_t recordcount;        /* Records generated so far */
  uint8_t encoding;            /* Data encoding */
  int dataoffset;              /* Offset to data payload, and header size, in bytes */
  uint32_t maxsamples;         /* Max samples per record */
  uint32_t maxdatabytes;       /* Max data bytes per record */
  uint8_t samplesize;          /* Size of each sample */
  int8_t swapflag;             /* Byte swap flag */
  int8_t formatversion;        /* 2 or 3 */
  nstime_t nextstarttime;      /* Start time for next record */
  uint16_t blockette_1000_offset; /* Offset to B1000 (miniSEED 2) */
  uint16_t blockette_1001_offset; /* Offset to B1001 (miniSEED 2) */
  uint8_t finished;            /* Packing complete flag */
};

/* Generator-style packing context for MS3TraceList (opaque in public header) */
struct MS3TraceListPacker
{
  MS3TraceList *mstl;          /* Source trace list */
  int reclen;                  /* Max record length */
  int8_t encoding;             /* Data encoding */
  uint32_t flags;              /* Packing flags */
  int8_t verbose;              /* Logging level */
  char *extra;                 /* Extra headers */
  nstime_t flush_idle_nanoseconds; /* Idle flush threshold */

  MS3TraceID *current_id;      /* Current trace ID */
  MS3TraceSeg *current_seg;    /* Current segment */
  MS3RecordPacker *seg_packing_state; /* Current segment packing state */
  MS3Record msr_template;      /* Template MS3Record for current segment */
  int64_t segpackedsamples;    /* Samples packed from current segment */
  int64_t totalpackedsamples;  /* Total samples packed */
  int64_t totalpackedrecords;  /* Total records packed */
  uint8_t finished;            /* Packing complete flag */
};

#ifdef __cplusplus
}
#endif

#endif
