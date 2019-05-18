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
#include "parson.h"

/* Internal from another source file */
extern double ms_nomsamprate (int factor, int multiplier);

/* Function(s) internal to this file */
static int msr3_pack_mseed3 (MS3Record *msr, void (*record_handler) (char *, int, void *),
                             void *handlerdata, int64_t *packedsamples,
                             uint32_t flags, int8_t verbose);

static int msr3_pack_mseed2 (MS3Record *msr, void (*record_handler) (char *, int, void *),
                             void *handlerdata, int64_t *packedsamples,
                             uint32_t flags, int8_t verbose);

static int msr_pack_data (void *dest, void *src, int maxsamples, int maxdatabytes,
                          char sampletype, int8_t encoding, int8_t swapflag,
                          uint16_t *byteswritten, char *sid, int8_t verbose);

static int ms_genfactmult (double samprate, int16_t *factor, int16_t *multiplier);


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

  if (msr->formatversion == 2) /* Pack version 2 if requested */
  {
    packedrecs = msr3_pack_mseed2 (msr, record_handler, handlerdata, packedsamples,
                                   flags, verbose);
  }
  else /* Pack version 3 otherwise */
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
  nstime_t nextstarttime;
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr)
    return -1;

  if (!record_handler)
  {
    ms_log (2, "%s(): record_handler() function pointer not set!\n", __func__);
    return -1;
  }

  if (msr->reclen < (MS3FSDH_LENGTH + strlen(msr->sid) + msr->extralength))
  {
    ms_log (2, "%s(%s): Record length (%d) is not large enough for header (%d), SID (%zu), and extra (%d)\n",
            __func__, msr->sid, msr->reclen, MS3FSDH_LENGTH, strlen(msr->sid), msr->extralength);
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

  /* Short cut: if there are no samples, record packing is complete */
  if (msr->numsamples <= 0)
  {
    /* Set encoding to ASCII for consistency and to reduce expectations */
    *pMS3FSDH_ENCODING(rawrec) = DE_ASCII;

    /* Calculate CRC (with CRC field set to 0) and set */
    memset (pMS3FSDH_CRC(rawrec), 0, sizeof(uint32_t));
    crc = ms_crc32c ((const uint8_t*)rawrec, dataoffset, 0);
    *pMS3FSDH_CRC(rawrec) = HO4u (crc, swapflag);

    if (verbose >= 1)
      ms_log (1, "%s: Packed %d byte record with no payload\n", msr->sid, dataoffset);

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
    ms_log (2, "%s(%s): Unknown sample type '%c'\n", __func__, msr->sid, msr->sampletype);
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

    /* Update record start time for next record */
    nextstarttime = ms_sampletime (msr->starttime, totalpackedsamples, msr->samprate);

    if (ms_nstime2time (nextstarttime, &year, &day, &hour, &min, &sec, &nsec))
    {
      ms_log (2, "%s(%s): Cannot convert next record starttime: %" PRId64 "\n",
              __func__, msr->sid, nextstarttime);
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

  sidlength = strlen (msr->sid);

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + sidlength + msr->extralength))
  {
    ms_log (2, "%s(%s): Buffer length (%d) is not large enough for fixed header (%d), SID (%zu), and extra (%d)\n",
            __func__, msr->sid, msr->reclen, MS3FSDH_LENGTH, sidlength, msr->extralength);
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

  /* Ensure that SID length fits in format, which uses data type uint8_t */
  if (sidlength > 255)
  {
    ms_log (2, "%s(%s): Source ID too long: %zu bytes\n", __func__, msr->sid, sidlength);
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
  *pMS3FSDH_ENCODING (record) = msr->encoding;

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
 ***************************************************************************/
int
msr3_pack_mseed2 (MS3Record *msr, void (*record_handler) (char *, int, void *),
                  void *handlerdata, int64_t *packedsamples,
                  uint32_t flags, int8_t verbose)
{
  char *rawrec = NULL;
  char *encoded = NULL;  /* Separate encoded data buffer for alignment */
  int8_t swapflag;
  int dataoffset = 0;
  int headerlen;

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
    return -1;

  if (!record_handler)
  {
    ms_log (2, "%s(): record_handler() function pointer not set!\n", __func__);
    return -1;
  }

  if (msr->reclen < 128)
  {
    ms_log (2, "%s(%s): Record length (%d) is not large enough, must be >= 128 bytes\n",
            __func__, msr->sid, msr->reclen);
    return -1;
  }

  /* Check that record length is a power of 2.
   * Power of two if (X & (X - 1)) == 0 */
  if ((msr->reclen & (msr->reclen - 1)) != 0)
  {
    ms_log (2, "%s(%s): Cannot create miniSEED 2, record length (%d) is not a power of 2\n",
            __func__, msr->sid, msr->reclen);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  /* Allocate space for data record */
  rawrec = (char *)libmseed_memory.malloc (msr->reclen);

  if (rawrec == NULL)
  {
    ms_log (2, "%s(%s): Cannot allocate memory\n", __func__, msr->sid);
    return -1;
  }

  /* Pack fixed header and extra headers, returned size is data offset */
  headerlen = msr3_pack_header2 (msr, rawrec, msr->reclen, verbose);

  if (headerlen < 0)
  {
    ms_log (2, "%s(%s): Cannot pack miniSEED version 2 header\n", __func__, msr->sid);
    return -1;
  }

  /* Short cut: if there are no samples, record packing is complete */
  if (msr->numsamples <= 0)
  {
    /* Set encoding to ASCII for consistency and to reduce expectations */
    *pMS2B1000_ENCODING (rawrec + 48) = DE_ASCII;

    /* Set empty part of record to zeros */
    memset (rawrec + headerlen, 0, msr->reclen - headerlen);

    if (verbose >= 1)
      ms_log (1, "%s: Packed %d byte record with no payload\n", msr->sid, msr->reclen);

    /* Send record to handler */
    record_handler (rawrec, msr->reclen, handlerdata);

    libmseed_memory.free (rawrec);

    if (packedsamples)
      *packedsamples = 0;

    return 1;
  }

  samplesize = ms_samplesize (msr->sampletype);

  if (!samplesize)
  {
    ms_log (2, "%s(%s): Unknown sample type '%c'\n", __func__, msr->sid, msr->sampletype);
    return -1;
  }

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

  /* Set data offset in header */
  *pMS2FSDH_DATAOFFSET(rawrec) = HO2u (dataoffset, swapflag);

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

    /* Copy encoded data into record */
    memcpy (rawrec + dataoffset, encoded, datalength);

    /* Update number of samples */
    *pMS2FSDH_NUMSAMPLES(rawrec) = HO2u (packsamples, swapflag);

    if (verbose >= 1)
      ms_log (1, "%s: Packed %d samples into %d byte record\n", msr->sid, packsamples, msr->reclen);

    /* Send record to handler */
    record_handler (rawrec, msr->reclen, handlerdata);

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
      ms_log (2, "%s(%s): Cannot convert next record starttime: %" PRId64 "\n",
              __func__, msr->sid, nextstarttime);
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
    ms_log (1, "%s: Packed %d total samples\n", msr->sid, totalpackedsamples);

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
 ***************************************************************************/
int
msr3_pack_header2 (MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int written = 0;
  int8_t swapflag;

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

  JSON_Object *rootobject = NULL;
  JSON_Value *rootvalue   = NULL;
  JSON_Value *extravalue  = NULL;

  const char *header_string;
  double header_number;

  if (!msr || !record)
    return -1;

  /* Set default record length and encoding if needed */
  if (msr->reclen == -1)
    msr->reclen = 4096;
  if (msr->encoding == -1)
    msr->encoding = DE_STEIM2;

  if (msr->reclen < 128 || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "%s(%s): Record length is out of range: %d\n", __func__, msr->sid, msr->reclen);
    return -1;
  }

  /* Check that record length is a power of 2.
   * Power of two if (X & (X - 1)) == 0 */
  if ((msr->reclen & (msr->reclen - 1)) != 0)
  {
    ms_log (2, "%s(%s): Cannot pack miniSEED 2, record length (%d) is not a power of 2\n",
            __func__, msr->sid, msr->reclen);
    return -1;
  }

  /* Calculate the record length as an exponent of 2 */
  for (reclenfind = 1, reclenexp = 1; reclenfind <= MAXRECLEN; reclenexp++)
  {
    reclenfind *= 2;
    if (reclenfind == msr->reclen)
      break;
  }

  /* Parse identifier codes from full identifier */
  if (ms_sid2nslc (msr->sid, network, station, location, channel))
  {
    ms_log (2, "%s(%s): Cannot parse identifier codes from full identifier\n",
            __func__, msr->sid, msr->reclen);
    return -1;
  }

  /* Verify that identifier codes will fit into and are appropriate for miniSEED 2 */
  if (strlen (network) > 2 || strlen (station) > 5 || strlen (location) > 2 || strlen (channel) != 3)
  {
    ms_log (2, "%s(%s): Cannot create miniSEED 2 for N,S,L,C codes: %s, %s, %s, %s\n",
            __func__, msr->sid, network, station, location, channel);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  if (verbose > 2 && swapflag)
    ms_log (1, "%s: Byte swapping needed for packing of header\n", msr->sid);

  /* Break down start time into individual components */
  if (ms_nstime2time (msr->starttime, &year, &day, &hour, &min, &sec, &nsec))
  {
    ms_log (2, "%s(%s): Cannot convert starttime: %" PRId64 "\n", __func__, msr->sid, msr->starttime);
    return -1;
  }

  /* Calculate time at fractional 100usec resolution and microsecond offset */
  fsec = nsec / 100000;
  msec_offset = ((nsec / 1000) - (fsec * 100));

  /* Generate factor & multipler representation of sample rate */
  if (ms_genfactmult (msr3_sampratehz(msr), &factor, &multiplier))
  {
    ms_log (2, "%s(%s): Cannot convert sample rate (%g) to factor and multiplier\n",
            __func__, msr->sid, msr->samprate);
    return -1;
  }

  /* Parse extra headers if present */
  if (msr->extra && msr->extralength > 0)
  {
    json_set_allocation_functions (libmseed_memory.malloc,
                                   libmseed_memory.free);

    /* Parse JSON extra headers */
    rootvalue = json_parse_string (msr->extra);
    if (!rootvalue)
    {
      ms_log (2, "%s(): Extra headers are not JSON\n", __func__);
      return -1;
    }

    /* Get expected root object */
    rootobject = json_value_get_object (rootvalue);
    if (!rootobject)
    {
      ms_log (2, "%s(): Extra headers are not a JSON object\n", __func__);
      json_value_free (rootvalue);
      return -1;
    }
  }

  /* Build fixed header */
  memcpy (pMS2FSDH_SEQNUM (record), "000000", 6);

  if ((header_string = json_object_dotget_string (rootobject, "FDSN.DataQuality")) &&
      MS2_ISDATAINDICATOR (header_string[0]))
    *pMS2FSDH_DATAQUALITY (record) = header_string[0];
  else
    *pMS2FSDH_DATAQUALITY (record) = 'D';

  *pMS2FSDH_RESERVED (record) = 0;
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
  if (json_object_dotget_boolean (rootobject, "FDSN.Event.Begin") == 1) /* Bit 2 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x04;
  if (json_object_dotget_boolean (rootobject, "FDSN.Event.End") == 1) /* Bit 3 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x08;

  if ((extravalue = json_object_dotget_value (rootobject, "FDSN.Time.LeapSecond")) &&
      json_value_get_type (extravalue) == JSONNumber)
  {
    if (json_value_get_number (extravalue) > 0) /* Bit 4 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x10;
    if (json_value_get_number (extravalue) < 0) /* Bit 5 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x20;
  }

  if (json_object_dotget_boolean (rootobject, "FDSN.Event.InProgress") == 1) /* Bit 6 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x40;

  /* Map I/O and clock bit flags */
  *pMS2FSDH_IOFLAGS (record) = 0;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.StationVolumeParityError") == 1) /* Bit 0 */
    *pMS2FSDH_IOFLAGS (record) |= 0x01;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.LongRecordRead") == 1) /* Bit 1 */
    *pMS2FSDH_IOFLAGS (record) |= 0x02;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.ShortRecordRead") == 1) /* Bit 2 */
    *pMS2FSDH_IOFLAGS (record) |= 0x04;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.StartOfTimeSeries") == 1) /* Bit 3 */
    *pMS2FSDH_IOFLAGS (record) |= 0x08;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.EndOfTimeSeries") == 1) /* Bit 4 */
    *pMS2FSDH_IOFLAGS (record) |= 0x10;
  if (msr->flags & 0x04) /* Bit 5 */
    *pMS2FSDH_IOFLAGS (record) |= 0x20;

  /* Map data quality bit flags */
  *pMS2FSDH_DQFLAGS (record) = 0;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.AmplifierSaturation") == 1) /* Bit 0 */
    *pMS2FSDH_DQFLAGS (record) |= 0x01;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.DigitizerClipping") == 1) /* Bit 1 */
    *pMS2FSDH_DQFLAGS (record) |= 0x02;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.Spikes") == 1) /* Bit 2 */
    *pMS2FSDH_DQFLAGS (record) |= 0x04;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.Glitches") == 1) /* Bit 3 */
    *pMS2FSDH_DQFLAGS (record) |= 0x08;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.MissingData") == 1) /* Bit 4 */
    *pMS2FSDH_DQFLAGS (record) |= 0x10;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.TelemetrySyncError") == 1) /* Bit 5 */
    *pMS2FSDH_DQFLAGS (record) |= 0x20;
  if (json_object_dotget_boolean (rootobject, "FDSN.Flags.FilterCharging") == 1) /* Bit 6 */
    *pMS2FSDH_DQFLAGS (record) |= 0x40;
  if (msr->flags & 0x02) /* Bit 7 */
    *pMS2FSDH_DQFLAGS (record) |= 0x80;

  if ((extravalue = json_object_dotget_value (rootobject, "FDSN.Time.Correction")) &&
      json_value_get_type (extravalue) == JSONNumber)
  {
    *pMS2FSDH_TIMECORRECT (record) = HO4d (json_value_get_number (extravalue) * 10000, swapflag);
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
  *pMS2B1000_ENCODING (record + written)  = msr->encoding;
  *pMS2B1000_BYTEORDER (record + written) = 1;
  *pMS2B1000_RECLEN (record + written)    = reclenexp;
  *pMS2B1000_RESERVED (record + written)  = 0;

  written += 8;

  /* Add Blockette 1001 if microsecond offset or timing quality is present */
  if (msec_offset || mseh_exists (msr, "FDSN.Time.Quality"))
  {
    *next_blockette = HO2u ((uint16_t)written, swapflag);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;
    next_blockette = pMS2B1001_NEXT (record + written);

    *pMS2B1001_TYPE (record + written)          = HO2u (1001, swapflag);
    *pMS2B1001_NEXT (record + written)          = 0;

    if (mseh_get_number (msr, "FDSN.Time.Quality", &header_number) == 0)
      *pMS2B1001_TIMINGQUALITY (record + written) = (uint8_t) (header_number + 0.5);
    else
      *pMS2B1001_TIMINGQUALITY (record + written) = 0;

    *pMS2B1001_MICROSECOND (record + written)   = msec_offset;
    *pMS2B1001_RESERVED (record + written)      = 0;
    *pMS2B1001_FRAMECOUNT (record + written)    = 0;

    written += 8;
  }

  /* Add Blockette 100 if sample rate is not well represented by factor/multiplier */
  if (ms_dabs(msr3_sampratehz(msr) - ms_nomsamprate(factor, multiplier)) > 0.0001)
  {
    *next_blockette = HO2u ((uint16_t)written, swapflag);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;
    next_blockette = pMS2B100_NEXT (record + written);

    *pMS2B100_TYPE (record + written)     = HO2u (100, swapflag);
    *pMS2B100_NEXT (record + written)     = 0;
    *pMS2B100_SAMPRATE (record + written) = HO4f (msr->samprate, swapflag);
    *pMS2B100_FLAGS (record + written)    = 0;
    memset (pMS2B100_RESERVED (record + written), 0, 3);

    written += 12;
  }

  // FDSN.Time.Exception array B500

  // FDSN.Event.Detection array B200, 201

  // FDSN.Event.Calibration.Sequence array B300, 310, 320, 390

  if (rootvalue)
    json_value_free (rootvalue);

  return written;
} /* End of msr3_pack_header2() */

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
