/***************************************************************************
 * Routines to handle TraceList and related structures.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "internalstate.h"

static MS3TraceSeg *lm_msr2seg (const MS3Record *msr, nstime_t endtime);
static MS3TraceSeg *lm_addmsrtoseg (MS3TraceSeg *seg, const MS3Record *msr, nstime_t endtime,
                                    int8_t whence);
static MS3TraceSeg *lm_addsegtoseg (MS3TraceSeg *seg1, MS3TraceSeg *seg2);
static MS3RecordPtr *lm_add_recordptr (MS3TraceSeg *seg, const MS3Record *msr, nstime_t endtime,
                                       int8_t whence);

static void lm_free_segment_memory (MS3TraceSeg *seg, int8_t freeprvtptr);
static int lm_remove_segment (MS3TraceList *mstl, MS3TraceID *id, MS3TraceSeg *seg,
                              int8_t freeprvtptr);
static uint32_t lm_lcg_r (uint64_t *state);
static uint8_t lm_random_height (uint8_t maximum, uint64_t *state);

/* Test if two sample rates are similar using either specified tolerance (if positive) or default
 * tolerance */
#define IS_SAMPRATE_SIMILAR(SR1, SR2, SRT) \
  ((SRT > 0.0) ? fabs (SR1 - SR2) > SRT : MS_ISRATETOLERABLE (SR1, SR2))

/* Test if a MS3TraceSeg represents time coverage */
#define SEGMENT_HAS_TIME_COVERAGE(seg) ((seg)->samplecnt > 0 && (seg)->samprate != 0.0)

/** ************************************************************************
 * @brief Initialize a ::MS3TraceList container
 *
 * A new ::MS3TraceList is allocated if needed. If the supplied
 * ::MS3TraceList is not NULL it will be re-initialized and any
 * associated memory will be freed including data at @p prvtptr pointers.
 *
 * @param[in] mstl ::MS3TraceList to reinitialize or NULL
 *
 * @returns a pointer to a MS3TraceList struct on success or NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_free()
 ***************************************************************************/
MS3TraceList *
mstl3_init (MS3TraceList *mstl)
{
  if (mstl)
  {
    mstl3_free (&mstl, 1);
  }

  mstl = (MS3TraceList *)libmseed_memory.malloc (sizeof (MS3TraceList));

  if (mstl == NULL)
  {
    ms_log (2, "Cannot allocate memory\n");
    return NULL;
  }

  memset (mstl, 0, sizeof (MS3TraceList));

  /* Seed PRNG with 1, we only need random distribution */
  mstl->prngstate = 1;
  mstl->traces.height = MSTRACEID_SKIPLIST_HEIGHT;

  return mstl;
} /* End of mstl3_init() */

/** ************************************************************************
 * @brief Free all memory associated with a ::MS3TraceList
 *
 * The pointer to the target ::MS3TraceList will be set to NULL.
 *
 * @param[in] ppmstl Pointer-to-pointer to the target ::MS3TraceList to free
 * @param[in] freeprvtptr If true, also free any data at the @p
 * prvtptr members of ::MS3TraceID.prvtptr, ::MS3TraceSeg.prvtptr, and
 * ::MS3RecordPtr.prvtptr (in ::MS3TraceSeg.recordlist)
 ***************************************************************************/
void
mstl3_free (MS3TraceList **ppmstl, int8_t freeprvtptr)
{
  MS3TraceID *id = NULL;
  MS3TraceID *nextid = NULL;
  MS3TraceSeg *seg = NULL;
  MS3TraceSeg *nextseg = NULL;

  if (!ppmstl)
    return;

  /* Free any associated traces */
  id = (*ppmstl)->traces.next[0];
  while (id)
  {
    nextid = id->next[0];

    /* Free any associated trace segments */
    seg = id->first;
    while (seg)
    {
      nextseg = seg->next;

      /* Free all memory associated with the segment */
      lm_free_segment_memory (seg, freeprvtptr);

      seg = nextseg;
    }

    /* Free private pointer data if present and requested */
    if (freeprvtptr && id->prvtptr)
      libmseed_memory.free (id->prvtptr);

    libmseed_memory.free (id);

    id = nextid;
  }

  libmseed_memory.free (*ppmstl);

  *ppmstl = NULL;

  return;
} /* End of mstl3_free() */

/** ************************************************************************
 * @brief Find matching ::MS3TraceID in a ::MS3TraceList
 *
 * Return the ::MS3TraceID matching the @p sid in the specified ::MS3TraceList.
 *
 * If @p prev is not NULL, set pointers to previous entries for the
 * expected location of the trace ID.  Useful for adding a new ID
 * with mstl3_addID(), and should be set to @p NULL otherwise.
 *
 * @param[in] mstl Pointer to the ::MS3TraceList to search
 * @param[in] sid Source ID to search for in the list
 * @param[in] pubversion If non-zero, find the entry with this version
 * @param[out] prev Pointers to previous entries in expected location or NULL
 *
 * @returns a pointer to the matching ::MS3TraceID or NULL if not found or error.
 ***************************************************************************/
MS3TraceID *
mstl3_findID (MS3TraceList *mstl, const char *sid, uint8_t pubversion, MS3TraceID **prev)
{
  MS3TraceID *id = NULL;
  int level;
  int cmp;

  if (!mstl || !sid)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl' or 'sid'\n", __func__);
    return NULL;
  }

  level = MSTRACEID_SKIPLIST_HEIGHT - 1;

  /* Search trace ID skip list, starting from the head/sentinel node */
  id = &(mstl->traces);
  while (id != NULL && level >= 0)
  {
    if (prev != NULL) /* Track previous entries at each level */
    {
      prev[level] = id;
    }

    if (id->next[level] == NULL)
    {
      level -= 1;
    }
    else
    {
      cmp = strcmp (id->next[level]->sid, sid);

      /* If source IDs match, check publication if matching requested */
      if (!cmp && pubversion && id->next[level]->pubversion != pubversion)
      {
        cmp = (id->next[level]->pubversion < pubversion) ? -1 : 1;
      }

      if (cmp == 0) /* Found matching trace ID */
      {
        return id->next[level];
      }
      else if (cmp > 0) /* Drop a level */
      {
        level -= 1;
      }
      else /* Continue at this level */
      {
        id = id->next[level];
      }
    }
  }

  return NULL;
} /* End of mstl3_findID() */

/** ************************************************************************
 * @brief Add ::MS3TraceID to a ::MS3TraceList
 *
 * The @p prev array is the list of pointers to previous entries at different
 * levels of the skip list.  It is common to first search the list using
 * mstl3_findID() which returns this list of pointers for use here.
 * If this value is NULL mstl3_findID() will be run to find the pointers.
 *
 * @param[in] mstl Add ID to this ::MS3TraceList
 * @param[in] id The ::MS3TraceID to add
 * @param[in] prev Pointers to previous entries in expected location, can be NULL
 *
 * @returns a pointer to the added ::MS3TraceID or NULL on error.
 *
 * @see mstl3_findID()
 ***************************************************************************/
MS3TraceID *
mstl3_addID (MS3TraceList *mstl, MS3TraceID *id, MS3TraceID **prev)
{
  MS3TraceID *local_prev[MSTRACEID_SKIPLIST_HEIGHT] = {NULL};
  int level;

  if (!mstl || !id)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl' or 'id'\n", __func__);
    return NULL;
  }

  /* If previous list pointers not supplied, find them */
  if (!prev)
  {
    mstl3_findID (mstl, id->sid, 0, local_prev);
    prev = local_prev;
  }

  /* Set level of new entry to a random level within head height */
  id->height = lm_random_height (MSTRACEID_SKIPLIST_HEIGHT, &(mstl->prngstate));

  /* Set all pointers above new entry level to NULL */
  for (level = MSTRACEID_SKIPLIST_HEIGHT - 1; level > id->height; level--)
  {
    id->next[level] = NULL;
  }

  /* Connect previous and new ID pointers */
  for (level = id->height - 1; level >= 0; level--)
  {
    if (!prev[level])
    {
      ms_log (2, "No previous pointer at level %d for adding SID %s\n", level, id->sid);
      return NULL;
    }

    id->next[level] = prev[level]->next[level];
    prev[level]->next[level] = id;
  }

  mstl->numtraceids++;

  return id;
} /* End of mstl3_addID() */

/***************************************************************************
 * Implementation of MS3TraceList addition functions
 *
 * @see mstl3_addmsr()
 * @see mstl3_addmsr_recordptr()
 ***************************************************************************/
MS3TraceSeg *
_mstl3_addmsr_impl (MS3TraceList *mstl, const MS3Record *msr, MS3RecordPtr **pprecptr,
                    int8_t splitversion, int8_t autoheal, uint32_t flags,
                    const MS3Tolerance *tolerance)
{
  MS3TraceID *id = NULL;
  MS3TraceID *previd[MSTRACEID_SKIPLIST_HEIGHT] = {NULL};

  MS3TraceSeg *seg = NULL;
  MS3TraceSeg *searchseg = NULL;
  MS3TraceSeg *segbefore = NULL;
  MS3TraceSeg *segafter = NULL;
  MS3TraceSeg *followseg = NULL;

  nstime_t endtime;
  nstime_t pregap;
  nstime_t postgap;
  nstime_t lastgap;
  nstime_t firstgap;
  nstime_t nsperiod;
  nstime_t nstimetol = 0;
  nstime_t nnstimetol = 0;

  double sampratehz;
  double sampratetol = -1.0;

  if (!mstl || !msr)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl' or 'msr'\n", __func__);
    return NULL;
  }

  /* Calculate end time for MS3Record */
  if ((endtime = msr3_endtime (msr)) == NSTERROR)
  {
    ms_log (2, "Error calculating record end time\n");
    return NULL;
  }

  /* If splitversion is true and MSF_SPLITISVERSION is set in flags, use splitversion
   * as the version, otherwise use msr->pubversion */
  int8_t pubversion = (flags & MSF_SPLITISVERSION) ? splitversion : msr->pubversion;

  /* Search for matching trace ID */
  id = mstl3_findID (mstl, msr->sid, (splitversion) ? pubversion : 0, previd);

  /* If no matching ID was found create new MS3TraceID and MS3TraceSeg entries */
  if (!id)
  {
    if (!(id = (MS3TraceID *)libmseed_memory.malloc (sizeof (MS3TraceID))))
    {
      ms_log (2, "Error allocating memory\n");
      return NULL;
    }
    memset (id, 0, sizeof (MS3TraceID));

    /* Populate MS3TraceID */
    memcpy (id->sid, msr->sid, sizeof (id->sid));
    id->pubversion = pubversion;
    id->earliest = msr->starttime;
    id->latest = endtime;
    id->numsegments = 1;

    if (!(seg = lm_msr2seg (msr, endtime)))
    {
      return NULL;
    }
    id->first = id->last = seg;

    /* Add MS3RecordPtr if requested */
    if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 1)))
    {
      return NULL;
    }

    /* Add new MS3TraceID to MS3TraceList */
    if (mstl3_addID (mstl, id, previd) == NULL)
    {
      ms_log (2, "Error adding new ID to trace list\n");
      return NULL;
    }
  }
  /* Add data coverage to the matching MS3TraceID */
  else
  {
    /* Calculate nanosecond sample period */
    nsperiod = msr3_nsperiod (msr);

    /* Calculate high-precision time tolerance */
    if (tolerance && tolerance->time)
      nstimetol = (nstime_t)(NSTMODULUS * tolerance->time (msr));
    else
      nstimetol = (nstime_t)(0.5 * nsperiod); /* Default time tolerance is 1/2 sample period */

    nnstimetol = (nstimetol) ? -nstimetol : 0;

    /* Calculate sample rate tolerance */
    if (tolerance && tolerance->samprate)
      sampratetol = tolerance->samprate (msr);

    sampratehz = msr3_sampratehz (msr);

    /* last/firstgap are negative when the record overlaps the trace
     * segment and positive when there is a time gap. */

    /* Gap relative to the last segment */
    lastgap = msr->starttime - id->last->endtime - nsperiod;

    /* Gap relative to the first segment */
    firstgap = id->first->starttime - endtime - nsperiod;

    /* Search first for the simple scenarios in order of likelihood:
     * - Record fits at end of last segment
     * - Record fits after all coverage
     * - Record fits before all coverage
     * - Record fits at beginning of first segment
     *
     * If none of those scenarios are true search the complete segment list.
     */

    /* Record coverage fits at end of last segment */
    if (lastgap <= nstimetol && lastgap >= nnstimetol &&
        IS_SAMPRATE_SIMILAR (sampratehz, id->last->samprate, sampratetol) &&
        SEGMENT_HAS_TIME_COVERAGE (id->last))
    {
      if (!lm_addmsrtoseg (id->last, msr, endtime, 1))
        return NULL;

      seg = id->last;

      if (endtime > id->latest)
        id->latest = endtime;

      /* Add MS3RecordPtr if requested */
      if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 1)))
        return NULL;
    }
    /* Record coverage is after all other coverage */
    else if ((msr->starttime - nsperiod - nstimetol) > id->latest)
    {
      if (!(seg = lm_msr2seg (msr, endtime)))
        return NULL;

      /* Add to end of list */
      id->last->next = seg;
      seg->prev = id->last;
      id->last = seg;
      id->numsegments++;

      if (endtime > id->latest)
        id->latest = endtime;

      /* Add MS3RecordPtr if requested */
      if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 0)))
        return NULL;
    }
    /* Record coverage is before all other coverage */
    else if ((endtime + nsperiod + nstimetol) < id->earliest)
    {
      if (!(seg = lm_msr2seg (msr, endtime)))
        return NULL;

      /* Add to beginning of list */
      id->first->prev = seg;
      seg->next = id->first;
      id->first = seg;
      id->numsegments++;

      if (msr->starttime < id->earliest)
        id->earliest = msr->starttime;

      /* Add MS3RecordPtr if requested */
      if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 0)))
        return NULL;
    }
    /* Record coverage fits at beginning of first segment */
    else if (firstgap <= nstimetol && firstgap >= nnstimetol &&
             IS_SAMPRATE_SIMILAR (sampratehz, id->first->samprate, sampratetol) &&
             SEGMENT_HAS_TIME_COVERAGE (id->first))
    {
      if (!lm_addmsrtoseg (id->first, msr, endtime, 2))
        return NULL;

      seg = id->first;

      if (msr->starttime < id->earliest)
        id->earliest = msr->starttime;

      /* Add MS3RecordPtr if requested */
      if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 2)))
        return NULL;
    }
    /* Search complete segment list for matches */
    else
    {
      /* This search finds the following values if they exist: */
      segbefore = NULL; /* The first segment end that matches the record start (within tolerance) */
      segafter = NULL;  /* The first segment start that matches the record end (within tolerance) */
      followseg = NULL; /* The segment with latest start time before the record start */
      searchseg = id->first;
      while (searchseg)
      {
        /* Skip segments with no time coverage, these cannot be extended */
        if (!SEGMENT_HAS_TIME_COVERAGE (searchseg))
        {
          searchseg = searchseg->next;
          continue;
        }

        /* Done searching if autohealing and record exactly matches a segment.
         *
         * Rationale: autohealing would have combined this segment
         * with another if that were possible, so this record will
         * also not fit with any other segment. */
        if (autoheal && msr->starttime == searchseg->starttime && endtime == searchseg->endtime)
        {
          followseg = searchseg;
          break;
        }

        if (msr->starttime > searchseg->starttime)
          followseg = searchseg;

        if (!segbefore)
        {
          postgap = msr->starttime - searchseg->endtime - nsperiod;

          if (postgap <= nstimetol && postgap >= nnstimetol &&
              IS_SAMPRATE_SIMILAR (sampratehz, searchseg->samprate, sampratetol))
            segbefore = searchseg;
        }

        if (!segafter)
        {
          pregap = searchseg->starttime - endtime - nsperiod;

          if (pregap <= nstimetol && pregap >= nnstimetol &&
              IS_SAMPRATE_SIMILAR (sampratehz, searchseg->samprate, sampratetol))
            segafter = searchseg;
        }

        /* Done searching if both before and after segments are found */
        if (segbefore && segafter)
          break;
        /* Done searching if not autohealing and one match found */
        else if (!autoheal && (segbefore || segafter))
          break;

        searchseg = searchseg->next;
      } /* Done looping through segments */

      /* Add MS3Record coverage to end of segment before */
      if (segbefore)
      {
        if (!lm_addmsrtoseg (segbefore, msr, endtime, 1))
        {
          return NULL;
        }

        /* Add MS3RecordPtr if requested */
        if (pprecptr && !(*pprecptr = lm_add_recordptr (segbefore, msr, endtime, 1)))
        {
          return NULL;
        }

        /* Merge two segments that now fit if autohealing */
        if (autoheal && segafter && segbefore != segafter)
        {
          /* Add segafter coverage to segbefore */
          if (!lm_addsegtoseg (segbefore, segafter))
          {
            return NULL;
          }

          /* Shift last segment pointer if it's going to be removed */
          if (segafter == id->last)
            id->last = id->last->prev;

          /* Remove segafter from list */
          if (segafter->prev)
            segafter->prev->next = segafter->next;
          if (segafter->next)
            segafter->next->prev = segafter->prev;

          /* Free all memory associated with the segment after that has been merged */
          lm_free_segment_memory (segafter, 1);

          id->numsegments -= 1;
        }

        seg = segbefore;
      }
      /* Add MS3Record coverage to beginning of segment after */
      else if (segafter)
      {
        if (!lm_addmsrtoseg (segafter, msr, endtime, 2))
        {
          return NULL;
        }

        /* Add MS3RecordPtr if requested */
        if (pprecptr && !(*pprecptr = lm_add_recordptr (segafter, msr, endtime, 2)))
        {
          return NULL;
        }

        seg = segafter;
      }
      /* Add MS3Record coverage to new segment */
      else
      {
        /* Create new segment */
        if (!(seg = lm_msr2seg (msr, endtime)))
        {
          return NULL;
        }

        /* Add MS3RecordPtr if requested */
        if (pprecptr && !(*pprecptr = lm_add_recordptr (seg, msr, endtime, 0)))
        {
          return NULL;
        }

        /* Add new segment as first in list */
        if (!followseg)
        {
          seg->next = id->first;
          if (id->first)
            id->first->prev = seg;

          id->first = seg;
        }
        /* Add new segment after the followseg segment */
        else
        {
          seg->next = followseg->next;
          seg->prev = followseg;
          if (followseg->next)
            followseg->next->prev = seg;
          followseg->next = seg;

          if (followseg == id->last)
            id->last = seg;
        }

        id->numsegments++;
      }
    } /* End of searching segment list */

    /* Track largest publication version */
    if (pubversion > id->pubversion)
      id->pubversion = pubversion;

    /* Track earliest and latest times */
    if (msr->starttime < id->earliest)
      id->earliest = msr->starttime;

    if (endtime > id->latest)
      id->latest = endtime;
  } /* End of adding coverage to matching ID */

  /* Sort modified segment into place, logic above should limit these to few shifts if any */
  while (seg->next &&
         (seg->starttime > seg->next->starttime ||
          (seg->starttime == seg->next->starttime && seg->endtime < seg->next->endtime)))
  {
    /* Move segment down list, swap seg and seg->next */
    segafter = seg->next;

    if (seg->prev)
      seg->prev->next = segafter;

    if (segafter->next)
      segafter->next->prev = seg;

    segafter->prev = seg->prev;
    seg->prev = segafter;
    seg->next = segafter->next;
    segafter->next = seg;

    /* Reset first and last segment pointers if replaced */
    if (id->first == seg)
      id->first = segafter;

    if (id->last == segafter)
      id->last = seg;
  }
  while (seg->prev &&
         (seg->starttime < seg->prev->starttime ||
          (seg->starttime == seg->prev->starttime && seg->endtime > seg->prev->endtime)))
  {
    /* Move segment up list, swap seg and seg->prev */
    segbefore = seg->prev;

    if (seg->next)
      seg->next->prev = segbefore;

    if (segbefore->prev)
      segbefore->prev->next = seg;

    segbefore->next = seg->next;
    seg->next = segbefore;
    seg->prev = segbefore->prev;
    segbefore->prev = seg;

    /* Reset first and last segment pointers if replaced */
    if (id->first == segbefore)
      id->first = seg;

    if (id->last == seg)
      id->last = segbefore;
  }

  /* Store update time at seg.prvtptr, allocate if needed */
  if (seg && flags & MSF_PPUPDATETIME)
  {
    if (!seg->prvtptr)
    {
      if (!(seg->prvtptr = libmseed_memory.malloc (sizeof (nstime_t))))
      {
        ms_log (2, "Error allocating memory\n");
        return NULL;
      }
    }

    /* Set to current time */
    *(nstime_t *)seg->prvtptr = lmp_systemtime ();
  }

  return seg;
} /* End of _mstl3_addmsr_impl() */

/** ************************************************************************
 * @brief Add data coverage from an ::MS3Record to a ::MS3TraceList
 *
 * Searching the list for the appropriate ::MS3TraceID and ::MS3TraceSeg and
 * either add data to existing entries or create new ones as needed.
 *
 * As this routine adds data to a trace list it constructs continuous time
 * series, merging segments when possible.  The @p tolerance pointer to a
 * ::MS3Tolerance identifies function pointers that are expected to return time
 * tolerance, in seconds, and sample rate tolerance, in Hertz, for the given
 * ::MS3Record.  If @p tolerance is NULL, or the function pointers it identifies
 * are NULL, default tolerances will be used as follows: - Default time
 * tolerance is 1/2 the sampling period - Default sample rate (sr) tolerance is
 * abs(1‐sr1/sr2) < 0.0001
 *
 * In recommended usage, the @p splitversion flag is @b 0 and different
 * publication versions of otherwise matching data are merged.  If more than one
 * version contributes to a given source identifier's segments, its
 * ::MS3TraceID.pubversion will be the set to the largest contributing version.
 * If the @p splitversion flag is @b 1 the publication versions will be kept
 * separate with each version isolated in separate ::MS3TraceID entries.
 *
 * @note The ::MS3TraceList data structure is designed as a container of data
 * sample payloads (integers, floats, text) unrelated to miniSEED records.  Many
 * of the details in a miniSEED record that are not relevant to represent the
 * series are not stored in the trace list, and therefore, will not be included
 * in an output such as miniSEED produced by mstl3_pack().
 *
 * While header-only miniSEED records, with no data payload, can be added to a
 * trace list they will be represented as empty segments with no time coverage
 * for informational purposes.
 *
 * If the ::MSF_PPUPDATETIME flag is set in @p flags, the update time of the
 * segment will be stored at ::MS3TraceSeg.prvtptr.  This update time is used to
 * determine if the segment is idle and should be flushed by
 * mstl3_pack_ppupdate_flushidle(). If this flag is set, ensure to free the
 * memory using mstl3_free() with the @p freeprvtptr parameter set to 1.
 *
 * @param[in] mstl Destination ::MS3TraceList to add data to
 * @param[in] msr ::MS3Record containing the data to add to list
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] autoheal Flag to control automatic merging of segments
 * @param[in] flags Flags to control optional functionality
 * @parblock
 *  - @c ::MSF_PPUPDATETIME : Store update time (as nstime_t) at ::MS3TraceSeg.prvtptr
 *  - @c ::MSF_SPLITISVERSION : Use @p splitversion as the version, otherwise use msr->pubversion
 * @endparblock
 * @param[in] tolerance Tolerance function pointers as ::MS3Tolerance
 *
 * @returns a pointer to the ::MS3TraceSeg updated or NULL on error.
 *
 * @see mstl3_addmsr_recordptr()
 * @see mstl3_readbuffer()
 * @see ms3_readtracelist()
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
MS3TraceSeg *
mstl3_addmsr (MS3TraceList *mstl, const MS3Record *msr, int8_t splitversion, int8_t autoheal,
              uint32_t flags, const MS3Tolerance *tolerance)
{
  return _mstl3_addmsr_impl (mstl, msr, NULL, splitversion, autoheal, flags, tolerance);
}

/** ************************************************************************
 * @copydoc mstl3_addmsr()
 *
 * This function is identical to mstl3_addmsr() but with the additional @p
 * pprecptr parameter:
 *
 * @param[in] pprecptr Pointer to pointer to a ::MS3RecordPtr for @ref record-list
 *
 * If the @p pprecptr is not NULL a @ref record-list will be maintained for each
 * segment.  If the value of @p *pprecptr is NULL, a new ::MS3RecordPtr will be
 * allocated, otherwise the supplied structure will be updated.  The
 * ::MS3RecordPtr will be added to the appropriate record list and the values of
 * ::MS3RecordPtr.msr and ::MS3RecordPtr.endtime will be set, all other fields
 * should be set by the caller.
 ***************************************************************************/
MS3TraceSeg *
mstl3_addmsr_recordptr (MS3TraceList *mstl, const MS3Record *msr, MS3RecordPtr **pprecptr,
                        int8_t splitversion, int8_t autoheal, uint32_t flags,
                        const MS3Tolerance *tolerance)
{
  return _mstl3_addmsr_impl (mstl, msr, pprecptr, splitversion, autoheal, flags, tolerance);
}

/** ************************************************************************
 * @brief Parse miniSEED from a buffer and populate a ::MS3TraceList
 *
 * For a full description of @p tolerance see mstl3_addmsr().
 *
 * If the ::MSF_UNPACKDATA flag is set in @p flags, the data samples
 * will be unpacked.  In most cases the caller probably wants this
 * flag set, without it the trace list will merely be a list of
 * channels.
 *
 * If the ::MSF_RECORDLIST flag is set in @p flags, a ::MS3RecordList
 * will be built for each ::MS3TraceSeg.  The ::MS3RecordPtr entries
 * contain the location of the data record, bit flags, extra headers, etc.
 *
 * @param[in] ppmstl Pointer-to-point to destination MS3TraceList
 * @param[in] buffer Source buffer to read miniSEED records from
 * @param[in] bufferlength Maximum length of @p buffer
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] flags Flags to control parsing and optional functionality:
 * @parblock
 *  - @c ::MSF_RECORDLIST : Build a ::MS3RecordList for each ::MS3TraceSeg
 *  - Flags supported by msr3_parse()
 *  - Flags supported by mstl3_addmsr()
 * @endparblock
 * @param[in] tolerance Tolerance function pointers as ::MS3Tolerance
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns The number of records parsed on success, otherwise a
 * negative library error code.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_addmsr()
 ***************************************************************************/
int64_t
mstl3_readbuffer (MS3TraceList **ppmstl, const char *buffer, uint64_t bufferlength,
                  int8_t splitversion, uint32_t flags, const MS3Tolerance *tolerance,
                  int8_t verbose)
{
  return mstl3_readbuffer_selection (ppmstl, buffer, bufferlength, splitversion, flags, tolerance,
                                     NULL, verbose);
} /* End of mstl3_readbuffer() */

/** ************************************************************************
 * @brief Parse miniSEED from a buffer and populate a ::MS3TraceList
 *
 * For a full description of @p tolerance see mstl3_addmsr().
 *
 * If the ::MSF_UNPACKDATA flag is set in @p flags, the data samples
 * will be unpacked.  In most cases the caller probably wants this
 * flag set, without it the trace list will merely be a list of
 * channels.
 *
 * If the ::MSF_RECORDLIST flag is set in @p flags, a ::MS3RecordList
 * will be built for each ::MS3TraceSeg.  The ::MS3RecordPtr entries
 * contain the location of the data record, bit flags, extra headers, etc.
 *
 * If @p selections is not NULL, the ::MS3Selections will be used to
 * limit what is returned to the caller.  Any data not matching the
 * selections will be skipped.
 *
 * @param[in] ppmstl Pointer-to-point to destination MS3TraceList
 * @param[in] buffer Source buffer to read miniSEED records from
 * @param[in] bufferlength Maximum length of @p buffer
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] flags Flags to control parsing and optional functionality:
 * @parblock
 *  - @c ::MSF_RECORDLIST : Build a ::MS3RecordList for each ::MS3TraceSeg
 *  - Flags supported by msr3_parse()
 *  - Flags supported by mstl3_addmsr()
 * @endparblock
 * @param[in] tolerance Tolerance function pointers as ::MS3Tolerance
 * @param[in] selections Specify limits to which data should be returned, see @ref data-selections
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns The number of records parsed on success, otherwise a
 * negative library error code.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_addmsr()
 ***************************************************************************/
int64_t
mstl3_readbuffer_selection (MS3TraceList **ppmstl, const char *buffer, uint64_t bufferlength,
                            int8_t splitversion, uint32_t flags, const MS3Tolerance *tolerance,
                            const MS3Selections *selections, int8_t verbose)
{
  MS3Record *msr = NULL;
  MS3TraceSeg *seg = NULL;
  MS3RecordPtr *recordptr = NULL;
  uint32_t dataoffset;
  uint32_t datasize;
  uint64_t offset = 0;
  uint32_t pflags = flags;
  int64_t reccount = 0;
  int parsevalue;

  if (!ppmstl)
  {
    ms_log (2, "%s(): Required input not defined: 'ppmstl'\n", __func__);
    return MS_GENERROR;
  }

  /* Initialize MS3TraceList if needed */
  if (!*ppmstl)
  {
    *ppmstl = mstl3_init (*ppmstl);

    if (!*ppmstl)
      return MS_GENERROR;
  }

  /* Defer data unpacking if selections are used by unsetting MSF_UNPACKDATA */
  if ((flags & MSF_UNPACKDATA) && selections)
    pflags &= ~(MSF_UNPACKDATA);

  while ((bufferlength - offset) > MINRECLEN)
  {
    parsevalue = msr3_parse (buffer + offset, bufferlength - offset, &msr, pflags, verbose);

    if (parsevalue < 0)
    {
      if (msr)
        msr3_free (&msr);

      return parsevalue;
    }

    if (parsevalue > 0)
      break;

    /* Test data against selections if specified */
    if (selections)
    {
      if (!ms3_matchselect (selections, msr->sid, msr->starttime, msr3_endtime (msr),
                            msr->pubversion, NULL))
      {
        if (verbose > 1)
        {
          ms_log (0,
                  "Skipping (selection) record for %s (%d bytes) starting at offset %" PRIu64 "\n",
                  msr->sid, msr->reclen, offset);
        }

        offset += msr->reclen;
        continue;
      }

      /* Unpack data samples if this has been deferred */
      if ((flags & MSF_UNPACKDATA) && msr->samplecnt > 0)
      {
        if (msr3_unpack_data (msr, verbose) != msr->samplecnt)
        {
          if (msr)
            msr3_free (&msr);

          return MS_GENERROR;
        }
      }
    }

    /* Add record to trace list */
    seg = mstl3_addmsr_recordptr (*ppmstl, msr, (flags & MSF_RECORDLIST) ? &recordptr : NULL,
                                  splitversion, 1, flags, tolerance);

    if (seg == NULL)
    {
      if (msr)
        msr3_free (&msr);

      return MS_GENERROR;
    }

    /* Populate remaining fields of record pointer */
    if (recordptr)
    {
      /* Determine offset to data and length of data payload */
      if (msr3_data_bounds (msr, &dataoffset, &datasize))
        return MS_GENERROR;

      recordptr->bufferptr = buffer + offset;
      recordptr->fileptr = NULL;
      recordptr->filename = NULL;
      recordptr->fileoffset = 0;
      recordptr->dataoffset = dataoffset;
      recordptr->prvtptr = NULL;
    }

    reccount += 1;
    offset += msr->reclen;
  }

  if (msr)
    msr3_free (&msr);

  return reccount;
} /* End of mstl3_readbuffer_selection() */

/***************************************************************************
 * Create an MS3TraceSeg structure from an MS3Record structure.
 *
 * Return a pointer to a MS3TraceSeg otherwise NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static MS3TraceSeg *
lm_msr2seg (const MS3Record *msr, nstime_t endtime)
{
  MS3TraceSeg *seg = NULL;
  size_t datasize = 0;
  int samplesize;

  if (!msr)
  {
    ms_log (2, "%s(): Required input not defined: 'msr'\n", __func__);
    return NULL;
  }

  if (!(seg = (MS3TraceSeg *)libmseed_memory.malloc (sizeof (MS3TraceSeg))))
  {
    ms_log (2, "Error allocating memory\n");
    return NULL;
  }
  memset (seg, 0, sizeof (MS3TraceSeg));

  /* Populate MS3TraceSeg */
  seg->starttime = msr->starttime;
  seg->endtime = endtime;
  seg->samprate = msr3_sampratehz (msr);
  seg->samplecnt = msr->samplecnt;
  seg->sampletype = msr->sampletype;
  seg->numsamples = msr->numsamples;

  /* Allocate space for and copy datasamples */
  if (msr->datasamples && msr->numsamples)
  {
    if (!(samplesize = ms_samplesize (msr->sampletype)))
    {
      ms_log (2, "Unknown sample size for sample type: %c\n", msr->sampletype);
      return NULL;
    }

    datasize = samplesize * msr->numsamples;

    if (!(seg->datasamples = libmseed_memory.malloc ((size_t)(datasize))))
    {
      ms_log (2, "Error allocating memory\n");
      return NULL;
    }
    seg->datasize = datasize;

    /* Copy data samples from MS3Record to MS3TraceSeg */
    memcpy (seg->datasamples, msr->datasamples, datasize);
  }

  return seg;
} /* End of lm_msr2seg() */

/***************************************************************************
 * Add data coverage from a MS3Record structure to a MS3TraceSeg structure.
 *
 * Data coverage is added to the beginning or end of MS3TraceSeg
 * according to the whence flag:
 * 1 : add coverage to the end
 * 2 : add coverage to the beginninig
 *
 * Return a pointer to a MS3TraceSeg otherwise, NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static MS3TraceSeg *
lm_addmsrtoseg (MS3TraceSeg *seg, const MS3Record *msr, nstime_t endtime, int8_t whence)
{
  int samplesize = 0;
  void *newdatasamples = NULL;
  size_t newdatasize = 0;

  if (!seg || !msr)
  {
    ms_log (2, "%s(): Required input not defined: 'seg' or 'msr'\n", __func__);
    return NULL;
  }

  /* Allocate more memory for data samples if included */
  if (msr->datasamples && msr->numsamples > 0)
  {
    if (msr->sampletype != seg->sampletype)
    {
      ms_log (2, "MS3Record sample type (%c) does not match segment sample type (%c)\n",
              msr->sampletype, seg->sampletype);
      return NULL;
    }

    if (!(samplesize = ms_samplesize (msr->sampletype)))
    {
      ms_log (2, "Unknown sample size for sample type: %c\n", msr->sampletype);
      return NULL;
    }

    newdatasize = (seg->numsamples + msr->numsamples) * samplesize;

    if (libmseed_prealloc_block_size)
    {
      size_t current_size = seg->datasize;
      newdatasamples = libmseed_memory_prealloc (seg->datasamples, newdatasize, &current_size);
      seg->datasize = current_size;
    }
    else
    {
      newdatasamples = libmseed_memory.realloc (seg->datasamples, newdatasize);
      seg->datasize = newdatasize;
    }

    if (!newdatasamples)
    {
      ms_log (2, "Error allocating memory\n");
      seg->datasize = 0;
      return NULL;
    }

    seg->datasamples = newdatasamples;
  }

  /* Add coverage to end of segment */
  if (whence == 1)
  {
    seg->endtime = endtime;
    seg->samplecnt += msr->samplecnt;

    if (msr->datasamples && msr->numsamples > 0)
    {
      memcpy ((char *)seg->datasamples + (seg->numsamples * samplesize), msr->datasamples,
              (size_t)(msr->numsamples * samplesize));

      seg->numsamples += msr->numsamples;
    }
  }
  /* Add coverage to beginning of segment */
  else if (whence == 2)
  {
    seg->starttime = msr->starttime;
    seg->samplecnt += msr->samplecnt;

    if (msr->datasamples && msr->numsamples > 0)
    {
      memmove ((char *)seg->datasamples + (msr->numsamples * samplesize), seg->datasamples,
               (size_t)(seg->numsamples * samplesize));

      memcpy (seg->datasamples, msr->datasamples, (size_t)(msr->numsamples * samplesize));

      seg->numsamples += msr->numsamples;
    }
  }
  else
  {
    ms_log (2, "unrecognized whence value: %d\n", whence);
    return NULL;
  }

  return seg;
} /* End of lm_addmsrtoseg() */

/***************************************************************************
 * Add data coverage from seg2 to seg1.
 *
 * Return a pointer to a seg1 otherwise NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static MS3TraceSeg *
lm_addsegtoseg (MS3TraceSeg *seg1, MS3TraceSeg *seg2)
{
  int samplesize = 0;
  void *newdatasamples = NULL;
  size_t newdatasize = 0;

  if (!seg1 || !seg2)
  {
    ms_log (2, "%s(): Required input not defined: 'seg1' or 'seg2'\n", __func__);
    return NULL;
  }

  /* Allocate more memory for data samples if included */
  if (seg2->datasamples && seg2->numsamples > 0)
  {
    if (seg2->sampletype != seg1->sampletype)
    {
      ms_log (2, "MS3TraceSeg sample types do not match (%c and %c)\n", seg1->sampletype,
              seg2->sampletype);
      return NULL;
    }

    if (!(samplesize = ms_samplesize (seg1->sampletype)))
    {
      ms_log (2, "Unknown sample size for sample type: %c\n", seg1->sampletype);
      return NULL;
    }

    newdatasize = (seg1->numsamples + seg2->numsamples) * samplesize;

    if (libmseed_prealloc_block_size)
    {
      size_t current_size = seg1->datasize;
      newdatasamples = libmseed_memory_prealloc (seg1->datasamples, newdatasize, &current_size);
      seg1->datasize = current_size;
    }
    else
    {
      newdatasamples = libmseed_memory.realloc (seg1->datasamples, newdatasize);
      seg1->datasize = newdatasize;
    }

    if (!newdatasamples)
    {
      ms_log (2, "Error allocating memory\n");
      seg1->datasize = 0;
      return NULL;
    }

    seg1->datasamples = newdatasamples;
  }

  /* Add seg2 coverage to end of seg1 */
  seg1->endtime = seg2->endtime;
  seg1->samplecnt += seg2->samplecnt;

  if (seg2->datasamples && seg2->numsamples > 0)
  {
    memcpy ((char *)seg1->datasamples + (seg1->numsamples * samplesize), seg2->datasamples,
            (size_t)(seg2->numsamples * samplesize));

    seg1->numsamples += seg2->numsamples;
  }

  /* Add seg2 record list to end of seg1 record list */
  if (seg2->recordlist)
  {
    if (seg1->recordlist == NULL)
    {
      seg1->recordlist = seg2->recordlist;
    }
    else
    {
      seg1->recordlist->last->next = seg2->recordlist->first;
      seg1->recordlist->last = seg2->recordlist->last;
      seg1->recordlist->recordcnt += seg2->recordlist->recordcnt;

      /* Free record list container */
      libmseed_memory.free (seg2->recordlist);
    }

    seg2->recordlist = NULL;
  }

  return seg1;
} /* End of lm_addsegtoseg() */

/** ************************************************************************
 * @brief Add a ::MS3RecordPtr to the ::MS3RecordList of a ::MS3TraceSeg
 *
 * @param[in] seg ::MS3TraceSeg to add record to
 * @param[in] msr ::MS3Record to be added, for record length and start/end times
 * @param[in] endtime Time of last sample in record
 * @param[in] whence Where to add record to list
 * @parblock
 *  - @c 0 : New entry for new list, only when seg->recordlist == NULL
 *  - @c 1 : Add record pointer to end of list
 *  - @c 2 : Add record pointer to beginning of list
 * @endparblock
 *
 * @returns Pointer to added ::MS3RecordPtr on success and NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_unpack_recordlist()
 * @see mstl3_readbuffer()
 * @see mstl3_readbuffer_selection()
 * @see ms3_readtracelist()
 * @see ms3_readtracelist_timewin()
 * @see ms3_readtracelist_selection()
 * @see mstl3_addmsr()
 ***************************************************************************/
static MS3RecordPtr *
lm_add_recordptr (MS3TraceSeg *seg, const MS3Record *msr, nstime_t endtime, int8_t whence)
{
  MS3RecordPtr *recordptr = NULL;

  if (!seg || !msr)
  {
    ms_log (2, "%s(): Required input not defined: 'seg' or 'msr'\n", __func__);
    return NULL;
  }

  if (seg->recordlist && whence != 1 && whence != 2)
  {
    ms_log (2, "Unsupported 'whence' value: %d\n", whence);
    return NULL;
  }

  recordptr = (MS3RecordPtr *)libmseed_memory.malloc (sizeof (MS3RecordPtr));

  if (recordptr == NULL)
  {
    ms_log (2, "Cannot allocate memory\n");
    return NULL;
  }

  memset (recordptr, 0, sizeof (MS3RecordPtr));
  recordptr->msr = msr3_duplicate (msr, 0);
  recordptr->endtime = endtime;

  if (recordptr->msr == NULL)
  {
    ms_log (2, "Cannot duplicate MS3Record\n");
    libmseed_memory.free (recordptr);
    return NULL;
  }

  /* If no record list for the segment is present, allocate and add record pointer */
  if (seg->recordlist == NULL)
  {
    seg->recordlist = (MS3RecordList *)libmseed_memory.malloc (sizeof (MS3RecordList));

    if (seg->recordlist == NULL)
    {
      ms_log (2, "Cannot allocate memory\n");
      libmseed_memory.free (recordptr);
      return NULL;
    }

    seg->recordlist->recordcnt = 1;
    seg->recordlist->first = recordptr;
    seg->recordlist->last = recordptr;
  }
  /* Otherwise, add record pointer to existing list */
  else
  {
    /* Beginning of list */
    if (whence == 2)
    {
      recordptr->next = seg->recordlist->first;
      seg->recordlist->first = recordptr;
    }
    /* End of list */
    else
    {
      seg->recordlist->last->next = recordptr;
      seg->recordlist->last = recordptr;
    }

    seg->recordlist->recordcnt += 1;
  }

  return recordptr;
} /* End of lm_add_recordptr() */

/** ************************************************************************
 * @brief Convert the data samples associated with an MS3TraceSeg to another
 * data type
 *
 * Text data samples cannot be converted, if supplied or requested an
 * error will be returned.
 *
 * When converting float & double sample types to integer type a
 * simple rounding is applied by adding 0.5 to the sample value before
 * converting (truncating) to integer.  This compensates for common
 * machine representations of floating point values, e.g. "40.0"
 * represented by "39.99999999".
 *
 * If the @p truncate flag is true (non-zero) data samples will be
 * truncated to integers even if loss of sample precision is detected.
 * If the truncate flag is false (zero) and loss of precision is
 * detected an error is returned.  Loss of precision is determined by
 * testing that the difference between the floating point value and
 * the (truncated) integer value is greater than 0.000001.
 *
 * @param[in] seg The target ::MS3TraceSeg to convert
 * @param[in] type The desired data sample type:
 * @parblock
 *   - @c 'i' - 32-bit integer data type
 *   - @c 'f' - 32-bit float data type
 *   - @c 'd' - 64-bit float (double) data type
 * @endparblock
 * @param[in] truncate Control truncation of floating point values to integers
 *
 * @returns 0 on success, and -1 on failure.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mstl3_convertsamples (MS3TraceSeg *seg, char type, int8_t truncate)
{
  int32_t *idata;
  float *fdata;
  double *ddata;
  int64_t idx;

  if (!seg)
  {
    ms_log (2, "%s(): Required input not defined: 'seg'\n", __func__);
    return -1;
  }

  /* No conversion necessary, report success */
  if (seg->sampletype == type)
    return 0;

  if (seg->sampletype == 't' || type == 't' || seg->sampletype == 'a' || type == 'a')
  {
    ms_log (2, "Cannot convert text samples to/from numeric type\n");
    return -1;
  }

  idata = (int32_t *)seg->datasamples;
  fdata = (float *)seg->datasamples;
  ddata = (double *)seg->datasamples;

  /* Convert to 32-bit integers */
  if (type == 'i')
  {
    if (seg->sampletype == 'f') /* Convert floats to integers with simple rounding */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
      {
        /* Check for loss of sub-integer */
        if (!truncate && (fdata[idx] - (int32_t)fdata[idx]) > 0.000001)
        {
          ms_log (2, "Loss of precision when converting floats to integers, loss: %g\n",
                  (fdata[idx] - (int32_t)fdata[idx]));
          return -1;
        }

        /* Round to nearest integer, handling positive and negative values */
        idata[idx] = (int32_t)(fdata[idx] + (fdata[idx] >= 0 ? 0.5 : -0.5));
      }
    }
    else if (seg->sampletype == 'd') /* Convert doubles to integers with simple rounding */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
      {
        /* Check for loss of sub-integer */
        if (!truncate && (ddata[idx] - (int32_t)ddata[idx]) > 0.000001)
        {
          ms_log (2, "Loss of precision when converting doubles to integers, loss: %g\n",
                  (ddata[idx] - (int32_t)ddata[idx]));
          return -1;
        }

        /* Round to nearest integer, handling positive and negative values */
        idata[idx] = (int32_t)(ddata[idx] + (ddata[idx] >= 0 ? 0.5 : -0.5));
      }

      /* Reallocate buffer for reduced size needed, only if not pre-allocating */
      if (libmseed_prealloc_block_size == 0)
      {
        if (!(seg->datasamples = libmseed_memory.realloc (
                  seg->datasamples, (size_t)(seg->numsamples * sizeof (int32_t)))))
        {
          ms_log (2, "Cannot re-allocate buffer for sample conversion\n");
          return -1;
        }
        seg->datasize = seg->numsamples * sizeof (int32_t);
      }
    }

    seg->sampletype = 'i';
  } /* Done converting to 32-bit integers */

  /* Convert to 32-bit floats */
  else if (type == 'f')
  {
    if (seg->sampletype == 'i') /* Convert integers to floats */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        fdata[idx] = (float)idata[idx];
    }
    else if (seg->sampletype == 'd') /* Convert doubles to floats */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        fdata[idx] = (float)ddata[idx];

      /* Reallocate buffer for reduced size needed, only if not pre-allocating */
      if (libmseed_prealloc_block_size == 0)
      {
        if (!(seg->datasamples = libmseed_memory.realloc (
                  seg->datasamples, (size_t)(seg->numsamples * sizeof (float)))))
        {
          ms_log (2, "Cannot re-allocate buffer after sample conversion\n");
          return -1;
        }
        seg->datasize = seg->numsamples * sizeof (float);
      }
    }

    seg->sampletype = 'f';
  } /* Done converting to 32-bit floats */

  /* Convert to 64-bit doubles */
  else if (type == 'd')
  {
    if (!(ddata = (double *)libmseed_memory.malloc ((size_t)(seg->numsamples * sizeof (double)))))
    {
      ms_log (2, "Cannot allocate buffer for sample conversion to doubles\n");
      return -1;
    }

    if (seg->sampletype == 'i') /* Convert integers to doubles */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        ddata[idx] = (double)idata[idx];

      libmseed_memory.free (idata);
    }
    else if (seg->sampletype == 'f') /* Convert floats to doubles */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        ddata[idx] = (double)fdata[idx];

      libmseed_memory.free (fdata);
    }

    seg->datasamples = ddata;
    seg->datasize = seg->numsamples * sizeof (double);
    seg->sampletype = 'd';
  } /* Done converting to 64-bit doubles */

  return 0;
} /* End of mstl3_convertsamples() */

/** ************************************************************************
 * @brief Resize data sample buffers of ::MS3TraceList to what is needed
 *
 * This routine should only be used if pre-allocation of memory, via
 * ::libmseed_prealloc_block_size, was enabled to allocate the buffers.
 *
 * @param[in] mstl ::MS3TraceList to resize buffers
 *
 * @returns Return 0 on success, otherwise returns a libmseed error code.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mstl3_resize_buffers (MS3TraceList *mstl)
{
  MS3TraceID *id = NULL;
  MS3TraceSeg *seg = NULL;
  uint8_t samplesize = 0;
  size_t datasize;

  if (!mstl)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl'\n", __func__);
    return MS_GENERROR;
  }

  /* Loop through trace ID and segment lists */
  id = mstl->traces.next[0];
  while (id)
  {
    seg = id->first;
    while (seg)
    {
      samplesize = ms_samplesize (seg->sampletype);

      if (samplesize && seg->datasamples && seg->numsamples > 0)
      {
        datasize = (size_t)seg->numsamples * samplesize;

        if (seg->datasize > datasize)
        {
          seg->datasamples = libmseed_memory.realloc (seg->datasamples, datasize);

          if (seg->datasamples == NULL)
          {
            ms_log (2, "%s: Cannot (re)allocate memory\n", id->sid);
            return MS_GENERROR;
          }

          seg->datasize = datasize;
        }
      }

      seg = seg->next;
    }

    id = id->next[0];
  }

  return 0;
} /* End of mstl3_resize_buffers() */

/** ************************************************************************
 * @brief Unpack data samples in a @ref record-list associated with a ::MS3TraceList
 *
 * Normally a record list is created calling by ms3_readtracelist() or
 * mstl3_readbuffer() with the ::MSF_RECORDLIST flag.
 *
 * Unpacked data samples are written to the provided @p output buffer
 * (up to @p outputsize bytes).  If @p output is NULL, a buffer will
 * be allocated and associated with the ::MS3TraceSeg, just as if the
 * data were unpacked while constructing the @ref trace-list.
 *
 * The sample type of the decoded data is stored at
 * ::MS3TraceSeg.sampletype (i.e. @c seg->sampletype).
 *
 * A record pointer entry has multiple ways to identify the location
 * of a record: memory buffer, open file (FILE *) or file name.  This
 * routine uses the first populated record location in the following
 * order:
 *   -# Buffer pointer (::MS3RecordPtr.bufferptr)
 *   -# Open file and offset (::MS3RecordPtr.fileptr and ::MS3RecordPtr.fileoffset)
 *   -# File name and offset (::MS3RecordPtr.filename and ::MS3RecordPtr.fileoffset)
 *
 * It would be unusual to build a record list outside of the library,
 * but should that ever occur note that the record list is assumed to
 * be in correct time order and represent a contiguous time series.
 *
 * @param[in] id ::MS3TraceID for relevant ::MS3TraceSeg
 * @param[in] seg ::MS3TraceSeg with associated @ref record-list to unpack
 * @param[out] output Output buffer for data samples, can be NULL
 * @param[in] outputsize Size of @p output buffer
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the number of samples unpacked or -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_readbuffer()
 * @see mstl3_readbuffer_selection()
 * @see ms3_readtracelist()
 * @see ms3_readtracelist_selection()
 ***************************************************************************/
int64_t
mstl3_unpack_recordlist (MS3TraceID *id, MS3TraceSeg *seg, void *output, uint64_t outputsize,
                         int8_t verbose)
{
  MS3RecordPtr *recordptr = NULL;
  int64_t unpackedsamples = 0;
  int64_t totalunpackedsamples = 0;

  char *filebuffer = NULL;
  int64_t filebuffersize = 0;

  uint64_t outputoffset = 0;
  uint64_t decodedsize = 0;
  uint8_t samplesize = 0;
  char sampletype = 0;
  char recsampletype = 0;

  FILE *fileptr = NULL;
  const char *input = NULL;

  /* Linked list of open file pointers */
  struct filelist_s
  {
    const char *filename;
    FILE *fileptr;
    struct filelist_s *next;
  };
  struct filelist_s *filelist = NULL;
  struct filelist_s *filelistptr = NULL;

  if (!id || !seg)
  {
    ms_log (2, "%s(): Required input not defined: 'id' or 'seg'\n", __func__);
    return -1;
  }

  if (!seg->recordlist)
  {
    ms_log (2, "Required record list is not present (seg->recordlist)\n");
    return -1;
  }

  recordptr = seg->recordlist->first;

  if (ms_encoding_sizetype ((uint8_t)recordptr->msr->encoding, &samplesize, &sampletype))
  {
    ms_log (2, "%s: Cannot determine sample size and type for encoding: %u\n", id->sid,
            recordptr->msr->encoding);
    return -1;
  }

  /* Calculate buffer size needed for unpacked samples */
  decodedsize = seg->samplecnt * samplesize;

  /* If output buffer is supplied, check needed size */
  if (output)
  {
    if (decodedsize > outputsize)
    {
      ms_log (2,
              "%s: Output buffer (%" PRIu64 " bytes) is not large enough for decoded data (%" PRIu64
              " bytes)\n",
              id->sid, decodedsize, outputsize);
      return -1;
    }
  }
  /* Otherwise check that buffer is not already allocated  */
  else if (seg->datasamples)
  {
    ms_log (2, "%s: Segment data buffer is already allocated, cannot replace\n", id->sid);
    return -1;
  }
  /* Otherwise allocate new buffer */
  else
  {
    if ((output = libmseed_memory.malloc ((size_t)decodedsize)) == NULL)
    {
      ms_log (2, "%s: Cannot allocate memory for segment data samples\n", id->sid);
      return -1;
    }

    /* Associate allocated memory with segment */
    seg->datasamples = output;
    seg->datasize = decodedsize;
  }

  /* Iterate through record list and unpack data samples */
  while (recordptr)
  {
    /* Skip records with no samples */
    if (recordptr->msr->samplecnt == 0)
    {
      recordptr = recordptr->next;
      continue;
    }

    if (ms_encoding_sizetype ((uint8_t)recordptr->msr->encoding, NULL, &recsampletype))
    {
      ms_log (2, "%s: Cannot determine sample type for encoding: %u\n", id->sid,
              recordptr->msr->encoding);

      totalunpackedsamples = -1;
      break;
    }

    if (recsampletype != sampletype)
    {
      ms_log (2, "%s: Mixed sample types cannot be decoded together: %c versus %c\n", id->sid,
              recsampletype, sampletype);

      totalunpackedsamples = -1;
      break;
    }

    /* Decode data from buffer */
    if (recordptr->bufferptr)
    {
      input = recordptr->bufferptr + recordptr->dataoffset;
    }
    /* Decode data from a file at a byte offset */
    else if (recordptr->fileptr || recordptr->filename)
    {
      if (recordptr->fileptr)
      {
        fileptr = recordptr->fileptr;
      }
      else
      {
        /* Search file list for matching entry */
        filelistptr = filelist;
        while (filelistptr)
        {
          if (filelistptr->filename == recordptr->filename)
            break;

          filelistptr = filelistptr->next;
        }

        /* Add new entry to list and open file if needed */
        if (filelistptr == NULL)
        {
          if ((filelistptr = libmseed_memory.malloc (sizeof (struct filelist_s))) == NULL)
          {
            ms_log (2, "%s: Cannot allocate memory for file list entry for %s\n", id->sid,
                    recordptr->filename);

            totalunpackedsamples = -1;
            break;
          }

          if ((filelistptr->fileptr = fopen (recordptr->filename, "rb")) == NULL)
          {
            ms_log (2, "%s: Cannot open file (%s): %s\n", id->sid, recordptr->filename,
                    strerror (errno));

            totalunpackedsamples = -1;
            break;
          }

          filelistptr->filename = recordptr->filename;
          filelistptr->next = filelist;

          filelist = filelistptr;
        }

        fileptr = filelistptr->fileptr;
      }

      /* Allocate memory if needed, over-allocating (x2) to minimize reallocation */
      if (recordptr->msr->reclen > filebuffersize)
      {
        if ((filebuffer = libmseed_memory.realloc (filebuffer, recordptr->msr->reclen * 2)) == NULL)
        {
          ms_log (2, "%s: Cannot allocate memory for file read buffer\n", id->sid);

          totalunpackedsamples = -1;
          break;
        }

        filebuffersize = recordptr->msr->reclen * 2;
      }

      /* Seek to record position in file */
      if (lmp_fseek64 (fileptr, recordptr->fileoffset, SEEK_SET))
      {
        ms_log (2, "%s: Cannot seek in file: %s (%s)\n", id->sid,
                (recordptr->filename) ? recordptr->filename : "", strerror (errno));

        totalunpackedsamples = -1;
        break;
      }

      /* Read record into buffer */
      if (fread (filebuffer, 1, recordptr->msr->reclen, fileptr) != (size_t)recordptr->msr->reclen)
      {
        ms_log (2, "%s: Cannot read record from file: %s (%s)\n", id->sid,
                (recordptr->filename) ? recordptr->filename : "", strerror (errno));

        totalunpackedsamples = -1;
        break;
      }

      input = filebuffer + recordptr->dataoffset;
    } /* Done reading from file */
    else
    {
      ms_log (2, "%s: No buffer or file pointer for record\n", id->sid);

      totalunpackedsamples = -1;
      break;
    }

    /* Decode data from buffer */
    unpackedsamples = ms_decode_data (
        input, recordptr->msr->reclen - recordptr->dataoffset, (uint8_t)recordptr->msr->encoding,
        recordptr->msr->samplecnt, (unsigned char *)output + outputoffset,
        decodedsize - outputoffset, &sampletype, recordptr->msr->swapflag, id->sid, verbose);

    if (unpackedsamples < 0)
    {
      totalunpackedsamples = -1;
      break;
    }

    outputoffset += unpackedsamples * samplesize;
    totalunpackedsamples += unpackedsamples;

    recordptr = recordptr->next;
  } /* Done with record list entries */

  /* Free file read buffer if used */
  if (filebuffer)
    libmseed_memory.free (filebuffer);

  /* Close and free file list if used */
  while (filelist)
  {
    filelistptr = filelist->next;
    fclose (filelist->fileptr);
    libmseed_memory.free (filelist);
    filelist = filelistptr;
  }

  /* If output buffer was allocated here, do some maintenance */
  if (output == seg->datasamples)
  {
    /* Free allocated memory on error */
    if (totalunpackedsamples < 0)
    {
      libmseed_memory.free (output);
      seg->datasamples = NULL;
      seg->datasize = 0;
    }
    else
    {
      seg->numsamples = totalunpackedsamples;
    }
  }

  if (totalunpackedsamples > 0)
    seg->sampletype = sampletype;

  return totalunpackedsamples;
} /* End of mstl3_unpack_recordlist() */

/***************************************************************************
 * Implementation of MS3TraceList packing for the callback interfaces
 *
 * @see mstl3_pack()
 * @see mstl3_pack_ppupdate_flushidle()
 ***************************************************************************/
int64_t
_mstl3_pack_callback (MS3TraceList *mstl, void (*record_handler) (char *, int, void *),
                      void *handlerdata, int reclen, int8_t encoding, int64_t *packedsamples,
                      uint32_t flags, int8_t verbose, char *extra, uint32_t flush_idle_seconds)
{
  int64_t totalpackedrecords = 0;
  int64_t totalpackedsamples = 0;
  int64_t segpackedrecords = 0;
  int64_t segpackedsamples = 0;
  uint32_t segment_flags = 0;
  nstime_t flush_idle_nanoseconds = (nstime_t)flush_idle_seconds * NSTMODULUS;

  if (!mstl)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl'\n", __func__);
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  if (packedsamples)
    *packedsamples = 0;

  /* Loop through trace list */
  MS3TraceID *id = mstl->traces.next[0];
  while (id && totalpackedrecords >= 0)
  {
    MS3TraceID *nextid = id->next[0]; /* Save next pointer before potential removal */

    /* Loop through segment list */
    MS3TraceSeg *seg = id->first;
    while (seg)
    {
      MS3TraceSeg *nextseg = seg->next; /* Save next pointer before potential removal */
      segment_flags = flags;

      /* If flush_idle_seconds is set and the segment has a prvtptr with an
       * update time set during parsing with the ::MSF_PPUPDATE flag, check if
       * the segement has been updated within the specified number of seconds.
       * If it has not, force the flushing of the segment by setting the
       * ::MSF_FLUSHDATA flag. */
      if (flush_idle_nanoseconds > 0 && seg->prvtptr)
      {
        nstime_t *update_time = (nstime_t *)seg->prvtptr;
        nstime_t update_latency = lmp_systemtime () - *update_time;

        if (update_latency > flush_idle_nanoseconds)
        {
          segment_flags |= MSF_FLUSHDATA;
        }
      }

      segpackedrecords =
          mstl3_pack_segment (mstl, id, seg, record_handler, handlerdata, reclen, encoding,
                              &segpackedsamples, segment_flags, verbose, extra);

      if (segpackedrecords < 0)
      {
        ms_log (2, "%s: Error packing data from segment\n", id->sid);
        totalpackedrecords = -1;
        break;
      }

      totalpackedrecords += segpackedrecords;
      totalpackedsamples += segpackedsamples;

      /* Remove segment if no samples remain and the MSF_MAINTAINMSTL flag is not set */
      if ((flags & MSF_MAINTAINMSTL) == 0 && seg->numsamples == 0)
      {
        lm_remove_segment (mstl, id, seg, 1);
      }

      seg = nextseg;
    }

    id = nextid;
  }

  if (packedsamples)
    *packedsamples = totalpackedsamples;

  return totalpackedrecords;
} /* End of _mstl3_pack_callback() */

/** ************************************************************************
 * @brief Pack ::MS3TraceList data into miniSEED records
 *
 * The datasamples array, numsamples and starttime fields of each
 * trace segment will be adjusted as data are packed unless the
 * ::MSF_MAINTAINMSTL flag is specified in @p flags. If
 * ::MSF_MAINTAINMSTL is specified a caller would also normally set
 * the ::MSF_FLUSHDATA flag to pack all data in the trace list.
 *
 * <b>Use as a rolling buffer to generate data records:</b>
 * The behavior of adjusting the trace list as data are packed is
 * intended to allow using a ::MS3TraceList as an intermediate
 * collection of data buffers to generate data records from an
 * arbitrarily large data source, e.g. continuous data.  In this
 * pattern, data are added to a ::MS3TraceList and mstl3_pack() is
 * called repeatedly.  Data records are only produced if a complete
 * record can be generated, which often leaves small amounts of data
 * in each segment buffer.  On completion or shutdown the caller
 * usually makes a final call to mst3_pack() with the ::MSF_FLUSHDATA
 * flag set to flush all data from the buffers.  Implementing finer
 * control of when data records are generated, such as a time tolerance,
 * can be done using mstraceseg3_pack() directly.
 *
 * As each record is filled and finished they are passed to @p
 * record_handler() which should expect 1) a @c char* to the record,
 * 2) the length of the record and 3) a pointer supplied by the
 * original caller containing optional private data (@p handlerdata).
 * It is the responsibility of @p record_handler() to process the
 * record, the memory will be re-used or freed when @p
 * record_handler() returns.
 *
 * The requested @p encoding value is currently only used for integer
 * data samples. The encoding is set automatially for text and
 * floating point data samples as there is only a single encoding for
 * them.  A value of @c -1 can be used to use the default encoding.
 *
 * If @p extra is not NULL it is expected to contain extraheaders, a
 * string containing (compact) JSON, that will be added to each output
 * record.
 *
 * @param[in] mstl ::MS3TraceList containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the @p record_handler()
 * @param[in] reclen Maximum record length to create
 * @param[in] encoding Encoding for data samples, see msr3_pack()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Bit flags to control packing:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - @c ::MSF_MAINTAINMSTL : Do not remove packed data from the buffer
 *  - @c ::MSF_PACKVER2 : Pack miniSEED version 2 instead of default 3
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 * @param[in] extra If not NULL, add this buffer of extra headers to all records
 *
 * @returns the number of records created on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_pack_segment()
 * @see msr3_pack()
 ***************************************************************************/
int64_t
mstl3_pack (MS3TraceList *mstl, void (*record_handler) (char *, int, void *), void *handlerdata,
            int reclen, int8_t encoding, int64_t *packedsamples, uint32_t flags, int8_t verbose,
            char *extra)
{
  return _mstl3_pack_callback (mstl, record_handler, handlerdata, reclen, encoding, packedsamples,
                               flags, verbose, extra, 0);
}

/** ************************************************************************
 * @copydoc mstl3_pack()
 *
 * This function is identical to mstl3_pack() but with the additional @p
 * flush_idle_seconds parameter:
 *
 * @param[in] flush_idle_seconds If > 0, forces flushing of data segments that
 *                               have not been updated within the specified
 *                               number of seconds.
 ***************************************************************************/
int64_t
mstl3_pack_ppupdate_flushidle (MS3TraceList *mstl, void (*record_handler) (char *, int, void *),
                               void *handlerdata, int reclen, int8_t encoding,
                               int64_t *packedsamples, uint32_t flags, int8_t verbose, char *extra,
                               uint32_t flush_idle_seconds)
{
  return _mstl3_pack_callback (mstl, record_handler, handlerdata, reclen, encoding, packedsamples,
                               flags, verbose, extra, flush_idle_seconds);
}

/** ************************************************************************
 * @brief Initialize a packing state for generator-style trace list packing
 *
 * Create and initialize an opaque ::MS3TraceListPacker context for generating
 * miniSEED records one at a time from an ::MS3TraceList.
 *
 * The packing state should be freed with mstl3_pack_free() when done.
 *
 * If @p flush_idle_seconds is > 0, segments idle longer than the threshold will
 * be flushed automatically. This requires the ::MSF_PPUPDATETIME flag to be set
 * when adding data to the ::MS3TraceList to track update times.
 *
 * @param[in] mstl ::MS3TraceList containing data to pack
 * @param[in] reclen Maximum record length to create
 * @param[in] encoding Encoding for data samples, see msr3_pack()
 * @param[in] flags Bit flags to control packing:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - @c ::MSF_MAINTAINMSTL : Do not remove packed data from the buffer
 *  - @c ::MSF_PACKVER2 : Pack miniSEED version 2 instead of default 3
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 * @param[in] extra If not NULL, add this buffer of extra headers to all records
 * @param[in] flush_idle_seconds If > 0, forces flushing of data segments that
 *                               have not been updated within the specified
 *                               number of seconds.
 *
 * @returns pointer to ::MS3TraceListPacker on success and NULL on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_pack_next()
 * @see mstl3_pack_free()
 ***************************************************************************/
MS3TraceListPacker *
mstl3_pack_init (MS3TraceList *mstl, int reclen, int8_t encoding,
                 uint32_t flags, int8_t verbose, char *extra, uint32_t flush_idle_seconds)
{
  MS3TraceListPacker *packer = NULL;

  if (!mstl)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl'\n", __func__);
    return NULL;
  }

  /* Allocate packing state context */
  packer = (MS3TraceListPacker *)libmseed_memory.malloc (sizeof (MS3TraceListPacker));
  if (!packer)
  {
    ms_log (2, "Cannot allocate memory for trace list packing state context\n");
    return NULL;
  }

  memset (packer, 0, sizeof (MS3TraceListPacker));

  /* Store parameters */
  packer->mstl = mstl;
  packer->reclen = reclen;
  packer->encoding = encoding;
  packer->flags = flags;
  packer->verbose = verbose;
  packer->extra = extra;
  packer->flush_idle_nanoseconds = (nstime_t)flush_idle_seconds * NSTMODULUS;

  if (verbose > 2)
    ms_log (0, "Initialized trace list packing state\n");

  return packer;
} /* End of mstl3_pack_init() */

/** ************************************************************************
 * @brief Generate next miniSEED record from trace list packing state
 *
 * Pack the next miniSEED record from the packing state context initialized with
 * mstl3_pack_init().  The record pointer and length are returned via the @p
 * record and @p reclen parameters.  The record memory is owned by the packing
 * state and will be reused or freed on subsequent calls.
 *
 * This function should be called repeatedly until it returns 0 (no more
 * records) or -1 (error).
 *
 * The datasamples array, numsamples and starttime fields of each trace segment
 * will be adjusted as data are packed unless the ::MSF_MAINTAINMSTL flag is
 * specified in @p flags of mstl3_pack_init().
 *
 * @param[in] packer ::MS3TraceListPacker context
 * @param[in] flags Bit flags to control packing:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 * @endparblock
 * @param[out] record Pointer to record buffer (owned by pack state)
 * @param[out] reclen Length of record in bytes
 *
 * @returns 1 when a record is available, 0 when finished, and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_pack_init()
 * @see mstl3_pack_free()
 ***************************************************************************/
int
mstl3_pack_next (MS3TraceListPacker *packer, uint32_t flags, char **record, int32_t *reclen)
{
  int result;
  int samplesize;
  size_t extralength;
  MS3TraceID *id = NULL;
  MS3TraceSeg *seg = NULL;

  if (!packer || !record || !reclen)
  {
    ms_log (2, "%s(): Required input not defined\n", __func__);
    return -1;
  }

  /* If packing is finished, return immediately */
  if (packer->finished)
    return 0;

  /* If we have an active segment packing state, try to get another record from it */
  if (packer->seg_packing_state)
  {
    /* Set flags from caller */
    if (flags & MSF_FLUSHDATA)
      packer->seg_packing_state->flags |= MSF_FLUSHDATA;

    result = msr3_pack_next (packer->seg_packing_state, record, reclen);

    if (result == 1)
    {
      /* Got a record */
      packer->totalpackedrecords++;
      return 1;
    }
    else if (result == 0)
    {
      /* Segment packing finished, clean up and update segment */
      int64_t seg_total_packed;

      /* Free segment packing state and get packed sample count */
      msr3_pack_free (&packer->seg_packing_state, &seg_total_packed);
      packer->totalpackedsamples += seg_total_packed;

      if (packer->verbose > 1 && packer->current_id)
        ms_log (0, "%s: Packed %" PRId64 " samples from segment\n",
                packer->current_id->sid, seg_total_packed);

      /* If MSF_MAINTAINMSTL not set, update segment data buffer: remove packed samples */
      if ((packer->flags & MSF_MAINTAINMSTL) == 0 && seg_total_packed > 0 && packer->current_seg)
      {
        /* Calculate new start time */
        if (seg_total_packed == packer->current_seg->numsamples)
          packer->current_seg->starttime = packer->current_seg->endtime;
        else
          packer->current_seg->starttime = ms_sampletime (
              packer->current_seg->starttime, seg_total_packed, packer->current_seg->samprate);

        samplesize = ms_samplesize (packer->current_seg->sampletype);
        if (!samplesize)
        {
          ms_log (2, "Unknown sample size for sample type: %c\n", packer->current_seg->sampletype);
          return -1;
        }

        packer->current_seg->samplecnt -= seg_total_packed;
        packer->current_seg->numsamples -= seg_total_packed;

        /* Resize data buffer if samples remain */
        if (packer->current_seg->numsamples > 0)
        {
          size_t bufsize = packer->current_seg->numsamples * samplesize;

          /* Move remaining data to beginning of buffer */
          memmove (packer->current_seg->datasamples,
                   (uint8_t *)packer->current_seg->datasamples + (seg_total_packed * samplesize),
                   bufsize);

          /* Reallocate buffer for reduced size if not pre-allocating */
          if (libmseed_prealloc_block_size == 0)
          {
            packer->current_seg->datasamples =
                libmseed_memory.realloc (packer->current_seg->datasamples, bufsize);

            if (!packer->current_seg->datasamples)
            {
              ms_log (2, "Cannot (re)allocate datasamples buffer\n");
              return -1;
            }

            packer->current_seg->datasize = (uint64_t)bufsize;
          }
        }
        else
        {
          /* Remove empty segment
           * NOTE: If this is the last segment, lm_remove_segment() may also remove
           * and free the trace ID */
          lm_remove_segment (packer->mstl, packer->current_id, packer->current_seg, 1);
        }

        /* Reset position to scan from beginning on next call to catch newly added IDs */
        packer->current_id = NULL;
        packer->current_seg = NULL;
      }
      else if (packer->flags & MSF_MAINTAINMSTL)
      {
        /* When maintaining the trace list, advance to next segment instead of resetting
         * to avoid repacking the same segments */
        if (packer->current_seg && packer->current_seg->next)
        {
          /* Move to next segment in current trace ID */
          packer->current_seg = packer->current_seg->next;
        }
        else if (packer->current_id && packer->current_id->next[0])
        {
          /* Move to first segment of next trace ID */
          packer->current_id = packer->current_id->next[0];
          packer->current_seg = packer->current_id->first;
        }
        else
        {
          /* Reached the end of the trace list, mark as finished */
          packer->finished = 1;
          return 0;
        }
      }

      /* Fall through to scan for next segment to pack */
    }
    else
    {
      /* Error from segment packing */
      if (packer->current_id)
        ms_log (2, "%s: Error packing segment\n", packer->current_id->sid);
      else
        ms_log (2, "Error packing segment\n");
      return -1;
    }
  }

  /* Scan trace list to find a segment that can produce a record */
  /* When MSF_MAINTAINMSTL is set and we have a current position, start from there;
   * otherwise start from the beginning */
  if ((packer->flags & MSF_MAINTAINMSTL) && packer->current_id)
  {
    id = packer->current_id;
  }
  else
  {
    id = packer->mstl->traces.next[0];
  }

  for (; id; id = id->next[0])
  {
    /* Determine starting segment for this trace ID */
    if ((packer->flags & MSF_MAINTAINMSTL) && id == packer->current_id && packer->current_seg)
    {
      seg = packer->current_seg;
    }
    else
    {
      seg = id->first;
    }

    for (; seg; seg = seg->next)
    {
      /* Skip empty segments */
      if (seg->numsamples == 0)
        continue;

      /* Found a segment with data - try to pack it */
      packer->current_id = id;
      packer->current_seg = seg;

      uint32_t segment_flags = packer->flags;

      /* If flush_idle_nanoseconds is set and the segment has a prvtptr with an
       * update time set during parsing with the ::MSF_PPUPDATETIME flag, check if
       * the segment has been updated within the specified number of seconds.
       * If it has not, force the flushing of the segment by setting the
       * ::MSF_FLUSHDATA flag. */
      if (packer->flush_idle_nanoseconds > 0 && seg->prvtptr)
      {
        nstime_t *update_time = (nstime_t *)seg->prvtptr;
        nstime_t update_latency = lmp_systemtime () - *update_time;

        if (update_latency > packer->flush_idle_nanoseconds)
        {
          segment_flags |= MSF_FLUSHDATA;

          if (packer->verbose >= 2)
            ms_log (0, "%s: Flushing idle segment (idle for %.1f seconds)\n",
                    id->sid, (double)update_latency / NSTMODULUS);
        }
      }

      /* Initialize MS3Record template from segment */
      msr3_init (&packer->msr_template);

      packer->msr_template.reclen = packer->reclen;
      packer->msr_template.encoding = packer->encoding;
      memcpy (packer->msr_template.sid, id->sid, sizeof (packer->msr_template.sid));
      packer->msr_template.pubversion = id->pubversion;

      if (packer->extra)
      {
        packer->msr_template.extra = packer->extra;
        extralength = strlen (packer->extra);

        if (extralength > UINT16_MAX)
        {
          ms_log (2, "Extra headers are too long: %" PRIsize_t "\n", extralength);
          return -1;
        }

        packer->msr_template.extralength = (uint16_t)extralength;
      }

      /* Set data from segment */
      packer->msr_template.starttime = seg->starttime;
      packer->msr_template.samprate = seg->samprate;
      packer->msr_template.samplecnt = seg->samplecnt;
      packer->msr_template.datasamples = seg->datasamples;
      packer->msr_template.numsamples = seg->numsamples;
      packer->msr_template.sampletype = seg->sampletype;

      /* Set encoding for data types with only one encoding */
      switch (seg->sampletype)
      {
      case 't':
        packer->msr_template.encoding = DE_TEXT;
        break;
      case 'f':
        packer->msr_template.encoding = DE_FLOAT32;
        break;
      case 'd':
        packer->msr_template.encoding = DE_FLOAT64;
        break;
      default:
        packer->msr_template.encoding = packer->encoding;
      }

      /* Create segment packing state */
      packer->seg_packing_state = msr3_pack_init (&packer->msr_template, segment_flags, packer->verbose);

      if (!packer->seg_packing_state)
      {
        ms_log (2, "%s: Cannot initialize segment packing state\n", id->sid);
        return -1;
      }

      packer->segpackedsamples = 0;

      /* Set flags from caller */
      if (flags & MSF_FLUSHDATA)
        packer->seg_packing_state->flags |= MSF_FLUSHDATA;

      /* Try to get next record from segment packing state */
      result = msr3_pack_next (packer->seg_packing_state, record, reclen);

      if (result == 1)
      {
        /* Got a record */
        packer->totalpackedrecords++;
        return 1;
      }
      else if (result < 0)
      {
        /* Error from segment packing */
        ms_log (2, "%s: Error packing segment\n", id->sid);
        return -1;
      }

      /* result == 0: Segment couldn't produce a record (not enough data without flush)
       * Free the packing state and continue scanning for another segment */
      msr3_pack_free (&packer->seg_packing_state, NULL);
      packer->current_id = NULL;
      packer->current_seg = NULL;
    }
  }

  /* No segments can produce records at this time.
   * This means either:
   * 1. Without MSF_FLUSHDATA: all segments have data but not enough for complete records
   * 2. With MSF_FLUSHDATA: all segments are empty
   * The caller can add more data and call pack_next() again. */
  return 0;
} /* End of mstl3_pack_next() */

/** ************************************************************************
 * @brief Free trace list packing state and resources
 *
 * Free all memory associated with the ::MS3TraceListPacker and set the
 * pointer to NULL.
 *
 * @param[in] packer Pointer to ::MS3TraceListPacker pointer
 * @param[out] packedsamples Total number of samples packed, returned to caller
 *
 * @see mstl3_pack_init()
 * @see mstl3_pack_next()
 ***************************************************************************/
void
mstl3_pack_free (MS3TraceListPacker **packer, int64_t *packedsamples)
{
  if (!packer || !*packer)
    return;

  if (packedsamples)
    *packedsamples = (*packer)->totalpackedsamples;

  /* Free segment packer if still active */
  if ((*packer)->seg_packing_state)
  {
    int64_t seg_samples;
    msr3_pack_free (&(*packer)->seg_packing_state, &seg_samples);
  }

  libmseed_memory.free (*packer);
  *packer = NULL;
} /* End of mstl3_pack_free() */

/** ************************************************************************
 * @brief Pack a ::MS3TraceSeg data into miniSEED records
 *
 * The datasamples array, numsamples and starttime fields of each
 * trace segment will be adjusted as data are packed unless the
 * ::MSF_MAINTAINMSTL flag is specified in @p flags. If
 * ::MSF_MAINTAINMSTL is specified a caller would also normally set
 * the ::MSF_FLUSHDATA flag to pack all data in the trace list.
 *
 * As each record is filled and finished they are passed to @p
 * record_handler() which should expect 1) a @c char* to the record,
 * 2) the length of the record and 3) a pointer supplied by the
 * original caller containing optional private data (@p handlerdata).
 * It is the responsibility of @p record_handler() to process the
 * record, the memory will be re-used or freed when @p
 * record_handler() returns.
 *
 * The requested @p encoding value is currently only used for integer
 * data samples. The encoding is set automatially for text and
 * floating point data samples as there is only a single encoding for
 * them.  A value of @c -1 can be used to use the default encoding.
 *
 * If @p extra is not NULL it is expected to contain extraheaders, a
 * string containing (compact) JSON, that will be added to each output
 * record.
 *
 * If all of the data from the segment is packed into miniSEED records
 * and the ::MSF_MAINTAINMSTL is _not_ set the segment will be removed
 * from the trace list.  All memmory referenced by the segment, including
 * the `prvtptr`s, will also be freed.
 *
 * @param[in] mstl ::MS3TraceList for the relevent ::MS3TraceID (unused for now)
 * @param[in] id ::MS3TraceID for the relevant ::MS3TraceSeg
 * @param[in] seg ::MS3TraceSeg containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the @p record_handler()
 * @param[in] reclen Maximum record length to create
 * @param[in] encoding Encoding for data samples, see msr3_pack()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Bit flags to control packing:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - @c ::MSF_MAINTAINMSTL : Do not remove packed data from the buffer
 *  - @c ::MSF_PACKVER2 : Pack miniSEED version 2 instead of default 3
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 * @param[in] extra If not NULL, add this buffer of extra headers to all records
 *
 * @returns the number of records created on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_pack()
 * @see msr3_pack()
 ***************************************************************************/
int64_t
mstl3_pack_segment (MS3TraceList *mstl, MS3TraceID *id, MS3TraceSeg *seg,
                    void (*record_handler) (char *, int, void *), void *handlerdata, int reclen,
                    int8_t encoding, int64_t *packedsamples, uint32_t flags, int8_t verbose,
                    char *extra)
{
  (void)mstl;
  MS3Record msr = MS3Record_INITIALIZER;

  int64_t totalpackedrecords = 0;
  int64_t totalpackedsamples = 0;
  int segpackedrecords = 0;
  int64_t segpackedsamples = 0;
  int samplesize;
  size_t extralength;

  if (!id || !seg)
  {
    ms_log (2, "%s(): Required input not defined: 'id' or 'seg'\n", __func__);
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  if (packedsamples)
    *packedsamples = 0;

  if (seg->numsamples == 0)
  {
    return 0;
  }

  msr.reclen = reclen;
  msr.encoding = encoding;
  memcpy (msr.sid, id->sid, sizeof (msr.sid));
  msr.pubversion = id->pubversion;

  if (extra)
  {
    msr.extra = extra;
    extralength = strlen (extra);

    if (extralength > UINT16_MAX)
    {
      ms_log (2, "Extra headers are too long: %" PRIsize_t "\n", extralength);
      return -1;
    }

    msr.extralength = (uint16_t)extralength;
  }

  /* Pack segment data */
  msr.starttime = seg->starttime;
  msr.samprate = seg->samprate;
  msr.samplecnt = seg->samplecnt;
  msr.datasamples = seg->datasamples;
  msr.numsamples = seg->numsamples;
  msr.sampletype = seg->sampletype;

  /* Set encoding for data types with only one encoding, otherwise requested */
  switch (seg->sampletype)
  {
  case 't':
    msr.encoding = DE_TEXT;
    break;
  case 'f':
    msr.encoding = DE_FLOAT32;
    break;
  case 'd':
    msr.encoding = DE_FLOAT64;
    break;
  default:
    msr.encoding = encoding;
  }

  segpackedsamples = 0;
  segpackedrecords =
      msr3_pack (&msr, record_handler, handlerdata, &segpackedsamples, flags, verbose);

  if (verbose > 1)
  {
    ms_log (0, "Packed %d records for %s segment\n", segpackedrecords, msr.sid);
  }

  /* If MSF_MAINTAINMSTL not set, modify or remove segment accordingly */
  if ((flags & MSF_MAINTAINMSTL) == 0 && segpackedsamples > 0)
  {
    /* Calculate new start time, shortcut when all samples have been packed */
    if (segpackedsamples == seg->numsamples)
      seg->starttime = seg->endtime;
    else
      seg->starttime = ms_sampletime (seg->starttime, segpackedsamples, seg->samprate);

    if (!(samplesize = ms_samplesize (seg->sampletype)))
    {
      ms_log (2, "Unknown sample size for sample type: %c\n", seg->sampletype);
      return -1;
    }

    seg->samplecnt -= segpackedsamples;
    seg->numsamples -= segpackedsamples;

    /* Resize data buffer if samples remain */
    if (seg->numsamples > 0)
    {
      size_t bufsize = seg->numsamples * samplesize;

      memmove (seg->datasamples, (uint8_t *)seg->datasamples + (segpackedsamples * samplesize),
               bufsize);

      /* Reallocate buffer for reduced size needed, only if not pre-allocating */
      if (libmseed_prealloc_block_size == 0)
      {
        seg->datasamples = libmseed_memory.realloc (seg->datasamples, bufsize);

        if (seg->datasamples == NULL)
        {
          ms_log (2, "Cannot (re)allocate datasamples buffer\n");
          return -1;
        }

        seg->datasize = (uint64_t)bufsize;
      }
    }
  }

  totalpackedrecords += segpackedrecords;
  totalpackedsamples += segpackedsamples;

  if (packedsamples)
    *packedsamples = totalpackedsamples;

  return totalpackedrecords;
} /* End of mstl3_pack_segment() */

/** ************************************************************************
 * @deprecated This functionality will be removed in a future version.
 *
 * This deprecated function is provided for backwards compatibility. It will
 * call mstl3_pack_segment() as long as the MSF_MAINTAINMSTL flag is set.
 ***************************************************************************/
int64_t
mstraceseg3_pack (MS3TraceID *id, MS3TraceSeg *seg, void (*record_handler) (char *, int, void *),
                  void *handlerdata, int reclen, int8_t encoding, int64_t *packedsamples,
                  uint32_t flags, int8_t verbose, char *extra)
{
  if (!(flags & MSF_MAINTAINMSTL))
  {
    ms_log (2, "%s() is deprecated, use mstl3_pack_segment() instead\n", __func__);
    ms_log (2, "%s() cannot operate without the MSF_MAINTAINMSTL flag\n", __func__);
    return -1;
  }

  return mstl3_pack_segment (NULL, id, seg, record_handler, handlerdata, reclen, encoding,
                             packedsamples, flags, verbose, extra);
} /* End of mstraceseg3_pack() */

/** ************************************************************************
 * @brief Print trace list summary information for a ::MS3TraceList
 *
 * By default only print the source ID, starttime and endtime for each
 * trace.  If @p details is greater than 0 include the sample rate,
 * number of samples and a total trace count.  If @p gaps is greater than
 * 0 and the previous trace matches (SID & samprate) include the
 * gap between the endtime of the last trace and the starttime of the
 * current trace.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] timeformat Time string format, one of @ref ms_timeformat_t
 * @param[in] details Flag to control inclusion of more details
 * @param[in] gaps Flag to control inclusion of gap/overlap between segments
 * @param[in] versions Flag to control inclusion of publication version on SourceIDs
 ***************************************************************************/
void
mstl3_printtracelist (const MS3TraceList *mstl, ms_timeformat_t timeformat, int8_t details,
                      int8_t gaps, int8_t versions)
{
  const MS3TraceID *id = NULL;
  const MS3TraceSeg *seg = NULL;
  char stime[40];
  char etime[40];
  char gapstr[40];
  int8_t nogap;
  double gap;
  double delta;
  int tracecnt = 0;
  int segcnt = 0;

  char versioned_sid[LM_SIDLEN + 10] = {0};
  const char *display_sid = NULL;

  if (!mstl)
  {
    return;
  }

  /* Print out the appropriate header */
  if (details > 0 && gaps > 0)
    ms_log (0, "       SourceID                      Start sample                End sample        "
               "   Gap  Hz  Samples\n");
  else if (details <= 0 && gaps > 0)
    ms_log (0, "       SourceID                      Start sample                End sample        "
               "   Gap\n");
  else if (details > 0 && gaps <= 0)
    ms_log (0, "       SourceID                      Start sample                End sample        "
               "   Hz  Samples\n");
  else
    ms_log (0, "       SourceID                      Start sample                End sample\n");

  /* Loop through trace list */
  id = mstl->traces.next[0];
  while (id)
  {
    /* Generated versioned SID if requested */
    if (versions)
    {
      snprintf (versioned_sid, sizeof (versioned_sid), "%s#%u", id->sid, id->pubversion);
      display_sid = versioned_sid;
    }
    else
    {
      display_sid = id->sid;
    }

    /* Loop through segment list */
    seg = id->first;
    while (seg)
    {
      /* Create formatted time strings */
      if (ms_nstime2timestr_n (seg->starttime, stime, sizeof (stime), timeformat, NANO_MICRO) ==
          NULL)
        return;

      if (ms_nstime2timestr_n (seg->endtime, etime, sizeof (etime), timeformat, NANO_MICRO) == NULL)
        return;

      /* Print segment info at varying levels */
      if (gaps > 0)
      {
        gap = 0.0;
        nogap = 0;

        if (seg->prev)
          gap = (double)(seg->starttime - seg->prev->endtime) / NSTMODULUS;
        else
          nogap = 1;

        /* Check that any overlap is not larger than the trace coverage */
        if (gap < 0.0)
        {
          delta = (seg->samprate) ? (1.0 / seg->samprate) : 0.0;

          if ((gap * -1.0) > (((double)(seg->endtime - seg->starttime) / NSTMODULUS) + delta))
            gap = -(((double)(seg->endtime - seg->starttime) / NSTMODULUS) + delta);
        }

        /* Fix up gap display */
        if (nogap)
          snprintf (gapstr, sizeof (gapstr), " == ");
        else if (gap >= 86400.0 || gap <= -86400.0)
          snprintf (gapstr, sizeof (gapstr), "%-3.1fd", (gap / 86400));
        else if (gap >= 3600.0 || gap <= -3600.0)
          snprintf (gapstr, sizeof (gapstr), "%-3.1fh", (gap / 3600));
        else if (gap == 0.0)
          snprintf (gapstr, sizeof (gapstr), "-0  ");
        else
          snprintf (gapstr, sizeof (gapstr), "%-4.4g", gap);

        if (details <= 0)
          ms_log (0, "%-27s %-28s %-28s %-4s\n", display_sid, stime, etime, gapstr);
        else
          ms_log (0, "%-27s %-28s %-28s %-s %-3.3g %-" PRId64 "\n", display_sid, stime, etime,
                  gapstr, seg->samprate, seg->samplecnt);
      }
      else if (details > 0 && gaps <= 0)
        ms_log (0, "%-27s %-28s %-28s %-3.3g %-" PRId64 "\n", display_sid, stime, etime,
                seg->samprate, seg->samplecnt);
      else
        ms_log (0, "%-27s %-28s %-28s\n", display_sid, stime, etime);

      segcnt++;
      seg = seg->next;
    }

    tracecnt++;
    id = id->next[0];
  }

  if (details > 0)
    ms_log (0, "Total: %d trace(s) with %d segment(s)\n", tracecnt, segcnt);

  return;
} /* End of mstl3_printtracelist() */

/** ************************************************************************
 * @brief Print SYNC trace list summary information for a ::MS3TraceList
 *
 * The SYNC header line will be created using the supplied dccid, if
 * the pointer is NULL the string "DCC" will be used instead.
 *
 * If the @p subsecond flag is true the segment start and end times will
 * include subsecond precision, otherwise they will be truncated to
 * integer seconds.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] dccid The DCC identifier to include in the output
 * @param[in] subseconds Inclusion of subseconds, one of @ref ms_subseconds_t
 ***************************************************************************/
void
mstl3_printsynclist (const MS3TraceList *mstl, const char *dccid, ms_subseconds_t subseconds)
{
  const MS3TraceID *id = NULL;
  const MS3TraceSeg *seg = NULL;
  char starttime[40];
  char endtime[40];
  char yearday[32];
  char net[11] = {0};
  char sta[11] = {0};
  char loc[11] = {0};
  char chan[11] = {0};
  time_t now;
  struct tm *nt;

  if (!mstl)
  {
    return;
  }

  /* Generate current time stamp */
  now = time (NULL);
  nt = localtime (&now);
  nt->tm_year += 1900;
  nt->tm_yday += 1;
  snprintf (yearday, sizeof (yearday), "%04d,%03d", nt->tm_year, nt->tm_yday);

  /* Print SYNC header line */
  ms_log (0, "%s|%s\n", (dccid) ? dccid : "DCC", yearday);

  /* Loop through trace list */
  id = mstl->traces.next[0];
  while (id)
  {
    ms_sid2nslc_n (id->sid, net, sizeof (net), sta, sizeof (sta), loc, sizeof (loc), chan,
                   sizeof (chan));

    /* Loop through segment list */
    seg = id->first;
    while (seg)
    {
      ms_nstime2timestr_n (seg->starttime, starttime, sizeof (starttime), SEEDORDINAL, subseconds);
      ms_nstime2timestr_n (seg->endtime, endtime, sizeof (endtime), SEEDORDINAL, subseconds);

      /* Print SYNC line */
      ms_log (0, "%s|%s|%s|%s|%s|%s||%.10g|%" PRId64 "|||||||%s\n", net, sta, loc, chan, starttime,
              endtime, seg->samprate, seg->samplecnt, yearday);

      seg = seg->next;
    }

    id = id->next[0];
  }

  return;
} /* End of mstl3_printsynclist() */

/** ************************************************************************
 * @brief Print gap/overlap list summary information for a ::MS3TraceList.
 *
 * Overlaps are printed as negative gaps.
 *
 * If @p mingap and @p maxgap are not NULL their values will be
 * enforced and only gaps/overlaps matching their implied criteria
 * will be printed.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] timeformat Time string format, one of @ref ms_timeformat_t
 * @param[in] mingap Minimum gap to print in seconds (pointer to value)
 * @param[in] maxgap Maximum gap to print in seconds (pointer to value)
 ***************************************************************************/
void
mstl3_printgaplist (const MS3TraceList *mstl, ms_timeformat_t timeformat, double *mingap,
                    double *maxgap)
{
  const MS3TraceID *id = NULL;
  const MS3TraceSeg *seg = NULL;

  char time1[40], time2[40];
  char gapstr[40];
  double gap;
  double delta;
  double nsamples;
  int8_t printflag;
  int gapcnt = 0;

  if (!mstl)
  {
    return;
  }

  ms_log (0, "       SourceID                      Last Sample                 Next Sample         "
             " Gap  Samples\n");

  id = mstl->traces.next[0];
  while (id)
  {
    seg = id->first;
    while (seg->next)
    {
      /* Skip segments with no time coverage, usually from SOH records */
      if (!SEGMENT_HAS_TIME_COVERAGE (seg))
      {
        seg = seg->next;
        continue;
      }

      gap = (double)(seg->next->starttime - seg->endtime) / NSTMODULUS;

      /* Check that any overlap is not larger than the trace coverage */
      if (gap < 0.0)
      {
        delta = (seg->next->samprate) ? (1.0 / seg->next->samprate) : 0.0;

        if ((gap * -1.0) >
            (((double)(seg->next->endtime - seg->next->starttime) / NSTMODULUS) + delta))
          gap = -(((double)(seg->next->endtime - seg->next->starttime) / NSTMODULUS) + delta);
      }

      printflag = 1;

      /* Check gap/overlap criteria */
      if (mingap)
        if (gap < *mingap)
          printflag = 0;

      if (maxgap)
        if (gap > *maxgap)
          printflag = 0;

      if (printflag)
      {
        nsamples = fabs (gap) * seg->samprate;

        if (gap > 0.0)
          nsamples -= 1.0;
        else
          nsamples += 1.0;

        /* Fix up gap display */
        if (gap >= 86400.0 || gap <= -86400.0)
          snprintf (gapstr, sizeof (gapstr), "%-3.1fd", (gap / 86400));
        else if (gap >= 3600.0 || gap <= -3600.0)
          snprintf (gapstr, sizeof (gapstr), "%-3.1fh", (gap / 3600));
        else if (gap == 0.0)
          snprintf (gapstr, sizeof (gapstr), "-0  ");
        else
          snprintf (gapstr, sizeof (gapstr), "%-4.4g", gap);

        /* Create formatted time strings */
        if (ms_nstime2timestr_n (seg->endtime, time1, sizeof (time1), timeformat, NANO_MICRO) ==
            NULL)
          ms_log (2, "Cannot convert trace start time for %s\n", id->sid);

        if (ms_nstime2timestr_n (seg->next->starttime, time2, sizeof (time2), timeformat,
                                 NANO_MICRO) == NULL)
          ms_log (2, "Cannot convert trace end time for %s\n", id->sid);

        ms_log (0, "%-27s %-28s %-28s %-4s %-.8g\n", id->sid, time1, time2, gapstr, nsamples);

        gapcnt++;
      }

      seg = seg->next;
    }

    id = id->next[0];
  }

  ms_log (0, "Total: %d gap(s)\n", gapcnt);

  return;
} /* End of mstl3_printgaplist() */

/***************************************************************************
 * Free all memory associated with an MS3TraceSeg structure.
 *
 * This function frees the segment's data samples, record list, and the
 * segment structure itself. Private pointers are only freed if requested.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static void
lm_free_segment_memory (MS3TraceSeg *seg, int8_t freeprvtptr)
{
  MS3RecordPtr *recordptr;
  MS3RecordPtr *nextrecordptr;

  if (!seg)
    return;

  /* Free private pointer data if requested */
  if (freeprvtptr)
    libmseed_memory.free (seg->prvtptr);

  /* Free data samples */
  libmseed_memory.free (seg->datasamples);

  /* Free associated record list and related private pointers */
  if (seg->recordlist)
  {
    recordptr = seg->recordlist->first;
    while (recordptr)
    {
      nextrecordptr = recordptr->next;

      msr3_free (&recordptr->msr);

      /* Free private pointer data if requested */
      if (freeprvtptr)
        libmseed_memory.free (recordptr->prvtptr);

      libmseed_memory.free (recordptr);

      recordptr = nextrecordptr;
    }

    libmseed_memory.free (seg->recordlist);
  }

  libmseed_memory.free (seg);
} /* End of lm_seg3_free_memory() */

/** ************************************************************************
 * @brief Remove the target MS3TraceSeg structure from MS3TraceID.
 *
 * If the segment is the only segment in the trace ID, the trace ID will be
 * removed from the trace list as well.
 *
 * @param[in] mstl ::MS3TraceList containing the segment to remove
 * @param[in] id ::MS3TraceID containing the segment to remove
 * @param[in] seg ::MS3TraceSeg segment to remove
 * @param[in] freeprvtptr If true, free the private pointer data
 *
 * @returns 0 on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see mstl3_free()
 ***************************************************************************/
static int
lm_remove_segment (MS3TraceList *mstl, MS3TraceID *id, MS3TraceSeg *seg, int8_t freeprvtptr)
{
  MS3TraceID *searchid = NULL;
  MS3TraceID *previd[MSTRACEID_SKIPLIST_HEIGHT] = {NULL};
  int level;

  if (!mstl || !id || !seg)
  {
    ms_log (2, "%s(): Required input not defined: 'mstl', 'id' or 'seg'\n", __func__);
    return -1;
  }
  /* Update previous segment's next pointer or first pointer */
  if (seg->prev)
    seg->prev->next = seg->next;
  else
    id->first = seg->next;

  /* Update next segment's next pointer or last pointer */
  if (seg->next)
    seg->next->prev = seg->prev;
  else
    id->last = seg->prev;

  /* Decrement segment count */
  id->numsegments -= 1;

  /* Free all memory associated with the segment */
  lm_free_segment_memory (seg, freeprvtptr);

  /* If this was the last segment, remove the TraceID from the trace list */
  if (id->numsegments == 0)
  {
    /* Find previous nodes at each level for this TraceID */
    level = MSTRACEID_SKIPLIST_HEIGHT - 1;
    searchid = &(mstl->traces);

    while (level >= 0)
    {
      if (searchid == NULL || searchid->next[level] == NULL)
      {
        /* No more nodes at this level, record predecessor and drop level */
        previd[level] = (searchid != NULL) ? searchid : &(mstl->traces);
        level -= 1;
        searchid = &(mstl->traces); /* Reset search position for next level */
      }
      else if (searchid->next[level] == id)
      {
        /* Found the TraceID at this level, record predecessor and drop level */
        previd[level] = searchid;
        level -= 1;
        searchid = &(mstl->traces); /* Reset search position for next level */
      }
      else
      {
        /* Continue searching at this level */
        searchid = searchid->next[level];
      }
    }

    /* Remove TraceID from skip list by updating previous node pointers */
    for (level = id->height - 1; level >= 0; level--)
    {
      if (previd[level] && previd[level]->next[level] == id)
      {
        previd[level]->next[level] = id->next[level];
      }
    }

    /* Free private pointer data if requested */
    if (freeprvtptr && id->prvtptr)
      libmseed_memory.free (id->prvtptr);

    /* Free the TraceID */
    libmseed_memory.free (id);

    /* Decrement trace count */
    mstl->numtraceids--;
  }

  return 0;
}

/* Pseudo random number generator, as a linear congruential generator (LCG):
 * https://en.wikipedia.org/wiki/Linear_congruential_generator
 *
 * Yields a sequence of pseudo-randomized numbers distributed
 * between 0 and UINT32_MAX.  Roughly half-chance of being above
 * or below (UINT32_MAX / 2), good enough for coin-flipping.
 *
 * Uses 64-bit state but returns only the higher-order bits
 * for statistically better values.
 */
static uint32_t
lm_lcg_r (uint64_t *state)
{
  *state = 6364136223846793005ULL * *state + 1;
  return *state >> 32;
}

/* Return random height from 1 up to maximum.
 *
 * Coin-flipping method using a pseudo random number generator.
 */
static uint8_t
lm_random_height (uint8_t maximum, uint64_t *state)
{
  uint8_t height = 1;

  while (height < maximum && lm_lcg_r (state) < (UINT32_MAX / 2))
  {
    height++;
  }

  return height;
}
