/***************************************************************************
 * Internal state definitions for the miniSEED Library
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2026 Chad Trabant, EarthScope Data Services
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
  uint32_t rawrec_size;        /* Allocated size of rawrec, may exceed maxreclen */
  char *encoded;               /* Encoded data buffer */
  uint32_t encoded_size;       /* Allocated size of encoded, may exceed maxdatabytes */
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
  MS3TraceID *last_id;         /* Trace ID of last completed segment (MSF_MAINTAINMSTL) */
  MS3TraceSeg *last_seg;       /* Last completed segment (MSF_MAINTAINMSTL) */
  MS3RecordPacker *seg_packing_state; /* Current segment packing state, NULL when idle */
  MS3RecordPacker seg_packer;  /* Storage for seg_packing_state, reused across segments */
  MS3Record msr_template;      /* Template MS3Record for current segment */
  int64_t segpackedsamples;    /* Samples packed from current segment */
  int64_t totalpackedsamples;  /* Total samples packed */
  int64_t totalpackedrecords;  /* Total records packed */

  char resume_sid[LM_SIDLEN];  /* SID of last completed segment, scan resumes at or after it */
  uint8_t resume_pubversion;   /* Publication version of resume_sid, used as a tie-breaker */
  int8_t resume_valid;         /* Set once resume_sid/resume_pubversion hold a usable hint */
};

/* Test whether a record with the given geometry cannot hold numsamples,
 * without allocating or writing anything; always returns 0 (not
 * determined) for miniSEED 2, whose data offset depends on the blockette
 * layout built by msr3_pack_header2_offsets() */
extern int lm_pack_short_of_record (int8_t formatversion, uint32_t maxreclen, size_t sidlength,
                                    uint16_t extralength, uint8_t encoding, uint8_t samplesize,
                                    int64_t numsamples);

/* Start (or restart) a packing session in a caller-allocated ::MS3RecordPacker,
 * reusing its rawrec/encoded buffers when already large enough; returns 0 on
 * success and -1 on error */
extern int lm_pack_state_init (MS3RecordPacker *packer, const MS3Record *msr, uint32_t flags,
                               int8_t verbose);

/* Report the total samples packed by a session and end it, retaining the
 * packer's buffers for reuse by a later lm_pack_state_init() */
extern void lm_pack_state_finish (MS3RecordPacker *packer, int64_t *packedsamples);

/* Release a packer's rawrec/encoded buffers */
extern void lm_pack_state_free (MS3RecordPacker *packer);

/* Open a record list file path for reading, without a byte range suffix
 * if the path as given does not exist; returns NULL on error */
extern FILE *lm_fopen_recordfile (const char *path);

/* Number of most-recently-active segments tracked per MS3TraceID, used to
 * bound the segment-list search in _mstl3_addmsr_impl() */
#define LM_RECENTSEGS 4

/* Maximum hops walked when resolving list order among recent segments or
 * falling back to a direct list walk, beyond which the fast path is
 * refused in favor of a full scan */
#define LM_RECENTSEGS_MAXWALK 8

/* Tag in the otherwise unused source ID of a trace list's head node, set by
 * mstl3_init() and cleared when an MS3TraceID not allocated by this library
 * may be present.  The leading control byte is never part of a source ID. */
#define LM_TRACELIST_TAG "\001libmseed trace list"

/* Test whether a trace list was initialized by mstl3_init() and holds only
 * trace IDs allocated by this library as LMTraceIDNode */
static inline int
lm_tracelist_native (const MS3TraceList *mstl)
{
  return memcmp (mstl->traces.sid, LM_TRACELIST_TAG, sizeof (LM_TRACELIST_TAG)) == 0;
}

/* Private extension of MS3TraceID (opaque in public header).
 *
 * Tracks the most-recently-active segments of a trace ID (the "recent set")
 * and nonrecentendbound, an upper bound on the end time of every segment
 * NOT in the recent set.  The bound may overestimate, which only costs
 * search performance, but must never underestimate, which would cause a
 * match to be missed.  It starts at INT64_MIN and only ever rises, folding
 * in the end time of segments as they are evicted from the recent set or
 * removed from the trace list.
 *
 * The seen_* fields hold the trace ID's segment count and first and last
 * segments as of the library's last change to its segment list.  A
 * mismatch means the list was changed by the caller, and the recent set,
 * whose pointers may no longer be valid, is rebuilt before use.
 *
 * The public struct is the first member so public pointers, sizeof, and
 * field offsets are unaffected by the extension. */
typedef struct
{
  MS3TraceID id;
  MS3TraceSeg *recentseg[LM_RECENTSEGS];
  nstime_t nonrecentendbound;
  uint32_t seen_numsegments;
  MS3TraceSeg *seen_first;
  MS3TraceSeg *seen_last;
} LMTraceIDNode;

#ifdef __cplusplus
}
#endif

#endif
