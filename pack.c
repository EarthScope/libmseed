/***************************************************************************
 * Generic routines to pack miniSEED records using an MS3Record as a
 * header template and data source.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2023 Chad Trabant, EarthScope Data Services
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "mseedformat.h"
#include "packdata.h"
#include "extraheaders.h"

/* Internal from another source file */
extern double ms_nomsamprate (int factor, int multiplier);

/* Function(s) internal to this file */
static int msr3_pack_mseed3 (const MS3Record *msr, void (*record_handler) (char *, int, void *),
                             void *handlerdata, int64_t *packedsamples,
                             uint32_t flags, int8_t verbose);

static int msr3_pack_mseed2 (const MS3Record *msr, void (*record_handler) (char *, int, void *),
                             void *handlerdata, int64_t *packedsamples,
                             uint32_t flags, int8_t verbose);

static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          uint16_t *byteswritten, const char *sid, int8_t verbose);

static int ms_genfactmult (double samprate, int16_t *factor, int16_t *multiplier);

static uint32_t ms_timestr2btime (const char *timestr, uint8_t *btime, const char *sid, int8_t swapflag);


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
 *   - ::DE_TEXT (0), Text, expects type \c 't'
 *   - ::DE_INT16 (1), 16-bit integer, expects type \c 'i'
 *   - ::DE_INT32 (3), 32-bit integer, expects type \c 'i'
 *   - ::DE_FLOAT32 (4), 32-bit float (IEEE), expects type \c 'f'
 *   - ::DE_FLOAT64 (5), 64-bit float (IEEE), expects type \c 'd'
 *   - ::DE_STEIM1 (10), Stiem-1 compressed integers, expects type \c 'i'
 *   - ::DE_STEIM2 (11), Stiem-2 compressed integers, expects type \c 'i'
 *
 * If \a flags has ::MSF_FLUSHDATA set, all of the data will be packed
 * into data records even though the last one will probably be smaller
 * than requested or, in the case of miniSEED 2, unfilled.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when \a msr.reclen and \a msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the \a record_handler()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Bit flags used to control the packing process:
 * @parblock
 *  - \c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - \c ::MSF_PACKVER2 : Pack miniSEED version 2 regardless of ::MS3Record.formatversion
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the number of records created on success and -1 on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack (const MS3Record *msr, void (*record_handler) (char *, int, void *),
           void *handlerdata, int64_t *packedsamples, uint32_t flags, int8_t verbose)
{
  int packedrecs = 0;

  if (!msr)
  {
    ms_log (2, "Required argument not defined: 'msr'\n");
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  if ((msr->reclen != -1) && (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN))
  {
    ms_log (2, "%s: Record length is out of range: %d\n", msr->sid, msr->reclen);
    return -1;
  }

  /* Pack version 2 if requested */
  if (msr->formatversion == 2 || flags & MSF_PACKVER2)
  {
    packedrecs = msr3_pack_mseed2 (msr, record_handler, handlerdata, packedsamples,
                                   flags, verbose);
  }
  /* Pack version 3 otherwise */
  else
  {
    packedrecs = msr3_pack_mseed3 (msr, record_handler, handlerdata, packedsamples,
                                   flags, verbose);
  }

  return packedrecs;
} /* End of msr3_pack() */

/***************************************************************************
 * msr3_pack_mseed3:
 *
 * Pack data into miniSEED version 3 record(s).
 *
 * Returns the number of records created on success and -1 on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_mseed3 (const MS3Record *msr, void (*record_handler) (char *, int, void *),
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
  int32_t maxreclen;
  int8_t encoding;

  uint32_t crc;
  uint16_t datalength;
  nstime_t nextstarttime;
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr)
  {
    ms_log (2, "Required argument not defined: 'msr'\n");
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  /* Use default record length and encoding if needed */
  maxreclen = (msr->reclen == -1) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding == -1) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (maxreclen < (MS3FSDH_LENGTH + strlen(msr->sid) + msr->extralength))
  {
    ms_log (2, "%s: Record length (%d) is not large enough for header (%d), SID (%"PRIsize_t"), and extra (%d)\n",
            msr->sid, maxreclen, MS3FSDH_LENGTH, strlen(msr->sid), msr->extralength);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  /* Allocate space for data record */
  rawrec = (char *)libmseed_memory.malloc (maxreclen);

  if (rawrec == NULL)
  {
    ms_log (2, "%s: Cannot allocate memory\n", msr->sid);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, rawrec, maxreclen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED version 3 header\n", msr->sid);
    return -1;
  }

  /* Short cut: if there are no samples, record packing is complete */
  if (msr->numsamples <= 0)
  {
    /* Set encoding to text for consistency and to reduce expectations */
    *pMS3FSDH_ENCODING(rawrec) = DE_TEXT;

    /* Calculate CRC (with CRC field set to 0) and set */
    memset (pMS3FSDH_CRC(rawrec), 0, sizeof(uint32_t));
    crc = ms_crc32c ((const uint8_t*)rawrec, dataoffset, 0);
    *pMS3FSDH_CRC(rawrec) = HO4u (crc, swapflag);

    if (verbose >= 1)
      ms_log (0, "%s: Packed %d byte record with no payload\n", msr->sid, dataoffset);

    /* Send record to handler */
    record_handler (rawrec, dataoffset, handlerdata);

    libmseed_memory.free (rawrec);

    if (packedsamples)
      *packedsamples = 0;

    return 1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "%s: Unknown sample type '%c'\n", msr->sid, msr->sampletype);
    return -1;
  }

  /* Determine the max data bytes and sample count */
  maxdatabytes = maxreclen - dataoffset;

  if (encoding == DE_STEIM1)
  {
    maxsamples = (int)(maxdatabytes / 64) * STEIM1_FRAME_MAX_SAMPLES;
  }
  else if (encoding == DE_STEIM2)
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
      ms_log (2, "%s: Cannot allocate memory\n", msr->sid);
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
                                 msr->sampletype, encoding, swapflag,
                                 &datalength, msr->sid, verbose);

    if (packsamples < 0)
    {
      ms_log (2, "%s: Error packing data samples\n", msr->sid);
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
      ms_log (0, "%s: Packed %d samples into %d byte record\n", msr->sid, packsamples, reclen);

    /* Send record to handler */
    record_handler (rawrec, reclen, handlerdata);

    totalpackedsamples += packsamples;
    if (packedsamples)
      *packedsamples = totalpackedsamples;

    recordcnt++;

    if (totalpackedsamples >= msr->numsamples)
      break;

    /* Update record start time for next record */
    nextstarttime = ms_sampletime (msr->starttime, totalpackedsamples, msr->samprate);

    if (ms_nstime2time (nextstarttime, &year, &day, &hour, &min, &sec, &nsec))
    {
      ms_log (2, "%s: Cannot convert next record starttime: %" PRId64 "\n", msr->sid, nextstarttime);
      libmseed_memory.free (rawrec);
      return -1;
    }

    *pMS3FSDH_NSEC (rawrec) = HO4u (nsec, swapflag);
    *pMS3FSDH_YEAR (rawrec) = HO2u (year, swapflag);
    *pMS3FSDH_DAY (rawrec) = HO2u (day, swapflag);
    *pMS3FSDH_HOUR (rawrec) = hour;
    *pMS3FSDH_MIN (rawrec) = min;
    *pMS3FSDH_SEC (rawrec) = sec;
  }

  if (verbose >= 2)
    ms_log (0, "%s: Packed %" PRId64 " total samples\n", msr->sid, totalpackedsamples);

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
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_repack_mseed3 (const MS3Record *msr, char *record, uint32_t recbuflen,
                    int8_t verbose)
{
  int dataoffset;
  uint32_t origdataoffset;
  uint32_t origdatasize;
  uint32_t crc;
  uint32_t reclen;
  int8_t swapflag;

  if (!msr || ! record)
  {
    ms_log (2, "Required argument not defined: 'msr' or 'record'\n");
    return -1;
  }

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + msr->extralength))
  {
    ms_log (2, "%s: Record buffer length (%u) is not large enough for header (%d) and extra (%d)\n",
            msr->sid, recbuflen, MS3FSDH_LENGTH, msr->extralength);
    return -1;
  }

  if (msr->samplecnt > UINT32_MAX)
  {
    ms_log (2, "%s: Too many samples in input record (%" PRId64 " for a single record)\n",
            msr->sid, msr->samplecnt);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  dataoffset = msr3_pack_header3 (msr, record, recbuflen, verbose);

  if (dataoffset < 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED version 3 header\n", msr->sid);
    return -1;
  }

  /* Determine encoded data size */
  if (msr3_data_bounds (msr, &origdataoffset, &origdatasize))
  {
    ms_log (2, "%s: Cannot determine original data bounds\n", msr->sid);
    return -1;
  }

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + msr->extralength + origdatasize))
  {
    ms_log (2, "%s: Destination record buffer length (%u) is not large enough for record (%d)\n",
            msr->sid, recbuflen, (MS3FSDH_LENGTH + msr->extralength + origdatasize));
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
    ms_log (0, "%s: Repacked %" PRId64 " samples into a %u byte record\n",
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
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_header3 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int extraoffset = 0;
  size_t sidlength;
  int32_t maxreclen;
  int8_t encoding;
  int8_t swapflag;

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr || !record)
  {
    ms_log (2, "Required argument not defined: 'msr' or 'record'\n");
    return -1;
  }

  /* Use default record length and encoding if needed */
  maxreclen = (msr->reclen == -1) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding == -1) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (maxreclen < MINRECLEN || maxreclen > MAXRECLEN)
  {
    ms_log (2, "%s: Record length is out of range: %d\n", msr->sid, maxreclen);
    return -1;
  }

  sidlength = strlen (msr->sid);

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + sidlength + msr->extralength))
  {
    ms_log (2, "%s: Buffer length (%d) is not large enough for fixed header (%d), SID (%"PRIsize_t"), and extra (%d)\n",
            msr->sid, maxreclen, MS3FSDH_LENGTH, sidlength, msr->extralength);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  if (verbose > 2 && swapflag)
    ms_log (0, "%s: Byte swapping needed for packing of header\n", msr->sid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "%s: Cannot convert starttime: %" PRId64 "\n", msr->sid, msr->starttime);
    return -1;
  }

  /* Ensure that SID length fits in format, which uses data type uint8_t */
  if (sidlength > 255)
  {
    ms_log (2, "%s: Source ID too long: %"PRIsize_t" bytes\n", msr->sid, sidlength);
    return -1;
  }

  extraoffset = MS3FSDH_LENGTH + sidlength;

  /* Build fixed header */
  record[0] = 'M';
  record[1] = 'S';
  *pMS3FSDH_FORMATVERSION (record) = 3;
  *pMS3FSDH_FLAGS (record) = msr->flags;
  *pMS3FSDH_NSEC (record) = HO4u (nsec, swapflag);
  *pMS3FSDH_YEAR (record) = HO2u (year, swapflag);
  *pMS3FSDH_DAY (record) = HO2u (day, swapflag);
  *pMS3FSDH_HOUR (record) = hour;
  *pMS3FSDH_MIN (record) = min;
  *pMS3FSDH_SEC (record) = sec;
  *pMS3FSDH_ENCODING (record) = encoding;

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

/***************************************************************************
 * msr3_pack_mseed2:
 *
 * Pack data into miniSEED version 2 record(s).
 *
 * Returns the number of records created on success and -1 on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_mseed2 (const MS3Record *msr, void (*record_handler) (char *, int, void *),
                  void *handlerdata, int64_t *packedsamples,
                  uint32_t flags, int8_t verbose)
{
  char *rawrec = NULL;
  char *encoded = NULL;  /* Separate encoded data buffer for alignment */
  int8_t swapflag;
  int32_t reclen;
  int8_t encoding;
  int dataoffset = 0;
  int headerlen;
  int content;

  int samplesize;
  int maxdatabytes;
  int maxsamples;
  int recordcnt = 0;
  int packsamples;
  int packoffset;
  int64_t totalpackedsamples;

  uint16_t datalength;
  nstime_t nextstarttime;
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr)
  {
    ms_log (2, "Required argument not defined: 'msr'\n");
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  /* Use default record length and encoding if needed */
  reclen = (msr->reclen == -1) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding == -1) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (reclen < 128)
  {
    ms_log (2, "%s: Record length (%d) is not large enough, must be >= 128 bytes\n",
            msr->sid, reclen);
    return -1;
  }

  /* Check that record length is a power of 2.
   * Power of two if (X & (X - 1)) == 0 */
  if ((reclen & (reclen - 1)) != 0)
  {
    ms_log (2, "%s: Cannot create miniSEED 2, record length (%d) is not a power of 2\n",
            msr->sid, reclen);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  /* Allocate space for data record */
  rawrec = (char *)libmseed_memory.malloc (reclen);

  if (rawrec == NULL)
  {
    ms_log (2, "%s: Cannot allocate memory\n", msr->sid);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  headerlen = msr3_pack_header2 (msr, rawrec, reclen, verbose);

  if (headerlen < 0)
    return -1;

  /* Short cut: if there are no samples, record packing is complete */
  if (msr->numsamples <= 0)
  {
    /* Set encoding to text for consistency and to reduce expectations */
    *pMS2B1000_ENCODING (rawrec + 48) = DE_TEXT;

    /* Set empty part of record to zeros */
    memset (rawrec + headerlen, 0, reclen - headerlen);

    if (verbose >= 1)
      ms_log (0, "%s: Packed %d byte record with no payload\n", msr->sid, reclen);

    /* Send record to handler */
    record_handler (rawrec, reclen, handlerdata);

    libmseed_memory.free (rawrec);

    if (packedsamples)
      *packedsamples = 0;

    return 1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "%s: Unknown sample type '%c'\n", msr->sid, msr->sampletype);
    return -1;
  }

  /* Determine offset to encoded data */
  if (encoding == DE_STEIM1 || encoding == DE_STEIM2)
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

  /* Set data offset in header */
  *pMS2FSDH_DATAOFFSET(rawrec) = HO2u (dataoffset, swapflag);

  /* Determine the max data bytes and sample count */
  maxdatabytes = reclen - dataoffset;

  if (encoding == DE_STEIM1)
  {
    maxsamples = (int)(maxdatabytes / 64) * STEIM1_FRAME_MAX_SAMPLES;
  }
  else if (encoding == DE_STEIM2)
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
      ms_log (2, "%s: Cannot allocate memory\n", msr->sid);
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
                                 msr->sampletype, encoding, swapflag,
                                 &datalength, msr->sid, verbose);

    if (packsamples < 0)
    {
      ms_log (2, "%s: Error packing data samples\n", msr->sid);
      libmseed_memory.free (encoded);
      libmseed_memory.free (rawrec);
      return -1;
    }

    packoffset += packsamples * samplesize;

    /* Copy encoded data into record */
    memcpy (rawrec + dataoffset, encoded, datalength);

    /* Update number of samples */
    *pMS2FSDH_NUMSAMPLES(rawrec) = HO2u (packsamples, swapflag);

    /* Zero any space between encoded data and end of record */
    content = dataoffset + datalength;
    if (content < reclen)
      memset (rawrec + content, 0, reclen - content);

    if (verbose >= 1)
      ms_log (0, "%s: Packed %d samples into %d byte record\n", msr->sid, packsamples, reclen);

    /* Send record to handler */
    record_handler (rawrec, reclen, handlerdata);

    totalpackedsamples += packsamples;
    if (packedsamples)
      *packedsamples = totalpackedsamples;

    recordcnt++;

    if (totalpackedsamples >= msr->numsamples)
      break;

    /* Update record start time for next record */
    nextstarttime = ms_sampletime (msr->starttime, totalpackedsamples, msr->samprate);

    if (ms_nstime2time (nextstarttime, &year, &day, &hour, &min, &sec, &nsec))
    {
      ms_log (2, "%s: Cannot convert next record starttime: %" PRId64 "\n",
              msr->sid, nextstarttime);
      libmseed_memory.free (rawrec);
      return -1;
    }

    *pMS2FSDH_YEAR (rawrec) = HO2u (year, swapflag);
    *pMS2FSDH_DAY (rawrec)  = HO2u (day, swapflag);
    *pMS2FSDH_HOUR (rawrec) = hour;
    *pMS2FSDH_MIN (rawrec)  = min;
    *pMS2FSDH_SEC (rawrec)  = sec;
    *pMS2FSDH_FSEC (rawrec) = HO2u ((nsec / 100000), swapflag);
  }

  if (verbose >= 2)
    ms_log (0, "%s: Packed %" PRId64 " total samples\n", msr->sid, totalpackedsamples);

  if (encoded)
    libmseed_memory.free (encoded);

  libmseed_memory.free (rawrec);

  return recordcnt;
} /* End of msr3_pack_mseed2() */

/**********************************************************************/ /**
 * @brief Pack a miniSEED version 2 header into the specified buffer.
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
 * @returns the size of the header (fixed and blockettes) on success, otherwise -1.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_header2 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int written = 0;
  int8_t swapflag;
  int32_t reclen;
  int8_t encoding;

  char network[64];
  char station[64];
  char location[64];
  char channel[64];

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;
  uint16_t fsec;
  int8_t msec_offset;

  int reclenexp = 0;
  int reclenfind;
  int16_t factor;
  int16_t multiplier;
  uint16_t *next_blockette = NULL;

  yyjson_doc *ehdoc    = NULL;
  yyjson_val *ehroot   = NULL;
  yyjson_read_flag flg = YYJSON_READ_NOFLAG;
  yyjson_alc alc       = {_priv_malloc, _priv_realloc, _priv_free, NULL};
  yyjson_read_err err;
  yyjson_val *eharr;
  yyjson_arr_iter ehiter;
  yyjson_val *ehiterval;
  yyjson_val *ehval;
  const char *header_string;
  double header_number;
  bool header_boolean;

  int blockette_type;
  int blockette_length;

  if (!msr || !record)
  {
    ms_log (2, "Required argument not defined: 'msr' or 'record'\n");
    return -1;
  }

  /* Use default record length and encoding if needed */
  reclen = (msr->reclen == -1) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding == -1) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (reclen < 128 || reclen > MAXRECLEN)
  {
    ms_log (2, "%s: Record length is out of range: %d\n", msr->sid, reclen);
    return -1;
  }

  /* Check that record length is a power of 2.
   * Power of two if (X & (X - 1)) == 0 */
  if ((reclen & (reclen - 1)) != 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED 2, record length (%d) is not a power of 2\n",
            msr->sid, reclen);
    return -1;
  }

  /* Calculate the record length as an exponent of 2 */
  for (reclenfind = 1, reclenexp = 1; reclenfind <= MAXRECLEN; reclenexp++)
  {
    reclenfind *= 2;
    if (reclenfind == reclen)
      break;
  }

  /* Parse identifier codes from full identifier */
  if (ms_sid2nslc (msr->sid, network, station, location, channel))
  {
    ms_log (2, "%s: Cannot parse SEED identifier codes from full identifier\n", msr->sid);
    return -1;
  }

  /* Verify that identifier codes will fit into and are appropriate for miniSEED 2 */
  if (strlen (network) > 2 || strlen (station) > 5 || strlen (location) > 2 || strlen (channel) != 3)
  {
    ms_log (2, "%s: Cannot create miniSEED 2 for N,S,L,C codes: %s, %s, %s, %s\n",
            msr->sid, network, station, location, channel);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  if (verbose > 2 && swapflag)
    ms_log (0, "%s: Byte swapping needed for packing of header\n", msr->sid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "%s: Cannot convert starttime: %" PRId64 "\n", msr->sid, msr->starttime);
    return -1;
  }

  /* Calculate time at fractional 100usec resolution and microsecond offset */
  fsec = nsec / 100000;
  msec_offset = ((nsec / 1000) - (fsec * 100));

  /* Generate factor & multipler representation of sample rate */
  if (ms_genfactmult (msr3_sampratehz(msr), &factor, &multiplier))
  {
    ms_log (2, "%s: Cannot convert sample rate (%g) to factor and multiplier\n", msr->sid, msr->samprate);
    return -1;
  }

  /* Parse extra headers if present */
  if (msr->extra && msr->extralength > 0)
  {
    ehdoc = yyjson_read_opts (msr->extra, msr->extralength, flg, &alc, &err);

    if (!ehdoc)
    {
      ms_log (2, "%s() Cannot parse extra header JSON: %s\n",
              __func__, (err.msg) ? err.msg : "Unknown error");
      return -1;
    }

    ehroot = yyjson_doc_get_root (ehdoc);
  }

  /* Build fixed header */
  memcpy (pMS2FSDH_SEQNUM (record), "000000", 6);

  if (yyjson_ptr_get_str (ehroot, "/FDSN/DataQuality", &header_string) &&
      MS2_ISDATAINDICATOR (header_string[0]))
    *pMS2FSDH_DATAQUALITY (record) = header_string[0];
  else
    *pMS2FSDH_DATAQUALITY (record) = 'D';

  *pMS2FSDH_RESERVED (record) = ' ';
  ms_strncpopen (pMS2FSDH_STATION (record), station, 5);
  ms_strncpopen (pMS2FSDH_LOCATION (record), location, 2);
  ms_strncpopen (pMS2FSDH_CHANNEL (record), channel, 3);
  ms_strncpopen (pMS2FSDH_NETWORK (record), network, 2);

  *pMS2FSDH_YEAR (record)       = HO2u (year, swapflag);
  *pMS2FSDH_DAY (record)        = HO2u (day, swapflag);
  *pMS2FSDH_HOUR (record)       = hour;
  *pMS2FSDH_MIN (record)        = min;
  *pMS2FSDH_SEC (record)        = sec;
  *pMS2FSDH_UNUSED (record)     = 0;
  *pMS2FSDH_FSEC (record)       = HO2u (fsec, swapflag);
  *pMS2FSDH_NUMSAMPLES (record) = 0;

  *pMS2FSDH_SAMPLERATEFACT (record) = HO2d (factor, swapflag);
  *pMS2FSDH_SAMPLERATEMULT (record) = HO2d (multiplier, swapflag);

  /* Map activity bit flags */
  *pMS2FSDH_ACTFLAGS (record) = 0;
  if (msr->flags & 0x01) /* Bit 0 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x01;

  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/Begin", &header_boolean) && header_boolean) /* Bit 2 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/End", &header_boolean) && header_boolean) /* Bit 3 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x08;

  if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/LeapSecond", &header_number))
  {
    if (header_number > 0) /* Bit 4 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x10;
    else if (header_number < 0) /* Bit 5 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x20;
  }

  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/InProgress", &header_boolean) && header_boolean) /* Bit 6 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x40;

  /* Map I/O and clock bit flags */
  *pMS2FSDH_IOFLAGS (record) = 0;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/StationVolumeParityError", &header_boolean) && header_boolean) /* Bit 0 */
    *pMS2FSDH_IOFLAGS (record) |= 0x01;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/LongRecordRead", &header_boolean) && header_boolean) /* Bit 1 */
    *pMS2FSDH_IOFLAGS (record) |= 0x02;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/ShortRecordRead", &header_boolean) && header_boolean) /* Bit 2 */
    *pMS2FSDH_IOFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/StartOfTimeSeries", &header_boolean) && header_boolean) /* Bit 3 */
    *pMS2FSDH_IOFLAGS (record) |= 0x08;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/EndOfTimeSeries", &header_boolean) && header_boolean) /* Bit 4 */
    *pMS2FSDH_IOFLAGS (record) |= 0x10;
  if (msr->flags & 0x04) /* Bit 5 */
    *pMS2FSDH_IOFLAGS (record) |= 0x20;

  /* Map data quality bit flags */
  *pMS2FSDH_DQFLAGS (record) = 0;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/AmplifierSaturation", &header_boolean) && header_boolean) /* Bit 0 */
    *pMS2FSDH_DQFLAGS (record) |= 0x01;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/DigitizerClipping", &header_boolean) && header_boolean) /* Bit 1 */
    *pMS2FSDH_DQFLAGS (record) |= 0x02;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/Spikes", &header_boolean) && header_boolean) /* Bit 2 */
    *pMS2FSDH_DQFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/Glitches", &header_boolean) && header_boolean) /* Bit 3 */
    *pMS2FSDH_DQFLAGS (record) |= 0x08;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/MissingData", &header_boolean) && header_boolean) /* Bit 4 */
    *pMS2FSDH_DQFLAGS (record) |= 0x10;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/TelemetrySyncError", &header_boolean) && header_boolean) /* Bit 5 */
    *pMS2FSDH_DQFLAGS (record) |= 0x20;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/FilterCharging", &header_boolean) && header_boolean) /* Bit 6 */
    *pMS2FSDH_DQFLAGS (record) |= 0x40;
  if (msr->flags & 0x02) /* Bit 7 */
    *pMS2FSDH_DQFLAGS (record) |= 0x80;

  if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/Correction", &header_number))
  {
    *pMS2FSDH_TIMECORRECT (record) = HO4d (header_number * 10000, swapflag);

    /* Set time correction applied bit in activity flags.
       Rationale: V3 records do not allow unapplied time corrections and unapplied
       time corrections in V2 records are always applied on read by this library. */
    *pMS2FSDH_ACTFLAGS (record) |= 0x02;
  }
  else
  {
    *pMS2FSDH_TIMECORRECT (record) = 0;
  }

  *pMS2FSDH_NUMBLOCKETTES (record)   = 1;
  *pMS2FSDH_DATAOFFSET (record)      = 0;
  *pMS2FSDH_BLOCKETTEOFFSET (record) = HO2u (48, swapflag);

  written = 48;

  /* Add mandatory Blockette 1000 */
  next_blockette = pMS2B1000_NEXT (record + written);

  *pMS2B1000_TYPE (record + written)      = HO2u (1000, swapflag);
  *pMS2B1000_NEXT (record + written)      = 0;
  *pMS2B1000_ENCODING (record + written)  = encoding;
  *pMS2B1000_BYTEORDER (record + written) = 1;
  *pMS2B1000_RECLEN (record + written)    = reclenexp;
  *pMS2B1000_RESERVED (record + written)  = 0;

  written += 8;

  /* Add Blockette 1001 if microsecond offset or timing quality is present */
  if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/Quality", &header_number) || msec_offset)
  {
    *next_blockette = HO2u ((uint16_t)written, swapflag);
    next_blockette = pMS2B1001_NEXT (record + written);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;

    *pMS2B1001_TYPE (record + written) = HO2u (1001, swapflag);
    *pMS2B1001_NEXT (record + written) = 0;

    if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/Quality", &header_number))
      *pMS2B1001_TIMINGQUALITY (record + written) = (uint8_t) (header_number + 0.5);
    else
      *pMS2B1001_TIMINGQUALITY (record + written) = 0;

    *pMS2B1001_MICROSECOND (record + written) = msec_offset;
    *pMS2B1001_RESERVED (record + written)    = 0;
    *pMS2B1001_FRAMECOUNT (record + written)  = 0;

    written += 8;
  }

  /* Add Blockette 100 if sample rate is not well represented by factor/multiplier */
  if (ms_dabs(msr3_sampratehz(msr) - ms_nomsamprate(factor, multiplier)) > 0.0001)
  {
    *next_blockette = HO2u ((uint16_t)written, swapflag);
    next_blockette = pMS2B100_NEXT (record + written);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;

    *pMS2B100_TYPE (record + written)     = HO2u (100, swapflag);
    *pMS2B100_NEXT (record + written)     = 0;
    *pMS2B100_SAMPRATE (record + written) = HO4f (msr->samprate, swapflag);
    *pMS2B100_FLAGS (record + written)    = 0;
    memset (pMS2B100_RESERVED (record + written), 0, 3);

    written += 12;
  }

  /* Add Blockette 500 for timing execeptions */
  if ((eharr = yyjson_ptr_get (ehroot, "/FDSN/Time/Exception")) && yyjson_is_arr (eharr))
  {
    yyjson_arr_iter_init (eharr, &ehiter);

    while ((ehiterval = yyjson_arr_iter_next (&ehiter)))
    {
      if (!yyjson_is_obj (ehiterval))
        continue;

      blockette_length = 200;

      if ((recbuflen - written) < blockette_length)
      {
        ms_log (2, "%s: Record length not large enough for B500\n", msr->sid);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      *next_blockette = HO2u ((uint16_t)written, swapflag);
      next_blockette  = pMS2B500_NEXT (record + written);
      *pMS2FSDH_NUMBLOCKETTES (record) += 1;

      memset (record + written, 0, blockette_length);
      *pMS2B500_TYPE (record + written) = HO2u (500, swapflag);
      *pMS2B500_NEXT (record + written) = 0;

      if ((ehval = yyjson_ptr_get (ehiterval, "/VCOCorrection")) && yyjson_is_num (ehval))
        *pMS2B500_VCOCORRECTION (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

      if ((ehval = yyjson_ptr_get (ehiterval, "/Time")) && yyjson_is_str (ehval))
      {
        uint32_t l_nsec;
        uint16_t l_fsec;
        int8_t l_msec_offset;

        l_nsec = ms_timestr2btime (yyjson_get_str (ehval),
                                   (uint8_t *)pMS2B500_YEAR (record + written),
                                   msr->sid, swapflag);

        if (l_nsec == -1)
        {
          ms_log (2, "%s: Cannot convert B500 time: %s\n", msr->sid, yyjson_get_str (ehval));
          yyjson_doc_free (ehdoc);
          return -1;
        }

        /* Calculate time at fractional 100usec resolution and microsecond offset */
        l_fsec        = l_nsec / 100000;
        l_msec_offset = ((l_nsec / 1000) - (l_fsec * 100));

        *pMS2B500_MICROSECOND (record + written) = l_msec_offset;
      }

      if ((ehval = yyjson_ptr_get (ehiterval, "/ReceptionQuality")) && yyjson_is_num (ehval))
        *pMS2B500_RECEPTIONQUALITY (record + written) = (uint8_t)yyjson_get_num (ehval);

      if ((ehval = yyjson_ptr_get (ehiterval, "/Count")) && yyjson_is_num (ehval))
        *pMS2B500_EXCEPTIONCOUNT (record + written) = HO4d (yyjson_get_num (ehval), swapflag);

      if ((ehval = yyjson_ptr_get (ehiterval, "/Type")) && yyjson_is_str (ehval))
        ms_strncpopen (pMS2B500_EXCEPTIONTYPE (record + written), yyjson_get_str (ehval), 16);

      if ((ehval = yyjson_ptr_get (ehroot, "/FDSN/Clock/Model")) && yyjson_is_str (ehval))
        ms_strncpopen (pMS2B500_CLOCKMODEL (record + written), yyjson_get_str (ehval), 32);

      if ((ehval = yyjson_ptr_get (ehiterval, "/ClockStatus")) && yyjson_is_str (ehval))
        ms_strncpopen (pMS2B500_CLOCKSTATUS (record + written), yyjson_get_str (ehval), 128);

      written += blockette_length;
    }
  } /* End if /FDSN/Time/Exception */

  /* Add Blockette 200,201 for event detections */
  if ((eharr = yyjson_ptr_get (ehroot, "/FDSN/Event/Detection")) && yyjson_is_arr (eharr))
  {
    yyjson_arr_iter_init (eharr, &ehiter);

    while ((ehiterval = yyjson_arr_iter_next (&ehiter)))
    {
      if (!yyjson_is_obj (ehiterval))
        continue;

      /* Determine which detection type: MURDOCK versus the generic type */
      if ((ehval = yyjson_ptr_get (ehiterval, "/Type")) && yyjson_is_str (ehval) &&
          strncasecmp (yyjson_get_str (ehval), "MURDOCK", 7) == 0)
      {
        blockette_type = 201;
        blockette_length = 60;
      }
      else
      {
        blockette_type = 200;
        blockette_length = 52;
      }

      if ((recbuflen - written) < blockette_length )
      {
        ms_log (2, "%s: Record length not large enough for B%d\n", msr->sid, blockette_type);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      /* The initial fields of B200 and B201 are the same */
      *next_blockette = HO2u ((uint16_t)written, swapflag);
      next_blockette = pMS2B200_NEXT (record + written);
      *pMS2FSDH_NUMBLOCKETTES (record) += 1;

      memset (record + written, 0, blockette_length);
      *pMS2B200_TYPE (record + written) = HO2u (blockette_type, swapflag);
      *pMS2B200_NEXT (record + written) = 0;

      if ((ehval = yyjson_ptr_get (ehiterval, "/SignalAmplitude")) && yyjson_is_num (ehval))
        *pMS2B200_AMPLITUDE (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

      if ((ehval = yyjson_ptr_get (ehiterval, "/SignalPeriod")) && yyjson_is_num (ehval))
        *pMS2B200_PERIOD (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

      if ((ehval = yyjson_ptr_get (ehiterval, "/BackgroundEstimate")) && yyjson_is_num (ehval))
        *pMS2B200_BACKGROUNDEST (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

      /* Determine which wave: DILATATION versus (assumed) COMPRESSION */
      if ((ehval = yyjson_ptr_get (ehiterval, "/Wave")))
      {
        if (yyjson_is_str (ehval) && strncasecmp (yyjson_get_str (ehval), "DILATATION", 10) == 0)
          *pMS2B200_FLAGS (record + written) |= 0x01;
      }
      else if (blockette_type == 200)
      {
        *pMS2B200_FLAGS (record + written) |= 0x04;
      }

      if (blockette_type == 200 &&
          (ehval = yyjson_ptr_get (ehiterval, "/Units")) && yyjson_is_str (ehval) &&
          strncasecmp (yyjson_get_str (ehval), "COUNT", 5) != 0)
        *pMS2B200_FLAGS (record + written) |= 0x02;

      if ((ehval = yyjson_ptr_get (ehiterval, "/OnsetTime")) && yyjson_is_str (ehval))
      {
        if (ms_timestr2btime (yyjson_get_str (ehval), (uint8_t *)pMS2B200_YEAR (record + written),
                              msr->sid, swapflag) == -1)
        {
          ms_log (2, "%s: Cannot convert B%d time: %s\n", msr->sid, blockette_type, yyjson_get_str (ehval));
          yyjson_doc_free (ehdoc);
          return -1;
        }
      }

      if (blockette_type == 200)
      {
        if ((ehval = yyjson_ptr_get (ehiterval, "/Detector")) && yyjson_is_str (ehval))
          ms_strncpopen (pMS2B200_DETECTOR (record + written), yyjson_get_str (ehval), 24);
      }
      else /* Blockette 201 */
      {
        yyjson_val *ehsubarr;
        yyjson_arr_iter ehsubiter;
        yyjson_val *ehsubiterval;
        int idx = 0;

        if ((ehsubarr = yyjson_ptr_get (ehiterval, "/MEDSNR")) && yyjson_is_arr (ehsubarr))
        {
          yyjson_arr_iter_init (ehsubarr, &ehsubiter);

          while ((ehsubiterval = yyjson_arr_iter_next (&ehsubiter)))
          {
            if (!yyjson_is_num (ehsubiterval))
              continue;

            pMS2B201_MEDSNR (record + written)[idx++] = (uint8_t)yyjson_get_num (ehsubiterval);
          }
        }

        if ((ehval = yyjson_ptr_get (ehiterval, "/MEDLookback")) && yyjson_is_num (ehval))
          *pMS2B201_LOOPBACK (record + written) = (uint8_t)yyjson_get_num (ehval);

        if ((ehval = yyjson_ptr_get (ehiterval, "/MEDPickAlgorithm")) && yyjson_is_num (ehval))
          *pMS2B201_PICKALGORITHM (record + written) = (uint8_t)yyjson_get_num (ehval);

        if ((ehval = yyjson_ptr_get (ehiterval, "/Detector")) && yyjson_is_str (ehval))
          ms_strncpopen (pMS2B201_DETECTOR (record + written), yyjson_get_str (ehval), 24);
      }

      written += blockette_length;
    }
  } /* End if /FDSN/Event/Detection */

  /* Add Blockette B300, 310, 320, 390, 395 for calibrations */

  if ((eharr = yyjson_ptr_get (ehroot, "/FDSN/Calibration/Sequence")) && yyjson_is_arr (eharr))
  {
    yyjson_arr_iter_init (eharr, &ehiter);

    while ((ehiterval = yyjson_arr_iter_next (&ehiter)))
    {
      if (!yyjson_is_obj (ehiterval))
        continue;

      /* Determine which calibration type: STEP, SINE, PSEUDORANDOM, GENERIC */
      blockette_type   = 0;
      blockette_length = 0;
      if ((ehval = yyjson_ptr_get (ehiterval, "/Type")) && yyjson_is_str (ehval))
      {
        if (strncasecmp (yyjson_get_str (ehval), "STEP", 4) == 0)
        {
          blockette_type   = 300;
          blockette_length = 60;
        }
        else if (strncasecmp (yyjson_get_str (ehval), "SINE", 4) == 0)
        {
          blockette_type   = 310;
          blockette_length = 60;
        }
        else if (strncasecmp (yyjson_get_str (ehval), "PSEUDORANDOM", 12) == 0)
        {
          blockette_type   = 320;
          blockette_length = 64;
        }
        else if (strncasecmp (yyjson_get_str (ehval), "GENERIC", 7) == 0)
        {
          blockette_type   = 390;
          blockette_length = 28;
        }
      }
      else if ((ehval = yyjson_ptr_get (ehiterval, "/EndTime")))
      {
        blockette_type = 395;
        blockette_length = 16;
      }

      if (!blockette_type || !blockette_length)
      {
        ms_log (2, "%s: Unknown or unset /FDSN/Calibration/Sequence/Type or /EndTime\n", msr->sid);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      if ((recbuflen - written) < blockette_length )
      {
        ms_log (2, "%s: Record length not large enough for B%d\n", msr->sid, blockette_type);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      if (blockette_type == 300 || blockette_type == 310 ||
          blockette_type == 320 || blockette_type == 390)
      {
        /* The initial fields of B300, 310, 320, 390 are the same */
        *next_blockette = HO2u ((uint16_t)written, swapflag);
        next_blockette  = pMS2B300_NEXT (record + written);
        *pMS2FSDH_NUMBLOCKETTES (record) += 1;

        memset (record + written, 0, blockette_length);
        *pMS2B300_TYPE (record + written) = HO2u (blockette_type, swapflag);
        *pMS2B300_NEXT (record + written) = 0;

        if ((ehval = yyjson_ptr_get (ehiterval, "/BeginTime")) && yyjson_is_str (ehval))
        {
          if (ms_timestr2btime (yyjson_get_str (ehval), (uint8_t *)pMS2B300_YEAR (record + written),
                                msr->sid, swapflag) == -1)
          {
            ms_log (2, "%s: Cannot convert B%d time: %s\n", msr->sid, blockette_type, yyjson_get_str (ehval));
            yyjson_doc_free (ehdoc);
            return -1;
          }
        }

        if (blockette_type == 300)
        {
          if ((ehval = yyjson_ptr_get (ehiterval, "/Steps")) && yyjson_is_num (ehval))
            *pMS2B300_NUMCALIBRATIONS (record + written) = (uint8_t)yyjson_get_num (ehval);

          if ((ehval = yyjson_ptr_get (ehiterval, "/StepFirstPulsePositive")) && yyjson_get_bool (ehval))
            *pMS2B300_FLAGS (record + written) |= 0x01;

          if ((ehval = yyjson_ptr_get (ehiterval, "/StepAlternateSign")) && yyjson_get_bool (ehval))
            *pMS2B300_FLAGS (record + written) |= 0x02;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Trigger")) && yyjson_is_str (ehval) &&
              strncasecmp (yyjson_get_str (ehval), "AUTOMATIC", 9) == 0)
            *pMS2B300_FLAGS (record + written) |= 0x04;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Continued")) && yyjson_get_bool (ehval))
            *pMS2B300_FLAGS (record + written) |= 0x08;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Duration")) && yyjson_is_num (ehval))
            *pMS2B300_STEPDURATION (record + written) = HO4u (yyjson_get_num (ehval) * 10000, swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/StepBetween")) && yyjson_is_num (ehval))
            *pMS2B300_INTERVALDURATION (record + written) = HO4u (yyjson_get_num (ehval) * 10000, swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Amplitude")) && yyjson_is_num (ehval))
            *pMS2B300_AMPLITUDE (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/InputChannel")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B300_INPUTCHANNEL (record + written), yyjson_get_str (ehval), 3);

          if ((ehval = yyjson_ptr_get (ehiterval, "/ReferenceAmplitude")) && yyjson_is_num (ehval))
            *pMS2B300_REFERENCEAMPLITUDE (record + written) = HO4u (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Coupling")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B300_COUPLING (record + written), yyjson_get_str (ehval), 12);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Rolloff")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B300_ROLLOFF (record + written), yyjson_get_str (ehval), 12);
        }
        else if (blockette_type == 310)
        {
          if ((ehval = yyjson_ptr_get (ehiterval, "/Trigger")) && yyjson_is_str (ehval) &&
              strncasecmp (yyjson_get_str (ehval), "AUTOMATIC", 9) == 0)
            *pMS2B310_FLAGS (record + written) |= 0x04;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Continued")) && yyjson_get_bool (ehval))
            *pMS2B310_FLAGS (record + written) |= 0x08;

          if ((ehval = yyjson_ptr_get (ehiterval, "/AmplitudeRange")) && yyjson_is_str (ehval))
          {
            if (strncasecmp (yyjson_get_str (ehval), "PEAKTOPEAK", 10) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x10;
            if (strncasecmp (yyjson_get_str (ehval), "ZEROTOPEAK", 10) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x20;
            if (strncasecmp (yyjson_get_str (ehval), "RMS", 3) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x40;
          }

          if ((ehval = yyjson_ptr_get (ehiterval, "/Duration")) && yyjson_is_num (ehval))
            *pMS2B310_DURATION (record + written) = HO4u (yyjson_get_num (ehval) * 10000, swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/SinePeriod")) && yyjson_is_num (ehval))
            *pMS2B310_PERIOD (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Amplitude")) && yyjson_is_num (ehval))
            *pMS2B310_AMPLITUDE (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/InputChannel")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B310_INPUTCHANNEL (record + written), yyjson_get_str (ehval), 3);

          if ((ehval = yyjson_ptr_get (ehiterval, "/ReferenceAmplitude")) && yyjson_is_num (ehval))
            *pMS2B310_REFERENCEAMPLITUDE (record + written) = HO4u (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Coupling")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_COUPLING (record + written), yyjson_get_str (ehval), 12);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Rolloff")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_ROLLOFF (record + written), yyjson_get_str (ehval), 12);
        }
        else if (blockette_type == 320)
        {
          if ((ehval = yyjson_ptr_get (ehiterval, "/Trigger")) && yyjson_is_str (ehval) &&
              strncasecmp (yyjson_get_str (ehval), "AUTOMATIC", 9) == 0)
            *pMS2B320_FLAGS (record + written) |= 0x04;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Continued")) && yyjson_get_bool (ehval))
            *pMS2B320_FLAGS (record + written) |= 0x08;

          if ((ehval = yyjson_ptr_get (ehiterval, "/AmplitudeRange")) && yyjson_is_str (ehval) &&
              strncasecmp (yyjson_get_str (ehval), "RANDOM", 6) == 0)
            *pMS2B320_FLAGS (record + written) |= 0x10;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Duration")) && yyjson_is_num (ehval))
            *pMS2B320_DURATION (record + written) = HO4u (yyjson_get_num (ehval) * 10000, swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Amplitude")) && yyjson_is_num (ehval))
            *pMS2B320_PTPAMPLITUDE (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/InputChannel")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_INPUTCHANNEL (record + written), yyjson_get_str (ehval), 3);

          if ((ehval = yyjson_ptr_get (ehiterval, "/ReferenceAmplitude")) && yyjson_is_num (ehval))
            *pMS2B320_REFERENCEAMPLITUDE (record + written) = HO4u (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Coupling")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_COUPLING (record + written), yyjson_get_str (ehval), 12);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Rolloff")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_ROLLOFF (record + written), yyjson_get_str (ehval), 12);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Noise")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B320_NOISETYPE (record + written), yyjson_get_str (ehval), 8);
        }
        else if (blockette_type == 390)
        {
          if ((ehval = yyjson_ptr_get (ehiterval, "/Trigger")) && yyjson_is_str (ehval) &&
              strncasecmp (yyjson_get_str (ehval), "AUTOMATIC", 9) == 0)
            *pMS2B390_FLAGS (record + written) |= 0x04;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Continued")) && yyjson_get_bool (ehval))
            *pMS2B390_FLAGS (record + written) |= 0x08;

          if ((ehval = yyjson_ptr_get (ehiterval, "/Duration")) && yyjson_is_num (ehval))
            *pMS2B390_DURATION (record + written) = HO4u (yyjson_get_num (ehval) * 10000, swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/Amplitude")) && yyjson_is_num (ehval))
            *pMS2B390_AMPLITUDE (record + written) = HO4f (yyjson_get_num (ehval), swapflag);

          if ((ehval = yyjson_ptr_get (ehiterval, "/InputChannel")) && yyjson_is_str (ehval))
            ms_strncpopen (pMS2B390_INPUTCHANNEL (record + written), yyjson_get_str (ehval), 3);
        }

        written += blockette_length;
      }

      /* Add Blockette 395 if EndTime is included */
      if ((ehval = yyjson_ptr_get (ehiterval, "/EndTime")) && yyjson_is_str (ehval))
      {
        blockette_type  = 395;
        blockette_length = 16;

        if ((recbuflen - written) < blockette_length)
        {
          ms_log (2, "%s: Record length not large enough for B%d\n", msr->sid, blockette_type);
          yyjson_doc_free (ehdoc);
          return -1;
        }

        *next_blockette = HO2u ((uint16_t)written, swapflag);
        next_blockette  = pMS2B395_NEXT (record + written);
        *pMS2FSDH_NUMBLOCKETTES (record) += 1;

        memset (record + written, 0, blockette_length);
        *pMS2B395_TYPE (record + written) = HO2u (blockette_type, swapflag);
        *pMS2B395_NEXT (record + written) = 0;

        if (ms_timestr2btime (yyjson_get_str (ehval), (uint8_t *)pMS2B395_YEAR (record + written),
                              msr->sid, swapflag) == -1)
        {
          ms_log (2, "%s: Cannot convert B%d time: %s\n", msr->sid, blockette_type, yyjson_get_str (ehval));
          yyjson_doc_free (ehdoc);
          return -1;
        }

        written += blockette_length;
      }
    }
  } /* End if /FDSN/Event/Detection */

  if (ehdoc)
  {
    yyjson_doc_free (ehdoc);
  }

  return written;
} /* End of msr3_pack_header2() */

/************************************************************************
 *  Pack data samples.  The input data samples specified as 'src' will
 *  be packed with 'encoding' format and placed in 'dest'.
 *
 *  Return number of samples packed on success and a negative on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ************************************************************************/
static int
msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
               char sampletype, int8_t encoding, int8_t swapflag,
               uint16_t *byteswritten, const char *sid, int8_t verbose)
{
  int nsamples;

  if (byteswritten)
    *byteswritten = 0;

  /* Decide if this is a format that we can encode */
  switch (encoding)
  {
  case DE_TEXT:
    if (sampletype != 't' && sampletype != 'a')
    {
      ms_log (2, "%s: Sample type must be text (t) for text encoding not '%c'\n",
              sid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing text data\n", sid);

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
      ms_log (2, "%s: Not enough space in record (%d) for INT16 encoding, need at least %"PRIsize_t" bytes\n",
              sid, maxdatabytes, sizeof(int16_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing INT16 data samples\n", sid);

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
      ms_log (2, "%s: Not enough space in record (%d) for INT32 encoding, need at least %"PRIsize_t" bytes\n",
              sid, maxdatabytes, sizeof(int32_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing INT32 data samples\n", sid);

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
      ms_log (2, "%s: Not enough space in record (%d) for FLOAT32 encoding, need at least %"PRIsize_t" bytes\n",
              sid, maxdatabytes, sizeof(float));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing FLOAT32 data samples\n", sid);

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
      ms_log (2, "%s: Not enough space in record (%d) for FLOAT64 encoding, need at least %"PRIsize_t" bytes\n",
              sid, maxdatabytes, sizeof(double));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing FLOAT64 data samples\n", sid);

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
      ms_log (0, "%s: Packing Steim1 data frames\n", sid);

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
      ms_log (0, "%s: Packing Steim2 data frames\n", sid);

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

/***************************************************************************
 * ms_ratapprox:
 *
 * Find an approximate rational number for a real through continued
 * fraction expansion.  Given a double precsion 'real' find a
 * numerator (num) and denominator (den) whose absolute values are not
 * larger than 'maxval' while trying to reach a specified 'precision'.
 *
 * Returns the number of iterations performed.
 ***************************************************************************/
static int
ms_ratapprox (double real, int *num, int *den, int maxval, double precision)
{
  double realj, preal;
  char pos;
  int pnum, pden;
  int iterations = 1;
  int Aj1, Aj2, Bj1, Bj2;
  int bj = 0;
  int Aj = 0;
  int Bj = 1;

  if (real >= 0.0)
  {
    pos   = 1;
    realj = real;
  }
  else
  {
    pos   = 0;
    realj = -real;
  }

  preal = realj;

  bj    = (int)(realj + precision);
  realj = 1 / (realj - bj);
  Aj    = bj;
  Aj1   = 1;
  Bj    = 1;
  Bj1   = 0;
  *num = pnum = Aj;
  *den = pden = Bj;
  if (!pos)
    *num = -*num;

  while (ms_dabs (preal - (double)Aj / (double)Bj) > precision &&
         Aj < maxval && Bj < maxval)
  {
    Aj2   = Aj1;
    Aj1   = Aj;
    Bj2   = Bj1;
    Bj1   = Bj;
    bj    = (int)(realj + precision);
    realj = 1 / (realj - bj);
    Aj    = bj * Aj1 + Aj2;
    Bj    = bj * Bj1 + Bj2;
    *num  = pnum;
    *den  = pden;
    if (!pos)
      *num = -*num;
    pnum = Aj;
    pden = Bj;

    iterations++;
  }

  if (pnum < maxval && pden < maxval)
  {
    *num = pnum;
    *den = pden;
    if (!pos)
      *num = -*num;
  }

  return iterations;
}

/***************************************************************************
 * ms_rsqrt64:
 *
 * An optimized reciprocal square root calculation from:
 *   Matthew Robertson (2012). "A Brief History of InvSqrt"
 *   https://cs.uwaterloo.ca/~m32rober/rsqrt.pdf
 *
 * Further reference and description:
 *   https://en.wikipedia.org/wiki/Fast_inverse_square_root
 *
 * Modifications:
 * Add 2 more iterations of Newton's method to increase accuracy,
 * specifically for large values.
 * Use memcpy instead of assignment through differing pointer types.
 *
 * Returns 0 if the host is little endian, otherwise 1.
 ***************************************************************************/
static double
ms_rsqrt64 (double val)
{
  uint64_t i;
  double x2;
  double y;

  x2 = val * 0.5;
  y  = val;
  memcpy (&i, &y, sizeof (i));
  i = 0x5fe6eb50c7b537a9ULL - (i >> 1);
  memcpy (&y, &i, sizeof (y));
  y = y * (1.5 - (x2 * y * y));
  y = y * (1.5 - (x2 * y * y));
  y = y * (1.5 - (x2 * y * y));

  return y;
} /* End of ms_rsqrt64() */

/***************************************************************************
 * ms_reduce_rate:
 *
 * Reduce the specified sample rate into two "factors" (in some cases
 * the second factor is actually a divisor).
 *
 * Integer rates between 1 and 32767 can be represented exactly.
 *
 * Integer rates higher than 32767 will be matched as closely as
 * possible with the deviation becoming larger as the integers reach
 * (32767 * 32767).
 *
 * Non-integer rates between 32767.0 and 1.0/32767.0 are represented
 * exactly when possible and approximated otherwise.
 *
 * Non-integer rates greater than 32767 or less than 1/32767 are not supported.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
static int
ms_reduce_rate (double samprate, int16_t *factor1, int16_t *factor2)
{
  int num;
  int den;
  int32_t intsamprate = (int32_t) (samprate + 0.5);

  int32_t searchfactor1;
  int32_t searchfactor2;
  int32_t closestfactor;
  int32_t closestdiff;
  int32_t diff;

  /* Handle case of integer sample values. */
  if (ms_dabs (samprate - intsamprate) < 0.0000001)
  {
    /* If integer sample rate is less than range of 16-bit int set it directly */
    if (intsamprate <= 32767)
    {
      *factor1 = intsamprate;
      *factor2 = 1;
      return 0;
    }
    /* If integer sample rate is within the maximum possible nominal rate */
    else if (intsamprate <= (32767 * 32767))
    {
      /* Determine the closest factors that represent the sample rate.
       * The approximation gets worse as the values increase. */
      searchfactor1 = (int)(1.0 / ms_rsqrt64 (samprate));
      closestdiff   = searchfactor1;
      closestfactor = searchfactor1;

      while ((intsamprate % searchfactor1) != 0)
      {
        searchfactor1 -= 1;

        /* Track the factor that generates the closest match */
        searchfactor2 = intsamprate / searchfactor1;
        diff          = intsamprate - (searchfactor1 * searchfactor2);
        if (diff < closestdiff)
        {
          closestdiff   = diff;
          closestfactor = searchfactor1;
        }

        /* If the next iteration would create a factor beyond the limit
         * we accept the closest factor */
        if ((intsamprate / (searchfactor1 - 1)) > 32767)
        {
          searchfactor1 = closestfactor;
          break;
        }
      }

      searchfactor2 = intsamprate / searchfactor1;

      if (searchfactor1 <= 32767 && searchfactor2 <= 32767)
      {
        *factor1 = searchfactor1;
        *factor2 = searchfactor2;
        return 0;
      }
    }
  }
  /* Handle case of non-integer less than 16-bit int range */
  else if (samprate <= 32767.0)
  {
    /* For samples/seconds, determine, potentially approximate, numerator and denomiator */
    ms_ratapprox (samprate, &num, &den, 32767, 1e-8);

    /* Negate the factor2 to denote a division operation */
    *factor1 = (int16_t)num;
    *factor2 = (int16_t)-den;
    return 0;
  }

  return -1;
} /* End of ms_reduce_rate() */

/***************************************************************************
 * ms_genfactmult:
 *
 * Generate an appropriate SEED sample rate factor and multiplier from
 * a double precision sample rate.
 *
 * If the samplerate > 0.0 it is expected to be a rate in SAMPLES/SECOND.
 * If the samplerate < 0.0 it is expected to be a period in SECONDS/SAMPLE.
 *
 * Results use SAMPLES/SECOND notation when sample rate >= 1.0
 * Results use SECONDS/SAMPLE notation when samles rates < 1.0
 *
 * Returns 0 on success and -1 on error or calculation not possible.
 ***************************************************************************/
static int
ms_genfactmult (double samprate, int16_t *factor, int16_t *multiplier)
{
  int16_t factor1;
  int16_t factor2;

  if (!factor || !multiplier)
    return -1;

  /* Convert sample period to sample rate */
  if (samprate < 0.0)
    samprate = -1.0 / samprate;

  /* Handle special case of zero */
  if (samprate == 0.0)
  {
    *factor     = 0;
    *multiplier = 0;
    return 0;
  }
  /* Handle sample rates >= 1.0 with the SAMPLES/SECOND representation */
  else if (samprate >= 1.0)
  {
    if (ms_reduce_rate (samprate, &factor1, &factor2) == 0)
    {
      *factor     = factor1;
      *multiplier = factor2;
      return 0;
    }
  }
  /* Handle sample rates < 1 with the SECONDS/SAMPLE representation */
  else
  {
    /* Reduce rate as a sample period and invert factor/multiplier */
    if (ms_reduce_rate (1.0 / samprate, &factor1, &factor2) == 0)
    {
      *factor     = -factor1;
      *multiplier = -factor2;
      return 0;
    }
  }

  return -1;
} /* End of ms_genfactmult() */

/***************************************************************************
 * Convenience function to convert a month-day time string to a SEED
 * 2.x "BTIME" structure.
 *
 * The 10-byte BTIME structure layout:
 *
 * Value  Type      Offset  Description
 * year   uint16_t  0       Four digit year (e.g. 1987)
 * day    uint16_t  2       Day of year (Jan 1st is 1)
 * hour   uint8_t   4       Hour (0 - 23)
 * min    uint8_t   5       Minute (0 - 59)
 * sec    uint8_t   6       Second (0 - 59, 60 for leap seconds)
 * unused uint8_t   7       Unused, included for alignment
 * fract  uint16_t  8       0.0001 seconds, i.e. 1/10ths of milliseconds (0—9999)
 *
 * Return nanoseconds on success and -1 on error.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static inline uint32_t
ms_timestr2btime (const char *timestr, uint8_t *btime, const char *sid, int8_t swapflag)
{
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;
  nstime_t nstime;

  if (!timestr || !btime)
  {
    ms_log (2, "Required argument not defined: 'timestr' or 'btime'\n");
    return -1;
  }

  if ((nstime = ms_timestr2nstime (timestr)) == NSTERROR)
    return -1;

  if (ms_nstime2time (nstime, &year, &day, &hour, &min, &sec, &nsec))
    return -1;

  *((uint16_t *)(btime))     = HO2u (year, swapflag);
  *((uint16_t *)(btime + 2)) = HO2u (day, swapflag);
  *((uint8_t *)(btime + 4))  = hour;
  *((uint8_t *)(btime + 5))  = min;
  *((uint8_t *)(btime + 6))  = sec;
  *((uint8_t *)(btime + 7))  = 0;
  *((uint16_t *)(btime + 8)) = HO2u (nsec / 100000, swapflag);

  return nsec;
} /* End of timestr2btime() */
