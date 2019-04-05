/***************************************************************************
 * Generic routines to pack miniSEED records using an MS3Record as a
 * header template and data source.
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
                             uint32_t flags, int8_t verbose);

static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          uint16_t *byteswritten, char *sid, int8_t verbose);

/**********************************************************************/ /**
 * @brief Pack data into miniSEED records.
 *
 * Packing is performed according to the version at
 * @ref MS3Record.formatversion.
 *
 * The @ref MS3Record.datasamples array and @ref MS3Record.numsamples
 * value will __not__ be changed by this routine.  It is the
 * responsibility of the calling routine to adjust the data buffer if
 * desired.
 *
 * As each record is filled and finished they are passed to \a
 * record_handler() which should expect 1) a \c char* to the record,
 * 2) the length of the record and 3) a pointer supplied by the
 * original caller containing optional private data (\a handlerdata).
 * It is the responsibility of \a record_handler() to process the
 * record, the memory will be re-used or freed when \a
 * record_handler() returns.
 *
 * The following data encodings and expected @ref MS3Record.sampletype
 * are supported:
 *   - ::DE_ASCII (0), text (ASCII), expects type \c 'a'
 *   - ::DE_INT16 (1), 16-bit integer, expects type \c 'i'
 *   - ::DE_INT32 (3), 32-bit integer, expects type \c 'i'
 *   - ::DE_FLOAT32 (4), 32-bit float (IEEE), expects type \c 'f'
 *   - ::DE_FLOAT64 (5), 64-bit float (IEEE), expects type \c 'd'
 *   - ::DE_STEIM1 (10), Stiem-1 compressed integers, expects type \c 'i'
 *   - ::DE_STEIM2 (11), Stiem-2 compressed integers, expects type \c 'i'
 *
 * If \a flags has ::MSF_FLUSHDATA set, all of the data will be packed
 * into data records even though the last one will probably be smaller
 * than requested or, in the case of miniSEED 2.X, unfilled.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when \a msr.reclen and \a msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the \a record_handler()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Flags used to control the packing process:
 * @parblock
 *  - \c ::MSF_FLUSHDATA : Pack all data in the buffer
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the number of records created on success and -1 on error.
 ***************************************************************************/
int
msr3_pack (MS3Record *msr, void (*record_handler) (char *, int, void *),
           void *handlerdata, int64_t *packedsamples, uint32_t flags, int8_t verbose)
{
  int packedrecs = 0;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "%s(): record_handler() function pointer not set!\n", __func__);
    return -1;
  }

  /* Set default record length and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "%s(%s): Record length is out of range: %d\n",
            __func__, msr->sid, msr->reclen);
    return -1;
  }

  if (msr->formatversion == 2)
  {
    /* TODO */
    //packedrecs = msr3_pack_mseed2(msr, record_handler, handlerdata, packedsamples,
    //                           flags, verbose);
    ms_log (1, "%s(%s): miniSEED version 2 packing not yet supported\n",
            __func__, msr->sid);
    return -1;
  }
  else /* Pack version 3 by default */
  {
    packedrecs = msr3_pack_mseed3 (msr, record_handler, handlerdata, packedsamples,
                                   flags, verbose);
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
                  uint32_t flags, int8_t verbose)
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
    ms_log (2, "%s(): record_handler() function pointer not set!\n", __func__);
    return -1;
  }

  if (msr->reclen < (MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "%s(%s): Record length (%d) is not large enough for header (%d) and extra (%d)\n",
            __func__, msr->sid, msr->reclen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  if (msr->numsamples <= 0)
  {
    ms_log (2, "%s(%s): No samples to pack\n", __func__, msr->sid);
    return -1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "%s(%s): Unknown sample type '%c'\n", __func__, msr->sid, msr->sampletype);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  /* Allocate space for data record */
  rawrec = (char *)libmseed_memory.malloc (msr->reclen);

  if (rawrec == NULL)
  {
    ms_log (2, "%s(%s): Cannot allocate memory\n", __func__, msr->sid);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, rawrec, msr->reclen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "%s(%s): Cannot pack miniSEED version 3 header\n", __func__, msr->sid);
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
    encoded = (char *)libmseed_memory.malloc (maxdatabytes);

    if (encoded == NULL)
    {
      ms_log (2, "%s(%s): Cannot allocate memory\n", __func__, msr->sid);
      libmseed_memory.free (rawrec);
      return -1;
    }
  }

  /* Pack samples into records */
  totalpackedsamples = 0;
  packoffset = 0;
  if (packedsamples)
    *packedsamples = 0;

  while ((msr->numsamples - totalpackedsamples) > maxsamples || flags & MSF_FLUSHDATA)
  {
    packsamples = msr_pack_data (encoded,
                                 (char *)msr->datasamples + packoffset,
                                 (int)(msr->numsamples - totalpackedsamples), maxdatabytes,
                                 msr->sampletype, msr->encoding, swapflag,
                                 &datalength, msr->sid, verbose);

    if (packsamples < 0)
    {
      ms_log (2, "%s(%s): Error packing data samples\n", __func__, msr->sid);
      libmseed_memory.free (encoded);
      libmseed_memory.free (rawrec);
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
      ms_log (1, "%s: Packed %d samples into %d byte record\n", msr->sid, packsamples, reclen);

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
    ms_log (1, "%s: Packed %d total samples\n", msr->sid, totalpackedsamples);

  if (encoded)
    libmseed_memory.free (encoded);

  libmseed_memory.free (rawrec);

  return recordcnt;
} /* End of msr3_pack_mseed3() */

/**********************************************************************/ /**
 * @brief Repack a parsed miniSEED record into a version 3 record.
 *
 * Pack the parsed header into a version 3 header and copy the raw
 * encoded data from the original record.  The original record must be
 * available at the ::MS3Record.record pointer.
 *
 * This can be used to efficiently convert format versions or modify
 * header values without unpacking the data samples.
 *
 * @param[in] msr ::MS3Record containing record to repack
 * @param[out] record Destination buffer for repacked record
 * @param[in] recbuflen Length of destination buffer
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns record length on success and -1 on error.
 ***************************************************************************/
int
msr3_repack_mseed3 (MS3Record *msr, char *record, uint32_t recbuflen,
                    int8_t verbose)
{
  int dataoffset;
  uint32_t origdataoffset;
  uint16_t origdatasize;
  uint32_t crc;
  uint32_t reclen;
  int8_t swapflag;

  if (!msr)
    return -1;

  if (!record)
  {
    ms_log (2, "%s(): record buffer is not set!\n", __func__);
    return -1;
  }

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "%s(%s): Record buffer length (%d) is not large enough for header (%d) and extra (%d)\n",
            __func__, msr->sid, recbuflen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  if (msr->samplecnt > UINT32_MAX)
  {
    ms_log (2, "%s(%s): Too many samples in input record (%" PRId64 " for a single record)\n",
            __func__, msr->sid, msr->samplecnt);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, record, recbuflen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "%s(%s): Cannot pack miniSEED version 3 header\n", __func__, msr->sid);
    return -1;
  }

  /* Determine encoded data size */
  if (msr3_data_bounds (msr, &origdataoffset, &origdatasize))
  {
    ms_log (2, "%s(%s): Cannot determine original data bounds\n", __func__, msr->sid);
    return -1;
  }

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + msr->extralength + origdatasize))
  {
    ms_log (2, "%s(%s): Destination record buffer length (%d) is not large enough for record (%d)\n",
            __func__, msr->sid, recbuflen, (MS3FSDH_LENGTH + msr->extralength + origdatasize));
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
    ms_log (1, "%s: Repacked %d samples into a %d byte record\n",
            msr->sid, msr->samplecnt, reclen);

  return reclen;
} /* End of msr3_repack_mseed3() */

/**********************************************************************/ /**
 * @brief Pack a miniSEED version 3 header into the specified buffer.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when \a msr.reclen and \a msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record to pack
 * @param[out] record Destination for packed header
 * @param[in] recbuflen Length of destination buffer
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the size of the header (fixed and extra) on success, otherwise -1.
 ***************************************************************************/
int
msr3_pack_header3 (MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int extraoffset = 0;
  size_t sidlength;
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
    ms_log (2, "%s(%s): Record length is out of range: %d\n", __func__, msr->sid, msr->reclen);
    return -1;
  }

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "%s(%s): Buffer length (%d) is not large enough for fixed header (%d) and extra (%d)\n",
            __func__, msr->sid, msr->reclen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  if (verbose > 2 && swapflag)
    ms_log (1, "%s: Byte swapping needed for packing of header\n", msr->sid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "%s(%s): Cannot convert starttime: %" PRId64 "\n", __func__, msr->sid, msr->starttime);
    return -1;
  }

  sidlength = strlen (msr->sid);

  /* Ensure that SID length fits in format, which uses data type uint8_t */
  if (sidlength > 255)
  {
    ms_log (2, "%s(%s): Source ID too long: %llu bytes\n", __func__, msr->sid, (unsigned long long)sidlength);
    return -1;
  }

  extraoffset = MS3FSDH_LENGTH + sidlength;

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
  *pMS3FSDH_SIDLENGTH(record) = (uint8_t)sidlength;
  *pMS3FSDH_EXTRALENGTH(record) = HO2u(msr->extralength, swapflag);
  memcpy (pMS3FSDH_SID(record), msr->sid, sidlength);

  if (msr->extralength > 0)
    memcpy (record + extraoffset, msr->extra, msr->extralength);

  return (MS3FSDH_LENGTH + sidlength + msr->extralength);
} /* End of msr3_pack_header3() */

/************************************************************************
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
               uint16_t *byteswritten, char *sid, int8_t verbose)
{
  int nsamples;

  /* Check for encode debugging environment variable */
  if (libmseed_encodedebug < 0)
  {
    if (getenv ("ENCODE_DEBUG"))
      libmseed_encodedebug = 1;
    else
      libmseed_encodedebug = 0;
  }

  if (byteswritten)
    *byteswritten = 0;

  /* Decide if this is a format that we can encode */
  switch (encoding)
  {
  case DE_ASCII:
    if (sampletype != 'a')
    {
      ms_log (2, "%s: Sample type must be ascii (a) for ASCII text encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing ASCII data\n", sid);

    nsamples = msr_encode_text ((char *)src, maxsamples, (char *)dest, maxdatabytes);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples;

    break;

  case DE_INT16:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT16 encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof(int16_t))
    {
      ms_log (2, "%s: Not enough space in record (%d) for INT16 encoding, need at least %d bytes\n",
              sid, maxdatabytes, sizeof(int16_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing INT16 data samples\n", sid);

    nsamples = msr_encode_int16 ((int32_t *)src, maxsamples, (int16_t *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 2;

    break;

  case DE_INT32:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT32 encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof(int32_t))
    {
      ms_log (2, "%s: Not enough space in record (%d) for INT32 encoding, need at least %d bytes\n",
              sid, maxdatabytes, sizeof(int32_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing INT32 data samples\n", sid);

    nsamples = msr_encode_int32 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 4;

    break;

  case DE_FLOAT32:
    if (sampletype != 'f')
    {
      ms_log (2, "%s: Sample type must be float (f) for FLOAT32 encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof(float))
    {
      ms_log (2, "%s: Not enough space in record (%d) for FLOAT32 encoding, need at least %d bytes\n",
              sid, maxdatabytes, sizeof(float));
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing FLOAT32 data samples\n", sid);

    nsamples = msr_encode_float32 ((float *)src, maxsamples, (float *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 4;

    break;

  case DE_FLOAT64:
    if (sampletype != 'd')
    {
      ms_log (2, "%s: Sample type must be double (d) for FLOAT64 encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof(double))
    {
      ms_log (2, "%s: Not enough space in record (%d) for FLOAT64 encoding, need at least %d bytes\n",
              sid, maxdatabytes, sizeof(double));
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing FLOAT64 data samples\n", sid);

    nsamples = msr_encode_float64 ((double *)src, maxsamples, (double *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = nsamples * 8;

    break;

  case DE_STEIM1:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim1 compression not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < 64)
    {
      ms_log (2, "%s: Not enough space in record (%d) for STEIM1 encoding, need at least 64 bytes\n",
              sid, maxdatabytes);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing Steim1 data frames\n", sid);

    /* Always big endian Steim1 */
    swapflag = (ms_bigendianhost()) ? 0 : 1;

    nsamples = msr_encode_steim1 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, 0, byteswritten, swapflag);

    break;

  case DE_STEIM2:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim2 compression not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (maxdatabytes < 64)
    {
      ms_log (2, "%s: Not enough space in record (%d) for STEIM2 encoding, need at least 64 bytes\n",
              sid, maxdatabytes);
      return -1;
    }

    if (verbose > 1)
      ms_log (1, "%s: Packing Steim2 data frames\n", sid);

    /* Always big endian Steim2 */
    swapflag = (ms_bigendianhost()) ? 0 : 1;

    nsamples = msr_encode_steim2 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, 0, byteswritten, sid, swapflag);

    break;

  default:
    ms_log (2, "%s: Unable to pack format %d\n", sid, encoding);

    return -1;
  }

  return nsamples;
} /* End of msr_pack_data() */
