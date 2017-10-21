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
#include "packdata.h"

/* Function(s) internal to this file */
static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          char *tsid, int8_t verbose);

/***************************************************************************
 * msr3_pack:
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
msr3_pack (MS3Record *msr, void (*record_handler) (char *, int, void *),
           void *handlerdata, int64_t *packedsamples, int8_t flush, int8_t verbose)
{
  char *rawrec;
  int8_t headerswapflag = 0;
  int8_t dataswapflag = 0;

  int byteorder = -1;
  int samplesize;
  int headerlen = 0;
  int dataoffset = 0;
  int maxdatabytes;
  int maxsamples;
  int recordcnt = 0;
  int packsamples, packoffset;
  int64_t totalpackedsamples;
  nstime_t segstarttime;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "msr3_pack(): record_handler() function pointer not set!\n");
    return -1;
  }

  /* Track original segment start time for new start time calculation */
  segstarttime = msr->starttime;

  /* Set default indicator, record length, byte order and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  /* Set byte order according to format version */
  if (msr->formatversion == 2)
    byteorder = 1;
  else if (msr->formatversion == 3)
    byteorder = 0;

  if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_pack(%s): Record length is out of range: %d\n",
            msr->tsid, msr->reclen);
    return -1;
  }

  if (msr->numsamples <= 0)
  {
    ms_log (2, "msr3_pack(%s): No samples to pack\n", msr->tsid);
    return -1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "msr3_pack(%s): Unknown sample type '%c'\n",
            msr->tsid, msr->sampletype);
    return -1;
  }

  // NEEDS reworking below.  Allocations for mseed3 are more variant.  Split out for 2 versions?

  /* Allocate space for data record */
  rawrec = (char *)malloc (msr->reclen);

  if (rawrec == NULL)
  {
    ms_log (2, "msr3_pack(%s): Cannot allocate memory\n", msr->tsid);
    return -1;
  }

  /* Check to see if byte swapping is needed */
  if (byteorder != ms_bigendianhost ())
    headerswapflag = dataswapflag = 1;

  if (verbose > 2)
  {
    if (headerswapflag && dataswapflag)
      ms_log (1, "%s: Byte swapping needed for packing of header and data samples\n", msr->tsid);
    else if (headerswapflag)
      ms_log (1, "%s: Byte swapping needed for packing of header\n", msr->tsid);
    else if (dataswapflag)
      ms_log (1, "%s: Byte swapping needed for packing of data samples\n", msr->tsid);
    else
      ms_log (1, "%s: Byte swapping NOT needed for packing\n", msr->tsid);
  }

  /*
  headerlen = msr_pack_header_raw (msr, rawrec, msr->reclen, headerswapflag, 1,
                                   &HPblkt1001, msr->tsid, verbose);

  if (headerlen == -1)
  {
    ms_log (2, "msr_pack(%s): Error packing header\n", msr->tsid);
    free (rawrec);
    return -1;
  }
  */

  //CHAD, dataoffset is fsdh(36) + length of TSID + length of extras

  /* Determine offset to encoded data */
  if (msr->encoding == DE_STEIM1 || msr->encoding == DE_STEIM2)
  {
    dataoffset = 64;
    while (dataoffset < headerlen)
      dataoffset += 64;

    /* Zero memory between blockettes and data if any */
    memset (rawrec + headerlen, 0, dataoffset - headerlen);
  }
  else
  {
    dataoffset = headerlen;
  }

  // CHAD, needs reworking

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

  /* Pack samples into records */
  totalpackedsamples = 0;
  packoffset = 0;
  if (packedsamples)
    *packedsamples = 0;

  while ((msr->numsamples - totalpackedsamples) > maxsamples || flush)
  {
    packsamples = msr_pack_data (rawrec + dataoffset,
                                 (char *)msr->datasamples + packoffset,
                                 (int)(msr->numsamples - totalpackedsamples), maxdatabytes,
                                 msr->sampletype, msr->encoding, dataswapflag,
                                 msr->tsid, verbose);

    if (packsamples < 0)
    {
      ms_log (2, "msr3_pack(%s): Error packing data samples\n", msr->tsid);
      free (rawrec);
      return -1;
    }

    packoffset += packsamples * samplesize;

    /* Update number of samples */
    //CHAD, set packed samples in header
    //CHAD, set payload length in header

    if (verbose > 0)
      ms_log (1, "%s: Packed %d samples\n", msr->tsid, packsamples);

    /* Send record to handler */
    record_handler (rawrec, msr->reclen, handlerdata);

    totalpackedsamples += packsamples;
    if (packedsamples)
      *packedsamples = totalpackedsamples;

    recordcnt++;

    if (totalpackedsamples >= msr->numsamples)
      break;
  }

  if (verbose > 2)
    ms_log (1, "%s: Packed %d total samples\n", msr->tsid, totalpackedsamples);

  free (rawrec);

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
               char *tsid, int8_t verbose)
{
  int nsamples;

  /* Check for encode debugging environment variable */
  if (getenv ("ENCODE_DEBUG"))
    encodedebug = 1;

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

    nsamples = msr_encode_steim1 (src, maxsamples, dest, maxdatabytes, 0, swapflag);

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

    nsamples = msr_encode_steim2 (src, maxsamples, dest, maxdatabytes, 0, tsid, swapflag);

    break;

  default:
    ms_log (2, "%s: Unable to pack format %d\n", tsid, encoding);

    return -1;
  }

  return nsamples;
} /* End of msr_pack_data() */
