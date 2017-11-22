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
static int msr3_pack_mseed3 (MS3Record *msr, void (*record_handler) (char *, int, void *),
                             void *handlerdata, int64_t *packedsamples,
                             int8_t flush, int8_t verbose);

static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          uint16_t *byteswritten, char *tsid, int8_t verbose);

/***************************************************************************
 * msr3_pack:
 *
 * Pack data into miniSEED records. Packing is performed according to the
 * version at MS3Record->formatversion.
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
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when msr->reclen and msr->encoding are
 * -1 respectively.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
msr3_pack (MS3Record *msr, void (*record_handler) (char *, int, void *),
           void *handlerdata, int64_t *packedsamples, int8_t flush, int8_t verbose)
{
  int packedrecs = 0;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "msr3_pack(): record_handler() function pointer not set!\n");
    return -1;
  }

  /* Set default record length and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_pack(%s): Record length is out of range: %d\n",
            msr->tsid, msr->reclen);
    return -1;
  }

  if (msr->formatversion == 2)
  {
    /* TODO */
    //packedrecs = msr3_pack_mseed2(msr, record_handler, handlerdata, packedsamples,
    //                           flush, verbose);
    ms_log (1, "miniSEED version 2 packing not yet supported\n");
    return -1;
  }
  else /* Pack version 3 by default */
  {
    packedrecs = msr3_pack_mseed3 (msr, record_handler, handlerdata, packedsamples,
                                   flush, verbose);
  }

  return packedrecs;
} /* End of msr3_pack() */

/***************************************************************************
 * msr3_pack_mseed3:
 *
 * Pack data into miniSEED version 3 records.
 *
 * Returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
msr3_pack_mseed3 (MS3Record *msr, void (*record_handler) (char *, int, void *),
                  void *handlerdata, int64_t *packedsamples,
                  int8_t flush, int8_t verbose)
{
  char *rawrec = NULL;
  char *encoded = NULL;  /* Separate encoded data buffer for alignment */
  int8_t swapflag;
  int dataoffset = 0;

  int samplesize;
  int maxdatabytes;
  int maxsamples;
  int recordcnt = 0;
  int packsamples;
  int packoffset;
  int64_t totalpackedsamples;
  int32_t reclen;

  uint32_t crc;
  uint16_t datalength;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "msr3_pack_mseed3(): record_handler() function pointer not set!\n");
    return -1;
  }

  if (msr->reclen < (MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "msr3_pack_mseed3(%s): Record length (%d) is not large enough for header (%d) and extra (%d)\n",
            msr->tsid, msr->reclen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  if (msr->numsamples <= 0)
  {
    ms_log (2, "msr3_pack_mseed3(%s): No samples to pack\n", msr->tsid);
    return -1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "msr3_pack_mseed3(%s): Unknown sample type '%c'\n",
            msr->tsid, msr->sampletype);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  /* Allocate space for data record */
  rawrec = (char *)malloc (msr->reclen);

  if (rawrec == NULL)
  {
    ms_log (2, "msr3_pack_mseed3(%s): Cannot allocate memory\n", msr->tsid);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, rawrec, msr->reclen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "msr3_pack_mseed3(%s): Cannot pack miniSEED version 3 header\n",
            msr->tsid);
    return -1;
  }

  /* Determine the max data bytes and sample count */
  maxdatabytes = msr->reclen - dataoffset;

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

  /* Allocate space for encoded data separately for alignment */
  if (msr->numsamples > 0)
  {
    encoded = (char *)malloc (maxdatabytes);

    if (encoded == NULL)
    {
      ms_log (2, "msr3_pack_mseed3(%s): Cannot allocate memory\n", msr->tsid);
      free (rawrec);
      return -1;
    }
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
      ms_log (2, "msr3_pack_mseed3(%s): Error packing data samples\n", msr->tsid);
      free (encoded);
      free (rawrec);
      return -1;
    }

    packoffset += packsamples * samplesize;
    reclen = dataoffset + datalength;

    /* Copy encoded data into record */
    memcpy (rawrec + dataoffset, encoded, datalength);

    /* Update number of samples and data length */
    *pMS3FSDH_NUMSAMPLES(rawrec) = HO4u (packsamples, swapflag);
    *pMS3FSDH_DATALENGTH(rawrec) = HO2u (datalength, swapflag);

    /* Calculate CRC (with CRC field set to 0) and set */
    memset (pMS3FSDH_CRC(rawrec), 0, sizeof(uint32_t));
    crc = ms_crc32c ((const uint8_t*)rawrec, reclen, 0);
    *pMS3FSDH_CRC(rawrec) = HO4u (crc, swapflag);

    if (verbose >= 1)
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

  if (verbose >= 2)
    ms_log (1, "%s: Packed %d total samples\n", msr->tsid, totalpackedsamples);

  if (encoded)
    free (encoded);

  free (rawrec);

  return recordcnt;
} /* End of msr3_pack_mseed3() */

/***************************************************************************
 * msr3_repack_mseed3:
 *
 * Re-pack a parsed miniSEED record into a version 3 record.  Pack the
 * parsed header into a version 3 header and copy the raw encoded data
 * from the original record.  The original record must be at the
 * MS3Record->record pointer.
 *
 * This can be used to convert format versions or modify header values
 * without unpacking the data samples.
 *
 * Returns record length on success and -1 on error.
 ***************************************************************************/
int
msr3_repack_mseed3 (MS3Record *msr, char *record, uint32_t reclen,
                    int8_t verbose)
{
  int dataoffset;
  uint32_t origdataoffset;
  uint16_t origdatasize;
  uint32_t crc;
  int8_t swapflag;

  if (!msr)
    return -1;

  if (!record)
  {
    ms_log (2, "msr3_repack_mseed3(): record buffer is not set!\n");
    return -1;
  }

  if (reclen < (MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "msr3_repack_mseed3(%s): Record length (%d) is not large enough for header (%d) and extra (%d)\n",
            msr->tsid, reclen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  if (msr->samplecnt > UINT32_MAX)
  {
    ms_log (2, "msr3_repack_mseed3(%s): Too many samples in input record (%" PRId64 " for a single record)\n",
            msr->tsid, msr->samplecnt);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, record, reclen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "msr3_repack_mseed3(%s): Cannot pack miniSEED version 3 header\n",
            msr->tsid);
    return -1;
  }

  /* Determine encoded data size */
  if (msr3_data_bounds (msr, &origdataoffset, &origdatasize))
  {
    ms_log (2, "msr3_repack_mseed3(%s): Cannot determine original data bounds\n",
            msr->tsid);
    return -1;
  }

  if (reclen < (MS3FSDH_LENGTH + msr->extralength + origdatasize))
  {
    ms_log (2, "msr3_repack_mseed3(%s): Record length (%d) is not large enough for record (%d)\n",
            msr->tsid, reclen, (MS3FSDH_LENGTH + msr->extralength + origdatasize));
    return -1;
  }

  reclen = dataoffset + origdatasize;

  /* Copy encoded data into record */
  memcpy (record + dataoffset, msr->record + origdataoffset, origdatasize);

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  /* Update number of samples and data length */
  *pMS3FSDH_NUMSAMPLES(record) = HO4u ((uint32_t)msr->samplecnt, swapflag);
  *pMS3FSDH_DATALENGTH(record) = HO2u (origdatasize, swapflag);

  /* Calculate CRC (with CRC field set to 0) and set */
  memset (pMS3FSDH_CRC(record), 0, sizeof(uint32_t));
  crc = ms_crc32c ((const uint8_t*)record, reclen, 0);
  *pMS3FSDH_CRC(record) = HO4u (crc, swapflag);

  if (verbose >= 1)
    ms_log (1, "%s: Re-packed %d samples into a %d byte record\n",
            msr->tsid, msr->samplecnt, reclen);

  return reclen;
} /* End of msr3_repack_mseed3() */

/***************************************************************************
 * msr3_pack_header3:
 *
 * Pack a miniSEED version 3 header into the specified buffer.
 *
 * Default values are: maximum record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when msr->reclen and msr->encoding are
 * -1 respectively.
 *
 * Returns the size of the header (fixed and extra) on success, otherwise -1.
 ***************************************************************************/
int
msr3_pack_header3 (MS3Record *msr, char *record, uint32_t reclen, int8_t verbose)
{
  int extraoffset = 0;
  uint8_t tsidlength;
  int8_t swapflag;

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr || !record)
    return -1;

  /* Set default record length and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_pack_header3(%s): Record length is out of range: %d\n",
            msr->tsid, msr->reclen);
    return -1;
  }

  if (reclen < (MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "msr3_pack_header3(%s): Buffer length (%d) is not large enough for fixed header (%d) and extra (%d)\n",
            msr->tsid, msr->reclen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  if (verbose > 2 && swapflag)
    ms_log (1, "%s: Byte swapping needed for packing of header\n", msr->tsid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "msr3_pack_header3(%s): Cannot convert starttime: %" PRId64 "\n",
            msr->tsid, msr->starttime);
    return -1;
  }

  tsidlength = strlen (msr->tsid);
  extraoffset = MS3FSDH_LENGTH + tsidlength;

  /* Build fixed header */
  record[0] = 'M';
  record[1] = 'S';
  *pMS3FSDH_FORMATVERSION (record) = 3;
  *pMS3FSDH_FLAGS (record) = msr->flags;
  *pMS3FSDH_YEAR (record) = HO2u (year, swapflag);
  *pMS3FSDH_DAY (record) = HO2u (day, swapflag);
  *pMS3FSDH_HOUR (record) = hour;
  *pMS3FSDH_MIN (record) = min;
  *pMS3FSDH_SEC (record) = sec;
  *pMS3FSDH_ENCODING (record) = msr->encoding;
  *pMS3FSDH_NSEC (record) = HO4u (nsec, swapflag);

  /* If rate positive and less than one, convert to period notation */
  if (msr->samprate != 0.0 && msr->samprate > 0 && msr->samprate < 1.0)
    *pMS3FSDH_SAMPLERATE(record) = HO8f((-1.0 / msr->samprate), swapflag);
  else
    *pMS3FSDH_SAMPLERATE(record) = HO8f(msr->samprate, swapflag);

  *pMS3FSDH_PUBVERSION(record) = msr->pubversion;
  *pMS3FSDH_TSIDLENGTH(record) = tsidlength;
  *pMS3FSDH_EXTRALENGTH(record) = HO2u(msr->extralength, swapflag);
  memcpy (pMS3FSDH_TSID(record), msr->tsid, tsidlength);

  if (msr->extralength > 0)
    memcpy (record + extraoffset, msr->extra, msr->extralength);

  return (MS3FSDH_LENGTH + tsidlength + msr->extralength);
} /* End of msr3_pack_header3() */

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
