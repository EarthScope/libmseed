/***************************************************************************
 * msrutils.c:
 *
 * Generic routines to operate on miniSEED records.
 *
 * Written by Chad Trabant
 *   IRIS Data Management Center
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"

/***************************************************************************
 * msr3_init:
 *
 * Initialize and return an MS3Record struct, allocating memory if
 * needed.  If memory for the datasamples field has been allocated the
 * pointer will be retained for reuse.  If memory for extra headers
 * has been allocated it will be released.
 *
 * Returns a pointer to a MS3Record struct on success or NULL on error.
 ***************************************************************************/
MS3Record *
msr3_init (MS3Record *msr)
{
  void *datasamples = 0;

  if (!msr)
  {
    msr = (MS3Record *)malloc (sizeof (MS3Record));
  }
  else
  {
    datasamples = msr->datasamples;

    if (msr->extra)
      free (msr->extra);
  }

  if (msr == NULL)
  {
    ms_log (2, "msr3_init(): Cannot allocate memory\n");
    return NULL;
  }

  memset (msr, 0, sizeof (MS3Record));

  msr->datasamples = datasamples;

  msr->reclen    = -1;
  msr->samplecnt = -1;
  msr->encoding  = -1;

  return msr;
} /* End of msr3_init() */

/***************************************************************************
 * msr3_free:
 *
 * Free all memory associated with a MS3Record struct.
 ***************************************************************************/
void
msr3_free (MS3Record **ppmsr)
{
  if (ppmsr != NULL && *ppmsr != 0)
  {
    if ((*ppmsr)->extra)
      free ((*ppmsr)->extra);

    if ((*ppmsr)->datasamples)
      free ((*ppmsr)->datasamples);

    free (*ppmsr);

    *ppmsr = NULL;
  }
} /* End of msr3_free() */

/***************************************************************************
 * msr3_duplicate:
 *
 * Duplicate an M3SRecord struct including the extra headers.  If the
 * datadup flag is true and the source MS3Record has associated data
 * samples copy them as well.
 *
 * Returns a pointer to a new MS3Record on success and NULL on error.
 ***************************************************************************/
MS3Record *
msr3_duplicate (MS3Record *msr, int8_t datadup)
{
  MS3Record *dupmsr = 0;
  int samplesize = 0;

  if (!msr)
    return NULL;

  /* Allocate target MS3Record structure */
  if ((dupmsr = msr3_init (NULL)) == NULL)
    return NULL;

  /* Copy MS3Record structure */
  memcpy (dupmsr, msr, sizeof (MS3Record));

  /* Copy extra headers */
  if (msr->extralength > 0 && msr->extra)
  {
    /* Allocate memory for new FSDH structure */
    if ((dupmsr->extra = (void *)malloc (msr->extralength)) == NULL)
    {
      ms_log (2, "msr3_duplicate(): Error allocating memory\n");
      free (dupmsr);
      return NULL;
    }

    memcpy (dupmsr->extra, msr->extra, msr->extralength);
  }

  /* Copy data samples if requested and available */
  if (datadup && msr->numsamples > 0 && msr->datasamples)
  {
    /* Determine size of samples in bytes */
    samplesize = ms_samplesize (msr->sampletype);

    if (samplesize == 0)
    {
      ms_log (2, "msr3_duplicate(): unrecognized sample type: '%c'\n",
              msr->sampletype);
      free (dupmsr);
      return NULL;
    }

    /* Allocate memory for new data array */
    if ((dupmsr->datasamples = (void *)malloc ((size_t) (msr->numsamples * samplesize))) == NULL)
    {
      ms_log (2, "msr3_duplicate(): Error allocating memory\n");
      free (dupmsr);
      return NULL;
    }

    memcpy (dupmsr->datasamples, msr->datasamples, ((size_t) (msr->numsamples * samplesize)));
  }
  /* Otherwise make sure the sample array and count are zero */
  else
  {
    dupmsr->datasamples = 0;
    dupmsr->numsamples  = 0;
  }

  return dupmsr;
} /* End of msr3_duplicate() */

/***************************************************************************
 * msr_endtime:
 *
 * Calculate the time of the last sample in the record; this is the
 * actual last sample time and *not* the time "covered" by the last
 * sample.
 *
 * On the epoch time scale the value of a leap second is the same as
 * the second following the leap second, without external information
 * the values are ambiguous.
 *
 * Leap second handling: when a record completely contains a leap
 * second, starts before and ends after, the calculated end time will
 * be adjusted (reduced) by one second.
 *
 * Returns the time of the last sample as a high precision epoch time
 * on success and NSTERROR on error.
 ***************************************************************************/
nstime_t
msr3_endtime (MS3Record *msr)
{
  nstime_t span = 0;
  LeapSecond *lslist = leapsecondlist;

  if (!msr)
    return NSTERROR;

  if (msr->samprate > 0.0 && msr->samplecnt > 0)
    span = (hptime_t) (((double)(msr->samplecnt - 1) / msr->samprate * NSTMODULUS) + 0.5);

  /* Check if the record contains a leap second, if list is available */
  if (lslist)
  {
    while (lslist)
    {
      if (lslist->leapsecond > msr->starttime &&
          lslist->leapsecond <= (msr->starttime + span - NSTMODULUS))
      {
        span -= NSTMODULUS;
        break;
      }

      lslist = lslist->next;
    }
  }

  return (msr->starttime + span);
} /* End of msr3_endtime() */


/***************************************************************************
 * msr3_print:
 *
 * Prints header values in an MS3Record struct.
 *
 * The value of details functions as follows:
 *  0 = print a single summary line
 *  1 = print most details of header
 * >1 = print all details of header and extra headers if present
 *
 ***************************************************************************/
void
msr3_print (MS3Record *msr, int8_t details)
{
  char time[25];
  char b;

  if (!msr)
    return;

  /* Generate a start time string */
  ms_nstime2seedtimestr (msr->starttime, time, 1);

  /* Report information in the fixed header */
  if (details > 0)
  {
    ms_log (0, "%s, %d, %d (format: %d)\n",
            msr->tsid, msr->pubversion, msr->reclen, msr->formatversion);
    ms_log (0, "             start time: %s\n", time);
    ms_log (0, "      number of samples: %d\n", msr->samplecnt);
    ms_log (0, "       sample rate (Hz): %.10g\n", msr_sampratehz(msr));

    if (details > 1)
    {
      b = msr->flags;
      ms_log (0, "                  flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
              bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
              bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
      if (b & 0x01)
        ms_log (0, "                         [Bit 0] Calibration signals present\n");
      if (b & 0x02)
        ms_log (0, "                         [Bit 1] Time tag is questionable\n");
      if (b & 0x04)
        ms_log (0, "                         [Bit 2] Clock locked\n");
      if (b & 0x08)
        ms_log (0, "                         [Bit 3] Undefined bit set\n");
      if (b & 0x10)
        ms_log (0, "                         [Bit 4] Undefined bit set\n");
      if (b & 0x20)
        ms_log (0, "                         [Bit 5] Undefined bit set\n");
      if (b & 0x40)
        ms_log (0, "                         [Bit 6] Undefined bit set\n");
      if (b & 0x80)
        ms_log (0, "                         [Bit 7] Undefined bit set\n");
    }

    ms_log (0, "                    CRC: 0x%0X\n", msr->crc);
    ms_log (0, "    extra header length: %d bytes\n", msr->extralength);
    ms_log (0, "    data payload length: %d bytes\n", msr->datalength);
    ms_log (0, "       payload encoding: %s (val: %d)\n",
            (char *)ms_encodingstr (msr->encoding), msr->encoding);

    if (details > 1 && msr->extralength > 0 && msr->extra)
    {
      ms_log (0, "          extra headers:\n");
      mseh_print (msr, 16);
    }
  }
  else
  {
    ms_log (0, "%s, %d, %d, %" PRId64 " samples, %-.10g Hz, %s\n",
            msr->tsid, msr->pubversion, msr->reclen,
            msr->samplecnt, msr->samprate, time);
  }
} /* End of msr3_print() */

/***************************************************************************
 * msr3_host_latency:
 *
 * Calculate the latency based on the host time in UTC accounting for
 * the time covered using the number of samples and sample rate; in
 * other words, the difference between the host time and the time of
 * the last sample in the specified miniSEED record.
 *
 * Double precision is returned, but the true precision is dependent
 * on the accuracy of the host system clock among other things.
 *
 * Returns seconds of latency or 0.0 on error (indistinguishable from
 * 0.0 latency).
 ***************************************************************************/
double
msr3_host_latency (MS3Record *msr)
{
  double span = 0.0; /* Time covered by the samples */
  double epoch;      /* Current epoch time */
  double latency = 0.0;
  time_t tv;

  if (msr == NULL)
    return 0.0;

  /* Calculate the time covered by the samples */
  if (msr->samprate > 0.0 && msr->samplecnt > 0)
    span = (1.0 / msr->samprate) * (msr->samplecnt - 1);

  /* Grab UTC time according to the system clock */
  epoch = (double)time (&tv);

  /* Now calculate the latency */
  latency = epoch - ((double)msr->starttime / NSTMODULUS) - span;

  return latency;
} /* End of msr3_host_latency() */
