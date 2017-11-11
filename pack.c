/***************************************************************************
 * pack.c:
 *
 * Generic routines to pack miniSEED records using an MS3Record as a
 * header template and data source.
 *
 * Written by Chad Trabant,
 *   IRIS Data Management Center
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "mseedformat.h"
#include "packdata.h"

/* Function(s) internal to this file */
static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          uint16_t *byteswritten, char *tsid, int8_t verbose);

/***************************************************************************
 * msr3_pack3:
 *
 * Pack data into miniSEED records.  Using the record header values
 * in the MS3Record as a template the common header fields are packed
 * into the record header, blockettes in the blockettes chain are
 * packed and data samples are packed in the encoding format indicated
 * by the MS3Record->encoding field.
 *
 * The MS3Record->datasamples array and MS3Record->numsamples value will
 * not be changed by this routine.  It is the responsibility of the
 * calling routine to adjust the data buffer if desired.
 *
 * As each record is filled and finished they are passed to
 * record_handler which expects 1) a char * to the record, 2) the
 * length of the record and 3) a pointer supplied by the original
 * caller containing optional private data (handlerdata).  It is the
 * responsibility of record_handler to process the record, the memory
 * will be re-used or freed when record_handler returns.
 *
 * If the flush flag != 0 all of the data will be packed into data
 * records even though the last one will probably not be filled.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2)
 * and byteorder = 0 (LSBF).  The defaults are triggered when
 * msr->reclen, msr->encoding and msr->byteorder are -1 respectively.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
msr3_pack3 (MS3Record *msr, void (*record_handler) (char *, int, void *),
            void *handlerdata, int64_t *packedsamples, int8_t flush, int8_t verbose)
{
  char rawrec[MAXRECLEN];
  char encoded[65535];  /* Separate encoded data buffer for alignment */
  int8_t swapflag;

  int samplesize;
  int maxdatabytes;
  int maxsamples;
  int recordcnt = 0;
  int packsamples, packoffset;
  int64_t totalpackedsamples;
  int32_t reclen;

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  double samprate;
  uint32_t numsamples;
  uint32_t crc;
  uint8_t tsidlength;
  uint16_t extralength;
  uint16_t datalength;
  char *extraptr;
  char *dataptr;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "msr3_pack3(): record_handler() function pointer not set!\n");
    return -1;
  }

  /* Set default indicator, record length, byte order and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_pack3(%s): Record length is out of range: %d\n",
            msr->tsid, msr->reclen);
    return -1;
  }

  if (msr->numsamples <= 0)
  {
    ms_log (2, "msr3_pack3(%s): No samples to pack\n", msr->tsid);
    return -1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "msr3_pack3(%s): Unknown sample type '%c'\n",
            msr->tsid, msr->sampletype);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  if (verbose > 2 && swapflag)
    ms_log (1, "%s: Byte swapping needed for packing of header\n", msr->tsid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "msr3_pack3(%s): Cannot convert starttime: %" PRId64 "\n",
            msr->tsid, msr->starttime);
    return -1;
  }

  samprate = msr->samprate;
  tsidlength = strlen (msr->tsid);
  extralength = msr->extralength;

  extraptr = rawrec + MS3FSDH_LENGTH + tsidlength;
  dataptr = extraptr + extralength;

  if (swapflag)
  {
    ms_gswap2a (&year);
    ms_gswap2a (&day);
    ms_gswap4a (&nsec);
    ms_gswap4a (&samprate);
    ms_gswap2a (&extralength);
  }

  /* Build fixed header */
  rawrec[0] = 'M';
  rawrec[1] = 'S';
  *pMS3FSDH_FORMATVERSION(rawrec) = 3;
  *pMS3FSDH_FLAGS(rawrec) = msr->flags;
  memcpy (pMS3FSDH_YEAR(rawrec), &year, sizeof (uint16_t));
  memcpy (pMS3FSDH_DAY(rawrec), &day, sizeof (uint16_t));
  *pMS3FSDH_HOUR(rawrec) = hour;
  *pMS3FSDH_MIN(rawrec) = min;
  *pMS3FSDH_SEC(rawrec) = sec;
  *pMS3FSDH_ENCODING(rawrec) = msr->encoding;
  memcpy (pMS3FSDH_NSEC(rawrec), &nsec, sizeof (uint32_t));
  memcpy (pMS3FSDH_SAMPLERATE(rawrec), &samprate, sizeof (double));
  *pMS3FSDH_PUBVERSION(rawrec) = msr->pubversion;
  *pMS3FSDH_TSIDLENGTH(rawrec) = tsidlength;
  memcpy (pMS3FSDH_TSID(rawrec), msr->tsid, tsidlength);

  if (msr->extralength > 0)
    memcpy (extraptr, msr->extra, msr->extralength);

  /* Determine the max data bytes and sample count */
  maxdatabytes = msr->reclen - (dataptr - rawrec);

  if (msr->encoding == DE_STEIM1)
  {
    maxsamples = (int)(maxdatabytes / 64) * STEIM1_FRAME_MAX_SAMPLES;
  }
  else if (msr->encoding == DE_STEIM2)
  {
    maxsamples = (int)(maxdatabytes / 64) * STEIM2_FRAME_MAX_SAMPLES;
  }
  else
  {
    maxsamples = maxdatabytes / samplesize;
  }

  /* Pack samples into records */
  totalpackedsamples = 0;
  packoffset = 0;
  if (packedsamples)
    *packedsamples = 0;

  while ((msr->numsamples - totalpackedsamples) > maxsamples || flush)
  {
    packsamples = msr_pack_data (encoded,
                                 (char *)msr->datasamples + packoffset,
                                 (int)(msr->numsamples - totalpackedsamples), maxdatabytes,
                                 msr->sampletype, msr->encoding, swapflag,
                                 &datalength, msr->tsid, verbose);

    if (packsamples < 0)
    {
      ms_log (2, "msr3_pack3(%s): Error packing data samples\n", msr->tsid);
      return -1;
    }

    packoffset += packsamples * samplesize;
    reclen = MS3FSDH_LENGTH + tsidlength + msr->extralength + datalength;

    /* Copy encoded data into record */
    memcpy (dataptr, encoded, datalength);

    /* Update number of samples and data length */
    numsamples = HO4u (packsamples, swapflag);
    memcpy (pMS3FSDH_NUMSAMPLES(rawrec), &numsamples, sizeof(uint32_t));

    datalength = HO2u (datalength, swapflag);
    memcpy (pMS3FSDH_DATALENGTH(rawrec), &datalength, sizeof(uint16_t));

    /* Calculate CRC and set */
    memset (pMS3FSDH_CRC(rawrec), 0, sizeof(uint32_t));
    crc = ms_crc32c ((const uint8_t*)rawrec, reclen, 0);
    crc = HO4u (crc, swapflag);
    memcpy (pMS3FSDH_CRC(rawrec), &crc, sizeof(uint32_t));

    if (verbose > 0)
      ms_log (1, "%s: Packed %d samples into %d byte record\n", msr->tsid, packsamples, reclen);

    /* Send record to handler */
    record_handler (rawrec, reclen, handlerdata);

    totalpackedsamples += packsamples;
    if (packedsamples)
      *packedsamples = totalpackedsamples;

    recordcnt++;

    if (totalpackedsamples >= msr->numsamples)
      break;
  }

  if (verbose > 2)
    ms_log (1, "%s: Packed %d total samples\n", msr->tsid, totalpackedsamples);

  return recordcnt;
} /* End of msr_pack() */

/************************************************************************
 *  msr_pack_data:
 *
 *  Pack miniSEED data samples.  The input data samples specified as
 *  'src' will be packed with 'encoding' format and placed in 'dest'.
 *
 *  If a pointer to a 32-bit integer sample is provided in the
 *  argument 'lastintsample' and 'comphistory' is true the sample
 *  value will be used to seed the difference buffer for Steim1/2
 *  encoding and provide a compression history.  It will also be
 *  updated with the last sample packed in order to be used with a
 *  subsequent call to this routine.
 *
 *  Return number of samples packed on success and a negative on error.
 ************************************************************************/
static int
msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
               char sampletype, int8_t encoding, int8_t swapflag,
               uint16_t *byteswritten, char *tsid, int8_t verbose)
{
  int nsamples;

  /* Check for encode debugging environment variable */
  if (getenv ("ENCODE_DEBUG"))
    encodedebug = 1;

  if (byteswritten)
    *byteswritten = 0;

  /* Decide if this is a format that we can encode */
  switch (encoding)
  {
  case DE_ASCII:
    if (sampletype != 'a')
    {
      ms_log (2, "%s: Sample type must be ascii (a) for ASCII text encoding not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing ASCII data\n", tsid);

    nsamples = msr_encode_text (src, maxsamples, dest, maxdatabytes);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples;

    break;

  case DE_INT16:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT16 encoding not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing INT16 data samples\n", tsid);

    nsamples = msr_encode_int16 (src, maxsamples, dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 2;

    break;

  case DE_INT32:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT32 encoding not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing INT32 data samples\n", tsid);

    nsamples = msr_encode_int32 (src, maxsamples, dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 4;

    break;

  case DE_FLOAT32:
    if (sampletype != 'f')
    {
      ms_log (2, "%s: Sample type must be float (f) for FLOAT32 encoding not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing FLOAT32 data samples\n", tsid);

    nsamples = msr_encode_float32 (src, maxsamples, dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 4;

    break;

  case DE_FLOAT64:
    if (sampletype != 'd')
    {
      ms_log (2, "%s: Sample type must be double (d) for FLOAT64 encoding not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing FLOAT64 data samples\n", tsid);

    nsamples = msr_encode_float64 (src, maxsamples, dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 8;

    break;

  case DE_STEIM1:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim1 compression not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing Steim1 data frames\n", tsid);

    /* Always big endian Steim1 */
    swapflag = (ms_bigendianhost()) ? 0 : 1;

    nsamples = msr_encode_steim1 (src, maxsamples, dest, maxdatabytes, 0, byteswritten, swapflag);

    break;

  case DE_STEIM2:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim2 compression not '%c'\n",
              tsid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing Steim2 data frames\n", tsid);

    /* Always big endian Steim2 */
    swapflag = (ms_bigendianhost()) ? 0 : 1;

    nsamples = msr_encode_steim2 (src, maxsamples, dest, maxdatabytes, 0, byteswritten, tsid, swapflag);

    break;

  default:
    ms_log (2, "%s: Unable to pack format %d\n", tsid, encoding);

    return -1;
  }

  return nsamples;
} /* End of msr_pack_data() */
