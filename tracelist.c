/***************************************************************************
 * tracelist.c:
 *
 * Routines to handle TraceList and related structures.
 *
 * Written by Chad Trabant, IRIS Data Management Center
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"

MS3TraceSeg *mstl3_msr2seg (MS3Record *msr, nstime_t endtime);
MS3TraceSeg *mstl3_addmsrtoseg (MS3TraceSeg *seg, MS3Record *msr, nstime_t endtime, int8_t whence);
MS3TraceSeg *mstl3_addsegtoseg (MS3TraceSeg *seg1, MS3TraceSeg *seg2);

/**********************************************************************/ /**
 * @brief Initialize a ::MS3TraceList container
 *
 * A new ::MS3TraceList is allocated if needed. If the supplied
 * ::MS3TraceList is not NULL it will be re-initialized and any
 * associated memory will be freed including data at \a prvtptr pointers.
 *
 * @param[in] mstl ::MS3TraceList to reinitialize or NULL
 *
 * @returns a pointer to a MS3TraceList struct on success or NULL on error.
 ***************************************************************************/
MS3TraceList *
mstl3_init (MS3TraceList *mstl)
{
  if (mstl)
  {
    mstl3_free (&mstl, 1);
  }

  mstl = (MS3TraceList *)malloc (sizeof (MS3TraceList));

  if (mstl == NULL)
  {
    ms_log (2, "%s(): Cannot allocate memory\n", __func__);
    return NULL;
  }

  memset (mstl, 0, sizeof (MS3TraceList));

  return mstl;
} /* End of mstl3_init() */

/**********************************************************************/ /**
 * @brief Free all memory associated with a ::MS3TraceList
 *
 * The pointer to the target ::MS3TraceList will be set to NULL.
 *
 * @param[in] ppmstl Pointer-to-pointer to the target ::MS3TraceList to free
 * @param[in] freeprvtptr If true, also free any data at the \a prvtptr
 * members of ::MS3TraceID.prvtptr and ::MS3TraceSeg.prvtptr
 ***************************************************************************/
void
mstl3_free (MS3TraceList **ppmstl, int8_t freeprvtptr)
{
  MS3TraceID *id = 0;
  MS3TraceID *nextid = 0;
  MS3TraceSeg *seg = 0;
  MS3TraceSeg *nextseg = 0;

  if (!ppmstl)
    return;

  if (*ppmstl)
  {
    /* Free any associated traces */
    id = (*ppmstl)->traces;
    while (id)
    {
      nextid = id->next;

      /* Free any associated trace segments */
      seg = id->first;
      while (seg)
      {
        nextseg = seg->next;

        /* Free private pointer data if present and requested*/
        if (freeprvtptr && seg->prvtptr)
          free (seg->prvtptr);

        /* Free data array if allocated */
        if (seg->datasamples)
          free (seg->datasamples);

        free (seg);
        seg = nextseg;
      }

      /* Free private pointer data if present and requested*/
      if (freeprvtptr && id->prvtptr)
        free (id->prvtptr);

      free (id);
      id = nextid;
    }

    free (*ppmstl);

    *ppmstl = NULL;
  }

  return;
} /* End of mstl3_free() */

/**********************************************************************/ /**
 * @brief Add data coverage from an ::MS3Record to a ::MS3TraceList
 *
 * Searching the list for the appropriate ::MS3TraceID and
 * ::MS3TraceSeg and either add data to existing entries or create new
 * ones as needed.
 *
 * As this routine adds data to a trace list it attempts to construct
 * continuous time series, merging segments when possible.  The \a
 * timetol and \a sampratetol values define the tolerances used when
 * merging time series.  If \a timetol is ‐1.0 the default time
 * tolerance of 1/2 the sample period will be used.  If sampratetol is
 * ‐1.0 the default tolerance of abs(1‐sr1/sr2) < 0.0001 is used.  If
 * sampratetol or timetol is ‐2.0 the respective tolerance check will
 * not be performed.  If time tolerance is not checked the data will
 * be merged with a segment that fits best.
 *
 * If the \a splitversion flag is true the publication versions will be
 * kept separate, i.e. they must be the same to be merged. Otherwise,
 * different versions of otherwise matching traces are merged.  If
 * more than one version contributes to a given source identifer's
 * segments, its publication version will be the set to the largest
 * contributing version.
 *
 * If the \a autoheal flag is true extra processing is invoked to
 * conjoin trace segments that fit together after the ::MS3Record
 * coverage is added.  For segments that are removed, any memory at
 * the prvtptr will be freed.
 *
 * The lists are always maintained in a sorted order.  An
 * ::MS3TraceList is always maintained with the ::MS3TraceID entries
 * in descending alphanumeric order. An ::MS3TraceID is always
 * maintained with ::MS3TraceSeg entries in data time time order.
 *
 * @param[in] mstl Destination ::MS3TraceList to add data to
 * @param[in] msr ::MS3Record containing the data to add to list
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] autoheal Flag to control automatic merging of segments
 * @param[in] timetol Time tolerance in seconds for merging time series
 * @param[in] sampratetol Sample rate tolerance in samples per second
 *
 * @returns a pointer to the ::MS3TraceSeg updated or NULL on error.
 ***************************************************************************/
MS3TraceSeg *
mstl3_addmsr (MS3TraceList *mstl, MS3Record *msr, int8_t splitversion,
              int8_t autoheal, double timetol, double sampratetol)
{
  MS3TraceID *id = 0;
  MS3TraceID *searchid = 0;
  MS3TraceID *ltid = 0;

  MS3TraceSeg *seg = 0;
  MS3TraceSeg *searchseg = 0;
  MS3TraceSeg *segbefore = 0;
  MS3TraceSeg *segafter = 0;
  MS3TraceSeg *followseg = 0;

  nstime_t endtime;
  nstime_t pregap;
  nstime_t postgap;
  nstime_t lastgap;
  nstime_t firstgap;
  nstime_t nsdelta;
  nstime_t nstimetol = 0;
  nstime_t nnstimetol = 0;

  char *s1, *s2;
  int8_t whence;
  int8_t lastratecheck;
  int8_t firstratecheck;
  int mag;
  int cmp;
  int ltmag;
  int ltcmp;

  if (!mstl || !msr)
    return 0;

  /* Calculate end time for MS3Record */
  if ((endtime = msr3_endtime (msr)) == NSTERROR)
  {
    ms_log (2, "%s(): Error calculating record end time\n", __func__);
    return 0;
  }

  /* Search for matching trace ID starting with last accessed ID and
     then looping through the trace ID list. */
  if (mstl->last)
  {
    s1 = mstl->last->sid;
    s2 = msr->sid;
    while (*s1 == *s2++)
    {
      if (*s1++ == '\0')
        break;
    }
    cmp = (*s1 - *--s2);

    if (splitversion && mstl->last->pubversion != msr->pubversion)
    {
      cmp = 1;
    }

    if (!cmp)
    {
      id = mstl->last;
    }
    else
    {
      /* Loop through trace ID list searching for a match, simultaneously
         track the source name which is closest but less than the MS3Record
         to allow for later insertion with sort order. */
      searchid = mstl->traces;
      ltcmp = 0;
      ltmag = 0;
      while (searchid)
      {
        /* Compare source names */
        s1 = searchid->sid;
        s2 = msr->sid;
        mag = 0;
        while (*s1 == *s2++)
        {
          mag++;
          if (*s1++ == '\0')
            break;
        }
        cmp = (*s1 - *--s2);

        if (splitversion && mstl->last->pubversion != msr->pubversion)
        {
          cmp = 1;
        }

        /* If source names did not match track closest "less than" value
           and continue searching. */
        if (cmp != 0)
        {
          if (cmp < 0)
          {
            if ((ltcmp == 0 || cmp >= ltcmp) && mag >= ltmag)
            {
              ltcmp = cmp;
              ltmag = mag;
              ltid = searchid;
            }
            else if (mag > ltmag)
            {
              ltcmp = cmp;
              ltmag = mag;
              ltid = searchid;
            }
          }

          searchid = searchid->next;
          continue;
        }

        /* If we made it this far we found a match */
        id = searchid;
        break;
      }
    }
  } /* Done searching for match in trace ID list */

  /* If no matching ID was found create new MS3TraceID and MS3TraceSeg entries */
  if (!id)
  {
    if (!(id = (MS3TraceID *)calloc (1, sizeof (MS3TraceID))))
    {
      ms_log (2, "%s(): Error allocating memory\n", __func__);
      return 0;
    }

    /* Populate MS3TraceID */
    memcpy (id->sid, msr->sid, sizeof(id->sid));
    id->pubversion = msr->pubversion;

    id->earliest = msr->starttime;
    id->latest = endtime;
    id->numsegments = 1;

    if (!(seg = mstl3_msr2seg (msr, endtime)))
    {
      return 0;
    }
    id->first = id->last = seg;

    /* Add new MS3TraceID to MS3TraceList */
    if (!mstl->traces || !ltid)
    {
      id->next = mstl->traces;
      mstl->traces = id;
    }
    else
    {
      id->next = ltid->next;
      ltid->next = id;
    }

    mstl->numtraces++;
  }
  /* Add data coverage to the matching MS3TraceID */
  else
  {
    /* Calculate high-precision sample period */
    nsdelta = (nstime_t) ((msr->samprate) ? (NSTMODULUS / msr->samprate) : 0.0);

    /* Calculate high-precision time tolerance */
    if (timetol == -1.0)
      nstimetol = (nstime_t) (0.5 * nsdelta); /* Default time tolerance is 1/2 sample period */
    else if (timetol >= 0.0)
      nstimetol = (nstime_t) (timetol * NSTMODULUS);

    nnstimetol = (nstimetol) ? -nstimetol : 0;

    /* last/firstgap are negative when the record overlaps the trace
     * segment and positive when there is a time gap. */

    /* Gap relative to the last segment */
    lastgap = msr->starttime - id->last->endtime - nsdelta;

    /* Gap relative to the first segment */
    firstgap = id->first->starttime - endtime - nsdelta;

    /* Sample rate tolerance checks for first and last segments */
    if (sampratetol == -1.0)
    {
      lastratecheck = MS_ISRATETOLERABLE (msr->samprate, id->last->samprate);
      firstratecheck = MS_ISRATETOLERABLE (msr->samprate, id->first->samprate);
    }
    else
    {
      lastratecheck = (ms_dabs (msr->samprate - id->last->samprate) > sampratetol) ? 0 : 1;
      firstratecheck = (ms_dabs (msr->samprate - id->first->samprate) > sampratetol) ? 0 : 1;
    }

    /* Search first for the simple scenarios in order of likelihood:
     * - Record fits at end of last segment
     * - Record fits after all coverage
     * - Record fits before all coverage
     * - Record fits at beginning of first segment
     *
     * If none of those scenarios are true search the complete segment list.
     */

    /* Record coverage fits at end of last segment */
    if (lastgap <= nstimetol && lastgap >= nnstimetol && lastratecheck)
    {
      if (!mstl3_addmsrtoseg (id->last, msr, endtime, 1))
        return 0;

      seg = id->last;

      if (endtime > id->latest)
        id->latest = endtime;
    }
    /* Record coverage is after all other coverage */
    else if ((msr->starttime - nsdelta - nstimetol) > id->latest)
    {
      if (!(seg = mstl3_msr2seg (msr, endtime)))
        return 0;

      /* Add to end of list */
      id->last->next = seg;
      seg->prev = id->last;
      id->last = seg;
      id->numsegments++;

      if (endtime > id->latest)
        id->latest = endtime;
    }
    /* Record coverage is before all other coverage */
    else if ((endtime + nsdelta + nstimetol) < id->earliest)
    {
      if (!(seg = mstl3_msr2seg (msr, endtime)))
        return 0;

      /* Add to beginning of list */
      id->first->prev = seg;
      seg->next = id->first;
      id->first = seg;
      id->numsegments++;

      if (msr->starttime < id->earliest)
        id->earliest = msr->starttime;
    }
    /* Record coverage fits at beginning of first segment */
    else if (firstgap <= nstimetol && firstgap >= nnstimetol && firstratecheck)
    {
      if (!mstl3_addmsrtoseg (id->first, msr, endtime, 2))
        return 0;

      seg = id->first;

      if (msr->starttime < id->earliest)
        id->earliest = msr->starttime;
    }
    /* Search complete segment list for matches */
    else
    {
      searchseg = id->first;
      segbefore = 0; /* Find segment that record fits before */
      segafter = 0; /* Find segment that record fits after */
      followseg = 0; /* Track segment that record follows in time order */
      while (searchseg)
      {
        if (msr->starttime > searchseg->starttime)
          followseg = searchseg;

        whence = 0;

        postgap = msr->starttime - searchseg->endtime - nsdelta;
        if (!segbefore && postgap <= nstimetol && postgap >= nnstimetol)
          whence = 1;

        pregap = searchseg->starttime - endtime - nsdelta;
        if (!segafter && pregap <= nstimetol && pregap >= nnstimetol)
          whence = 2;

        if (!whence)
        {
          searchseg = searchseg->next;
          continue;
        }

        if (sampratetol == -1.0)
        {
          if (!MS_ISRATETOLERABLE (msr->samprate, searchseg->samprate))
          {
            searchseg = searchseg->next;
            continue;
          }
        }
        else
        {
          if (ms_dabs (msr->samprate - searchseg->samprate) > sampratetol)
          {
            searchseg = searchseg->next;
            continue;
          }
        }

        if (whence == 1)
          segbefore = searchseg;
        else
          segafter = searchseg;

        /* Done searching if not autohealing */
        if (!autoheal)
          break;

        /* Done searching if both before and after segments are found */
        if (segbefore && segafter)
          break;

        searchseg = searchseg->next;
      } /* Done looping through segments */

      /* Add MS3Record coverage to end of segment before */
      if (segbefore)
      {
        if (!mstl3_addmsrtoseg (segbefore, msr, endtime, 1))
        {
          return 0;
        }

        /* Merge two segments that now fit if autohealing */
        if (autoheal && segafter && segbefore != segafter)
        {
          /* Add segafter coverage to segbefore */
          if (!mstl3_addsegtoseg (segbefore, segafter))
          {
            return 0;
          }

          /* Shift last segment pointer if it's going to be removed */
          if (segafter == id->last)
            id->last = id->last->prev;

          /* Remove segafter from list */
          if (segafter->prev)
            segafter->prev->next = segafter->next;
          if (segafter->next)
            segafter->next->prev = segafter->prev;

          /* Free data samples, private data and segment structure */
          if (segafter->datasamples)
            free (segafter->datasamples);

          if (segafter->prvtptr)
            free (segafter->prvtptr);

          free (segafter);
        }

        seg = segbefore;
      }
      /* Add MS3Record coverage to beginning of segment after */
      else if (segafter)
      {
        if (!mstl3_addmsrtoseg (segafter, msr, endtime, 2))
        {
          return 0;
        }

        seg = segafter;
      }
      /* Add MS3Record coverage to new segment */
      else
      {
        /* Create new segment */
        if (!(seg = mstl3_msr2seg (msr, endtime)))
        {
          return 0;
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
    if (msr->pubversion > id->pubversion)
      id->pubversion = msr->pubversion;

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
  while (seg->prev && (seg->starttime < seg->prev->starttime ||
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

  /* Set MS3TraceID as last accessed */
  mstl->last = id;

  return seg;
} /* End of mstl3_addmsr() */


/****************************************************************/ /**
 * @brief Parse miniSEED from a buffer and populate a ::MS3TraceList
 *
 * For a full description of \a timetol and \a sampratetol see
 * mstl3_addmsr().
 *
 * If the ::MSF_UNPACKDATA flag is set in \a flags, the data samples
 * will be unpacked.  In most cases the caller probably wants this
 * flag set, without it the trace list will merely be a list of
 * channels.
 *
 * @param[in] ppmstl Pointer-to-point to destination MS3TraceList
 * @param[in] buffer Source buffer to read miniSEED records from
 * @param[in] bufferlength Maximum length of \a buffer
 * @param[in] timetol Time tolerance in seconds for merging time series
 * @param[in] sampratetol Sample rate tolerance in samples per second
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] flags Flag to control pasing of miniSEED, see msr3_parse()
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns The number of records parsed on success, otherwise a
 * negative library error code.
 *********************************************************************/
int64_t
mstl3_readbuffer (MS3TraceList **ppmstl, char *buffer, uint64_t bufferlength,
                  double timetol, double sampratetol, int8_t splitversion,
                  uint32_t flags, int8_t verbose)
{
  MS3Record *msr = 0;
  uint64_t offset = 0;
  int parsevalue;
  int64_t reccount = 0;

  if (!ppmstl)
    return MS_GENERROR;

  /* Initialize MS3TraceList if needed */
  if (!*ppmstl)
  {
    *ppmstl = mstl3_init (*ppmstl);

    if (!*ppmstl)
      return MS_GENERROR;
  }

  while ((bufferlength - offset) > MINRECLEN)
  {
    parsevalue = msr3_parse (buffer + offset, bufferlength - offset, &msr, flags, verbose);

    if (parsevalue < 0)
      return parsevalue;

    if (parsevalue > 0)
      break;

    if (mstl3_addmsr (*ppmstl, msr, splitversion, 1, timetol, sampratetol) == 0)
      return MS_GENERROR;

    reccount += 1;
    offset += msr->reclen;
  }

  return reccount;
} /* End of mstl3_readbuffer() */

/***************************************************************************
 * Create an MS3TraceSeg structure from an MS3Record structure.
 *
 * Return a pointer to a MS3TraceSeg otherwise 0 on error.
 ***************************************************************************/
MS3TraceSeg *
mstl3_msr2seg (MS3Record *msr, nstime_t endtime)
{
  MS3TraceSeg *seg = 0;
  int samplesize;

  if (!(seg = (MS3TraceSeg *)calloc (1, sizeof (MS3TraceSeg))))
  {
    ms_log (2, "%s(): Error allocating memory\n", __func__);
    return 0;
  }

  /* Populate MS3TraceSeg */
  seg->starttime = msr->starttime;
  seg->endtime = endtime;
  seg->samprate = msr->samprate;
  seg->samplecnt = msr->samplecnt;
  seg->sampletype = msr->sampletype;
  seg->numsamples = msr->numsamples;

  /* Allocate space for and copy datasamples */
  if (msr->datasamples && msr->numsamples)
  {
    samplesize = ms_samplesize (msr->sampletype);

    if (!(seg->datasamples = malloc ((size_t) (samplesize * msr->numsamples))))
    {
      ms_log (2, "%s(): Error allocating memory\n", __func__);
      return 0;
    }

    /* Copy data samples from MS3Record to MS3TraceSeg */
    memcpy (seg->datasamples, msr->datasamples, (size_t) (samplesize * msr->numsamples));
  }

  return seg;
} /* End of mstl3_msr2seg() */

/***************************************************************************
 * Add data coverage from a MS3Record structure to a MS3TraceSeg structure.
 *
 * Data coverage is added to the beginning or end of MS3TraceSeg
 * according to the whence flag:
 * 1 : add coverage to the end
 * 2 : add coverage to the beginninig
 *
 * Return a pointer to a MS3TraceSeg otherwise 0 on error.
 ***************************************************************************/
MS3TraceSeg *
mstl3_addmsrtoseg (MS3TraceSeg *seg, MS3Record *msr, nstime_t endtime, int8_t whence)
{
  int samplesize = 0;
  void *newdatasamples;

  if (!seg || !msr)
    return 0;

  /* Allocate more memory for data samples if included */
  if (msr->datasamples && msr->numsamples > 0)
  {
    if (msr->sampletype != seg->sampletype)
    {
      ms_log (2, "%s(): MS3Record sample type (%c) does not match segment sample type (%c)\n",
              __func__, msr->sampletype, seg->sampletype);
      return 0;
    }

    if (!(samplesize = ms_samplesize (msr->sampletype)))
    {
      ms_log (2, "%s(): Unknown sample size for sample type: %c\n", __func__, msr->sampletype);
      return 0;
    }

    if (!(newdatasamples = realloc (seg->datasamples, (size_t) ((seg->numsamples + msr->numsamples) * samplesize))))
    {
      ms_log (2, "%s(): Error allocating memory\n", __func__);
      return 0;
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
      memcpy ((char *)seg->datasamples + (seg->numsamples * samplesize),
              msr->datasamples,
              (size_t) (msr->numsamples * samplesize));

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
      memmove ((char *)seg->datasamples + (msr->numsamples * samplesize),
               seg->datasamples,
               (size_t) (seg->numsamples * samplesize));

      memcpy (seg->datasamples,
              msr->datasamples,
              (size_t) (msr->numsamples * samplesize));

      seg->numsamples += msr->numsamples;
    }
  }
  else
  {
    ms_log (2, "%s(): unrecognized whence value: %d\n", __func__, whence);
    return 0;
  }

  return seg;
} /* End of mstl3_addmsrtoseg() */

/***************************************************************************
 * Add data coverage from seg2 to seg1.
 *
 * Return a pointer to a seg1 otherwise 0 on error.
 ***************************************************************************/
MS3TraceSeg *
mstl3_addsegtoseg (MS3TraceSeg *seg1, MS3TraceSeg *seg2)
{
  int samplesize = 0;
  void *newdatasamples;

  if (!seg1 || !seg2)
    return 0;

  /* Allocate more memory for data samples if included */
  if (seg2->datasamples && seg2->numsamples > 0)
  {
    if (seg2->sampletype != seg1->sampletype)
    {
      ms_log (2, "%s(): MS3TraceSeg sample types do not match (%c and %c)\n",
              __func__, seg1->sampletype, seg2->sampletype);
      return 0;
    }

    if (!(samplesize = ms_samplesize (seg1->sampletype)))
    {
      ms_log (2, "%s(): Unknown sample size for sample type: %c\n", __func__, seg1->sampletype);
      return 0;
    }

    if (!(newdatasamples = realloc (seg1->datasamples, (size_t) ((seg1->numsamples + seg2->numsamples) * samplesize))))
    {
      ms_log (2, "%s(): Error allocating memory\n", __func__);
      return 0;
    }

    seg1->datasamples = newdatasamples;
  }

  /* Add seg2 coverage to end of seg1 */
  seg1->endtime = seg2->endtime;
  seg1->samplecnt += seg2->samplecnt;

  if (seg2->datasamples && seg2->numsamples > 0)
  {
    memcpy ((char *)seg1->datasamples + (seg1->numsamples * samplesize),
            seg2->datasamples,
            (size_t) (seg2->numsamples * samplesize));

    seg1->numsamples += seg2->numsamples;
  }

  return seg1;
} /* End of mstl3_addsegtoseg() */

/**********************************************************************/ /**
 * @brief Convert the data samples associated with an MS3TraceSeg to another
 * data type
 *
 * ASCII data samples cannot be converted, if supplied or requested an
 * error will be returned.
 *
 * When converting float & double sample types to integer type a
 * simple rounding is applied by adding 0.5 to the sample value before
 * converting (truncating) to integer.  This compensates for common
 * machine representations of floating point values, e.g. "40.0"
 * represented by "39.99999999".
 *
 * If the truncate flag is true (non-zero) data samples will be
 * truncated to integers even if loss of sample precision is detected.
 * If the truncate flag is false (zero) and loss of precision is
 * detected an error is returned.  Loss of precision is determined by
 * testing that the difference between the floating point value and
 * the (truncated) integer value is greater than 0.000001.
 *
 * @param[in] seg The target ::MS3TraceSeg to convert
 * @param[in] type The desired data sample type:
 * @parblock
 *   - \c 'i' - 32-bit integer data type
 *   - \c 'f' - 32-bit float data type
 *   - \c 'd' - 64-bit float (double) data type
 * @endparblock
 * @param[in] truncate Control truncation of floating point values to integers
 *
 * @returns 0 on success, and -1 on failure.
 ***************************************************************************/
int
mstl3_convertsamples (MS3TraceSeg *seg, char type, int8_t truncate)
{
  int32_t *idata;
  float *fdata;
  double *ddata;
  int64_t idx;

  if (!seg)
    return -1;

  /* No conversion necessary, report success */
  if (seg->sampletype == type)
    return 0;

  if (seg->sampletype == 'a' || type == 'a')
  {
    ms_log (2, "mstl3_convertsamples: cannot convert ASCII samples to/from numeric type\n");
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
          ms_log (1, "mstl3_convertsamples: Warning, loss of precision when converting floats to integers, loss: %g\n",
                  (fdata[idx] - (int32_t)fdata[idx]));
          return -1;
        }

        idata[idx] = (int32_t) (fdata[idx] + 0.5);
      }
    }
    else if (seg->sampletype == 'd') /* Convert doubles to integers with simple rounding */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
      {
        /* Check for loss of sub-integer */
        if (!truncate && (ddata[idx] - (int32_t)ddata[idx]) > 0.000001)
        {
          ms_log (1, "mstl3_convertsamples: Warning, loss of precision when converting doubles to integers, loss: %g\n",
                  (ddata[idx] - (int32_t)ddata[idx]));
          return -1;
        }

        idata[idx] = (int32_t) (ddata[idx] + 0.5);
      }

      /* Reallocate buffer for reduced size needed */
      if (!(seg->datasamples = realloc (seg->datasamples, (size_t) (seg->numsamples * sizeof (int32_t)))))
      {
        ms_log (2, "mstl3_convertsamples: cannot re-allocate buffer for sample conversion\n");
        return -1;
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

      /* Reallocate buffer for reduced size needed */
      if (!(seg->datasamples = realloc (seg->datasamples, (size_t) (seg->numsamples * sizeof (float)))))
      {
        ms_log (2, "mstl3_convertsamples: cannot re-allocate buffer after sample conversion\n");
        return -1;
      }
    }

    seg->sampletype = 'f';
  } /* Done converting to 32-bit floats */

  /* Convert to 64-bit doubles */
  else if (type == 'd')
  {
    if (!(ddata = (double *)malloc ((size_t) (seg->numsamples * sizeof (double)))))
    {
      ms_log (2, "mstl3_convertsamples: cannot allocate buffer for sample conversion to doubles\n");
      return -1;
    }

    if (seg->sampletype == 'i') /* Convert integers to doubles */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        ddata[idx] = (double)idata[idx];

      free (idata);
    }
    else if (seg->sampletype == 'f') /* Convert floats to doubles */
    {
      for (idx = 0; idx < seg->numsamples; idx++)
        ddata[idx] = (double)fdata[idx];

      free (fdata);
    }

    seg->datasamples = ddata;
    seg->sampletype = 'd';
  } /* Done converting to 64-bit doubles */

  return 0;
} /* End of mstl3_convertsamples() */

/**********************************************************************/ /**
 * @brief Pack ::MS3TraceList data into miniSEED records
 *
 * The datasamples array and numsamples field will be adjusted
 * (reduced) as data are packed.
 *
 * As each record is filled and finished they are passed to \a
 * record_handler() which should expect 1) a \c char* to the record,
 * 2) the length of the record and 3) a pointer supplied by the
 * original caller containing optional private data (\a handlerdata).
 * It is the responsibility of \a record_handler() to process the
 * record, the memory will be re-used or freed when \a
 * record_handler() returns.
 *
 * If \a extra is not NULL it is expected to contain extraheaders, a
 * string containing (compact) JSON, that will be added to each output
 * record.
 *
 * @param[in] mstl ::MS3TraceList containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the \a record_handler()
 * @param[in] reclen Maximum record length to produce
 * @param[in] encoding Encoding for data samples, see msr3_pack()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Flags used to control the packing process, see msr3_pack()
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 * @param[in] extra If not NULL, add this buffer of extra headers to all records
 *
 * @returns the number of records created on success and -1 on error.
 *
 * \sa msr3_pack()
 ***************************************************************************/
int
mstl3_pack (MS3TraceList *mstl, void (*record_handler) (char *, int, void *),
            void *handlerdata, int reclen, int8_t encoding,
            int64_t *packedsamples, uint32_t flags, int8_t verbose,
            char *extra)
{
  MS3Record *msr = NULL;
  MS3TraceID *id = NULL;
  MS3TraceSeg *seg = NULL;

  int totalpackedrecords = 0;
  int64_t totalpackedsamples = 0;
  int segpackedrecords = 0;
  int64_t segpackedsamples = 0;
  int samplesize;
  int64_t bufsize;

  if (!mstl || !record_handler)
  {
    return -1;
  }

  if (packedsamples)
    *packedsamples = 0;

  msr = msr3_init (NULL);

  if (msr == NULL)
  {
    ms_log (2, "%s(): Error initializing msr, out of memory?\n", __func__);
    return -1;
  }

  msr->reclen = reclen;
  msr->encoding = encoding;

  if (extra)
  {
    msr->extra = extra;
    msr->extralength = strlen(extra);
  }

  /* Loop through trace list */
  id = mstl->traces;
  while (id)
  {
    memcpy (msr->sid, id->sid, sizeof(msr->sid));
    msr->pubversion = id->pubversion;

    /* Loop through segment list */
    seg = id->first;
    while (seg)
    {
      msr->starttime = seg->starttime;
      msr->samprate = seg->samprate;
      msr->samplecnt = seg->samplecnt;
      msr->datasamples = seg->datasamples;
      msr->numsamples = seg->numsamples;
      msr->sampletype = seg->sampletype;

      segpackedrecords = msr3_pack (msr, record_handler, handlerdata, &segpackedsamples, flags, verbose);

      if (verbose > 1)
      {
        ms_log (1, "Packed %d records for %s segment\n", segpackedrecords, msr->sid);
      }

      /* Adjust segment start time, data array and sample count */
      if (segpackedsamples > 0)
      {
        /* The new start time was calculated my msr_pack */
        seg->starttime = msr->starttime;

        samplesize = ms_samplesize (seg->sampletype);
        bufsize = (seg->numsamples - segpackedsamples) * samplesize;

        if (bufsize > 0)
        {
          memmove (seg->datasamples,
                   (uint8_t *)seg->datasamples + (segpackedsamples * samplesize),
                   (size_t)bufsize);

          seg->datasamples = realloc (seg->datasamples, (size_t)bufsize);

          if (seg->datasamples == NULL)
          {
            ms_log (2, "%s(): Cannot (re)allocate datasamples buffer\n", __func__);
            return -1;
          }
        }
        else
        {
          if (seg->datasamples)
            free (seg->datasamples);
          seg->datasamples = NULL;
        }

        seg->samplecnt -= segpackedsamples;
        seg->numsamples -= segpackedsamples;
      }

      totalpackedrecords += segpackedrecords;
      totalpackedsamples += segpackedsamples;

      seg = seg->next;
    }

    id = id->next;
  }

  msr3_free (&msr);

  if (packedsamples)
    *packedsamples = totalpackedsamples;

  return totalpackedrecords;
} /* End of mstl3_pack() */

/**********************************************************************/ /**
 * @brief Print trace list summary information for a ::MS3TraceList
 *
 * By default only print the source ID, starttime and endtime for each
 * trace.  If \a details is greater than 0 include the sample rate,
 * number of samples and a total trace count.  If \a gaps is greater than
 * 0 and the previous trace matches (SID & samprate) include the
 * gap between the endtime of the last trace and the starttime of the
 * current trace.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] timeformat Time string format, one of @ref ms_timeformat_t
 * @param[in] details Flag to control inclusion of more details
 * @param[in] gaps Flag to control inclusion of gap/overlap between segments
 ***************************************************************************/
void
mstl3_printtracelist (MS3TraceList *mstl, ms_timeformat_t timeformat,
                      int8_t details, int8_t gaps)
{
  MS3TraceID *id = 0;
  MS3TraceSeg *seg = 0;
  char stime[30];
  char etime[30];
  char gapstr[20];
  int8_t nogap;
  double gap;
  double delta;
  int tracecnt = 0;
  int segcnt = 0;

  if (!mstl)
  {
    return;
  }

  /* Print out the appropriate header */
  if (details > 0 && gaps > 0)
    ms_log (0, "       SourceID                Start sample             End sample         Gap  Hz  Samples\n");
  else if (details <= 0 && gaps > 0)
    ms_log (0, "       SourceID                Start sample             End sample         Gap\n");
  else if (details > 0 && gaps <= 0)
    ms_log (0, "       SourceID                Start sample             End sample         Hz  Samples\n");
  else
    ms_log (0, "       SourceID                Start sample             End sample\n");

  /* Loop through trace list */
  id = mstl->traces;
  while (id)
  {
    /* Loop through segment list */
    seg = id->first;
    while (seg)
    {
      /* Create formatted time strings */
      if (ms_nstime2timestr (seg->starttime, stime, timeformat, NANO_MICRO) == NULL)
        ms_log (2, "Cannot convert trace start time for %s\n", id->sid);

      if (ms_nstime2timestr (seg->endtime, etime, timeformat, NANO_MICRO) == NULL)
        ms_log (2, "Cannot convert trace end time for %s\n", id->sid);

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
          ms_log (0, "%-24s %-24s %-24s %-4s\n",
                  id->sid, stime, etime, gapstr);
        else
          ms_log (0, "%-24s %-24s %-24s %-s %-3.3g %-" PRId64 "\n",
                  id->sid, stime, etime, gapstr, seg->samprate, seg->samplecnt);
      }
      else if (details > 0 && gaps <= 0)
        ms_log (0, "%-24s %-24s %-24s %-3.3g %-" PRId64 "\n",
                id->sid, stime, etime, seg->samprate, seg->samplecnt);
      else
        ms_log (0, "%-24s %-24s %-24s\n", id->sid, stime, etime);

      segcnt++;
      seg = seg->next;
    }

    tracecnt++;
    id = id->next;
  }

  if (tracecnt != mstl->numtraces)
    ms_log (2, "%s(): number of traces in trace list is inconsistent\n", __func__);

  if (details > 0)
    ms_log (0, "Total: %d trace(s) with %d segment(s)\n", tracecnt, segcnt);

  return;
} /* End of mstl3_printtracelist() */

/**********************************************************************/ /**
 * @brief Print SYNC trace list summary information for a ::MS3TraceList
 *
 * The SYNC header line will be created using the supplied dccid, if
 * the pointer is NULL the string "DCC" will be used instead.
 *
 * If the \a subsecond flag is true the segment start and end times will
 * include subsecond precision, otherwise they will be truncated to
 * integer seconds.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] dccid The DCC identifier to include in the output
 * @param[in] subseconds Inclusion of subseconds, one of @ref ms_subseconds_t
 ***************************************************************************/
void
mstl3_printsynclist (MS3TraceList *mstl, char *dccid, ms_subseconds_t subseconds)
{
  MS3TraceID *id = 0;
  MS3TraceSeg *seg = 0;
  char starttime[30];
  char endtime[30];
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
  id = mstl->traces;
  while (id)
  {
    ms_sid2nslc (id->sid, net, sta, loc, chan);

    /* Loop through segment list */
    seg = id->first;
    while (seg)
    {
      ms_nstime2timestr (seg->starttime, starttime, SEEDORDINAL, subseconds);
      ms_nstime2timestr (seg->endtime, endtime, SEEDORDINAL, subseconds);

      /* Print SYNC line */
      ms_log (0, "%s|%s|%s|%s|%s|%s||%.10g|%" PRId64 "|||||||%s\n",
              net, sta, loc, chan,
              starttime, endtime,
              seg->samprate, seg->samplecnt,
              yearday);

      seg = seg->next;
    }

    id = id->next;
  }

  return;
} /* End of mstl3_printsynclist() */

/**********************************************************************/ /**
 * @brief Print gap/overlap list summary information for a ::MS3TraceList.
 *
 * Overlaps are printed as negative gaps.
 *
 * If \a mingap and \a maxgap are not NULL their values will be
 * enforced and only gaps/overlaps matching their implied criteria
 * will be printed.
 *
 * @param[in] mstl ::MS3TraceList to print
 * @param[in] timeformat Time string format, one of @ref ms_timeformat_t
 * @param[in] mingap Minimum gap to print in seconds (pointer to value)
 * @param[in] maxgap Maximum gap to print in seconds (pointer to value)
 ***************************************************************************/
void
mstl3_printgaplist (MS3TraceList *mstl, ms_timeformat_t timeformat,
                    double *mingap, double *maxgap)
{
  MS3TraceID *id = 0;
  MS3TraceSeg *seg = 0;

  char time1[30], time2[30];
  char gapstr[30];
  double gap;
  double delta;
  double nsamples;
  int8_t printflag;
  int gapcnt = 0;

  if (!mstl)
    return;

  if (!mstl->traces)
    return;

  ms_log (0, "   SourceID              Last Sample              Next Sample       Gap  Samples\n");

  id = mstl->traces;
  while (id)
  {
    seg = id->first;
    while (seg->next)
    {
      /* Skip segments with 0 sample rate, usually from SOH records */
      if (seg->samprate == 0.0)
      {
        seg = seg->next;
        continue;
      }

      gap = (double)(seg->next->starttime - seg->endtime) / NSTMODULUS;

      /* Check that any overlap is not larger than the trace coverage */
      if (gap < 0.0)
      {
        delta = (seg->next->samprate) ? (1.0 / seg->next->samprate) : 0.0;

        if ((gap * -1.0) > (((double)(seg->next->endtime - seg->next->starttime) / NSTMODULUS) + delta))
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
        nsamples = ms_dabs (gap) * seg->samprate;

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
        if (ms_nstime2timestr (seg->endtime, time1, timeformat, NANO_MICRO) == NULL)
          ms_log (2, "Cannot convert trace start time for %s\n", id->sid);

        if (ms_nstime2timestr (seg->next->starttime, time2, timeformat, NANO_MICRO) == NULL)
          ms_log (2, "Cannot convert trace end time for %s\n", id->sid);

        ms_log (0, "%-17s %-24s %-24s %-4s %-.8g\n",
                id->sid, time1, time2, gapstr, nsamples);

        gapcnt++;
      }

      seg = seg->next;
    }

    id = id->next;
  }

  ms_log (0, "Total: %d gap(s)\n", gapcnt);

  return;
} /* End of mstl3_printgaplist() */
