/***************************************************************************
 * Generic routines to pack miniSEED records using an MS3Record as a
 * header template and data source.
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

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extraheaders.h"
#include "internalstate.h"
#include "libmseed.h"
#include "mseedformat.h"
#include "packdata.h"

/* Internal from another source file */
extern double ms_nomsamprate (int factor, int multiplier);

/* Function(s) internal to this file */

static int msr3_pack_header2_offsets (const MS3Record *msr, char *record, uint32_t recbuflen,
                                      uint16_t *blockette_1000_offset,
                                      uint16_t *blockette_1001_offset, int8_t verbose);

static int64_t msr_pack_data (void *dest, void *src, uint64_t maxsamples, uint64_t maxdatabytes,
                              char sampletype, int8_t encoding, int8_t swapflag,
                              uint32_t *byteswritten, const char *sid, int8_t verbose);

static void repack_steim_to_bigendian (char *data, uint32_t datasize, int8_t encoding);

static void repack_payload_order (const MS3Record *msr, char *data, uint32_t datasize,
                                  int8_t target_bigendian);

static int ms_genfactmult (double samprate, int16_t *factor, int16_t *multiplier);

static int lm_real2uint32 (double real, double scale, uint32_t *value, const char *sid,
                           const char *name);

static int lm_real2float (double real, float *value, const char *sid, const char *name);

static nstime_t ms_timestr2btime (const char *timestr, uint8_t *btime, int8_t *usec_offset,
                                  const char *sid, int8_t swapflag);

static nstime_t nstime2fsec_usec_offset (nstime_t nstime, uint16_t *fsec, int8_t *usec_offset);

/** ************************************************************************
 * @brief Pack data into miniSEED records using a callback function to handle
 * the records.
 *
 * Packing is performed according to the version at
 * @ref MS3Record.formatversion.
 *
 * The @ref MS3Record.datasamples array and @ref MS3Record.numsamples
 * value will __not__ be changed by this routine.  It is the
 * responsibility of the calling routine to adjust the data buffer if
 * desired.
 *
 * As each record is filled and finished they are passed to @p
 * record_handler() which should expect 1) a @c char* to the record,
 * 2) the length of the record and 3) a pointer supplied by the
 * original caller containing optional private data (@p handlerdata).
 * It is the responsibility of @p record_handler() to process the
 * record, the memory will be re-used or freed when @p record_handler() returns.
 *
 * The following data encodings and expected @ref MS3Record.sampletype
 * are supported:
 *   - ::DE_TEXT (0), Text, expects type @c 't'
 *   - ::DE_INT16 (1), 16-bit integer, expects type @c 'i'
 *   - ::DE_INT32 (3), 32-bit integer, expects type @c 'i'
 *   - ::DE_FLOAT32 (4), 32-bit float (IEEE), expects type @c 'f'
 *   - ::DE_FLOAT64 (5), 64-bit float (IEEE), expects type @c 'd'
 *   - ::DE_STEIM1 (10), Steim-1 compressed integers, expects type @c 'i'
 *   - ::DE_STEIM2 (11), Steim-2 compressed integers, expects type @c 'i'
 *
 * If @p flags has ::MSF_FLUSHDATA set, all of the data will be packed
 * into data records even though the last one will probably be smaller
 * than requested or, in the case of miniSEED 2, unfilled.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when @p msr.reclen and @p msr.encoding
 * are set to -1.
 *
 * To create a header-only record with no data payload (i.e., no samples), set
 * @ref MS3Record.numsamples to 0.
 *
 * @param[in] msr ::MS3Record containing data to pack
 * @param[in] record_handler() Callback function called for each record
 * @param[in] handlerdata A pointer that will be provided to the @p record_handler()
 * @param[out] packedsamples The number of samples packed, returned to caller
 * @param[in] flags Bit flags used to control the packing process:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - @c ::MSF_PACKVER2 : Pack miniSEED version 2 regardless of ::MS3Record.formatversion
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the number of records created on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see msr3_pack_init()
 * @see msr3_pack_next()
 * @see msr3_pack_free()
 ***************************************************************************/
int
msr3_pack (const MS3Record *msr, void (*record_handler) (char *, int, void *), void *handlerdata,
           int64_t *packedsamples, uint32_t flags, int8_t verbose)
{
  MS3RecordPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result;
  int recordcount = 0;

  if (!msr)
  {
    ms_log (2, "%s(): Required input not defined: 'msr'\n", __func__);
    return -1;
  }

  if (!record_handler)
  {
    ms_log (2, "callback record_handler() function pointer not set!\n");
    return -1;
  }

  /* Initialize packer */
  packer = msr3_pack_init (msr, flags, verbose);
  if (!packer)
    return -1;

  /* Generate records using generator interface */
  while ((result = msr3_pack_next (packer, &record, &reclen)) == 1)
  {
    record_handler (record, reclen, handlerdata);
    recordcount++;
  }

  /* Free packer and get total packed samples */
  msr3_pack_free (&packer, packedsamples);

  /* Return record count on success, -1 on error */
  return (result == 0) ? recordcount : -1;
} /* End of msr3_pack() */

/***************************************************************************
 * Maximum samples that fit in maxdatabytes for a given encoding, the same
 * formula used by lm_pack_state_init() to set packer->maxsamples.
 ***************************************************************************/
static uint32_t
lm_pack_maxsamples (uint32_t maxdatabytes, uint8_t encoding, uint8_t samplesize)
{
  if (encoding == DE_STEIM1)
    return (maxdatabytes / 64) * STEIM1_FRAME_MAX_SAMPLES;
  else if (encoding == DE_STEIM2)
    return (maxdatabytes / 64) * STEIM2_FRAME_MAX_SAMPLES;
  else
    return (samplesize) ? maxdatabytes / samplesize : 0;
} /* End of lm_pack_maxsamples() */

/***************************************************************************
 * Test whether a record with the given geometry cannot hold numsamples,
 * i.e. whether msr3_pack_next() would return 0 on a fresh packer with this
 * geometry and without ::MSF_FLUSHDATA set. No allocation, no writing.
 *
 * Only miniSEED 3 geometry is pure arithmetic; miniSEED 2 depends on the
 * blockette layout assembled by msr3_pack_header2_offsets(), so this
 * always returns 0 (not determined) for miniSEED 2 and callers must fall
 * back to a full msr3_pack_init() to find out.
 *
 * Returns non-zero if the record is short of a full record, and 0 if it
 * may already be able to fill one or the geometry cannot be determined.
 ***************************************************************************/
int
lm_pack_short_of_record (int8_t formatversion, uint32_t maxreclen, size_t sidlength,
                         uint16_t extralength, uint8_t encoding, uint8_t samplesize,
                         int64_t numsamples)
{
  uint32_t offset;
  uint32_t maxdatabytes;
  uint32_t maxsamples;

  if (formatversion != 3 || !samplesize || numsamples <= 0 || maxreclen < MINRECLEN ||
      maxreclen > MAXRECLEN)
    return 0;

  offset = MS3FSDH_LENGTH + (uint32_t)sidlength + extralength;

  if (maxreclen < offset)
    return 0;

  maxdatabytes = maxreclen - offset;
  maxsamples = lm_pack_maxsamples (maxdatabytes, encoding, samplesize);

  return (uint64_t)numsamples < maxsamples;
} /* End of lm_pack_short_of_record() */

/** ************************************************************************
 * @brief Initialize a packer for generator-style record creation
 *
 * Create and initialize an opaque ::MS3RecordPacker context for generating
 * miniSEED records one at a time from an ::MS3Record.
 *
 * To create a header-only record with no data payload (i.e., no samples), set
 * @ref MS3Record.numsamples to 0.
 *
 * The packer should be freed with msr3_pack_free() when done.
 *
 * @param[in] msr ::MS3Record containing data to pack
 * @param[in] flags Bit flags used to control the packing process:
 * @parblock
 *  - @c ::MSF_FLUSHDATA : Pack all data in the buffer
 *  - @c ::MSF_PACKVER2 : Pack miniSEED version 2 regardless of ::MS3Record.formatversion
 * @endparblock
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns pointer to ::MS3RecordPacker on success and NULL on error,
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see msr3_pack_next()
 * @see msr3_pack_free()
 ***************************************************************************/
MS3RecordPacker *
msr3_pack_init (const MS3Record *msr, uint32_t flags, int8_t verbose)
{
  MS3RecordPacker *packer = NULL;

  if (!msr)
  {
    ms_log (2, "%s(): Required input not defined: 'msr'\n", __func__);
    return NULL;
  }

  /* Allocate pack state context */
  packer = (MS3RecordPacker *)libmseed_memory.malloc (sizeof (MS3RecordPacker));
  if (!packer)
  {
    ms_log (2, "Cannot allocate memory for packer context\n");
    return NULL;
  }

  memset (packer, 0, sizeof (MS3RecordPacker));

  if (lm_pack_state_init (packer, msr, flags, verbose))
  {
    lm_pack_state_free (packer);
    libmseed_memory.free (packer);
    return NULL;
  }

  return packer;
} /* End of msr3_pack_init() */

/***************************************************************************
 * Start (or restart) a packing session in a caller-allocated packer,
 * reusing its rawrec/encoded buffers when they are already large enough.
 *
 * This is everything msr3_pack_init() does beyond allocating the
 * ::MS3RecordPacker itself, factored out so a packer embedded in a
 * longer-lived context (e.g. ::MS3TraceListPacker) can be re-initialized
 * for each segment without a malloc/free cycle.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
int
lm_pack_state_init (MS3RecordPacker *packer, const MS3Record *msr, uint32_t flags, int8_t verbose)
{
  if (!packer || !msr)
  {
    ms_log (2, "%s(): Required input not defined: 'packer' or 'msr'\n", __func__);
    return -1;
  }

  if ((msr->reclen != -1) && (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN))
  {
    ms_log (2, "%s: Record length is out of range: %d\n", msr->sid, msr->reclen);
    return -1;
  }

  if (msr->starttime == NSTUNSET || msr->starttime == NSTERROR)
  {
    ms_log (2, "%s: Record start time is unset\n", msr->sid);
    return -1;
  }

  /* Reset per-session state, retaining rawrec/encoded and their allocated
   * sizes for reuse regardless of what else this struct grows to hold */
  {
    char *rawrec = packer->rawrec;
    uint32_t rawrec_size = packer->rawrec_size;
    char *encoded = packer->encoded;
    uint32_t encoded_size = packer->encoded_size;

    memset (packer, 0, sizeof (*packer));

    packer->rawrec = rawrec;
    packer->rawrec_size = rawrec_size;
    packer->encoded = encoded;
    packer->encoded_size = encoded_size;
  }

  packer->msr = msr;
  packer->flags = flags;
  packer->verbose = verbose;

  /* Determine format version */
  packer->formatversion = (msr->formatversion == 2 || flags & MSF_PACKVER2) ? 2 : 3;

  /* Use default record length and encoding if needed */
  packer->maxreclen = (msr->reclen < 0) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  packer->encoding = (msr->encoding < 0) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  /* Check to see if byte swapping is needed */
  if (packer->formatversion == 3)
  {
    /* miniSEED 3 is little endian */
    packer->swapflag = (ms_bigendianhost ()) ? 1 : 0;
  }
  else
  {
    /* miniSEED 2 is big endian */
    packer->swapflag = (ms_bigendianhost ()) ? 0 : 1;
  }

  /* Validate record length vs header size */
  if (packer->formatversion == 3)
  {
    if (packer->maxreclen < (MS3FSDH_LENGTH + strlen (msr->sid) + msr->extralength))
    {
      ms_log (2,
              "%s: Record length (%u) is not large enough for header (%u), SID (%" PRIsize_t
              "), and extra (%d)\n",
              msr->sid, packer->maxreclen, MS3FSDH_LENGTH, strlen (msr->sid), msr->extralength);
      return -1;
    }
  }

  /* Allocate space for generated record, reusing the existing buffer if
   * already large enough */
  if (packer->rawrec_size < packer->maxreclen)
  {
    char *rawrec = (char *)libmseed_memory.realloc (packer->rawrec, packer->maxreclen);
    if (!rawrec)
    {
      ms_log (2, "%s: Cannot allocate memory for record buffer\n", msr->sid);
      return -1;
    }

    packer->rawrec = rawrec;
    packer->rawrec_size = packer->maxreclen;
  }

  memset (packer->rawrec, 0, packer->maxreclen);

  /* For records with samples, validate sample type before packing */
  if (msr->numsamples > 0)
  {
    packer->samplesize = ms_samplesize (msr->sampletype);
    if (!packer->samplesize)
    {
      ms_log (2, "%s: Unknown sample type '%c'\n", msr->sid, msr->sampletype);
      return -1;
    }
  }

  /* Pack header (required for header-only and data-containing records) */
  if (packer->formatversion == 3)
  {
    packer->dataoffset = msr3_pack_header3 (msr, packer->rawrec, packer->maxreclen, verbose);
  }
  else
  {
    packer->dataoffset = msr3_pack_header2_offsets (msr, packer->rawrec, packer->maxreclen,
                                                    &packer->blockette_1000_offset,
                                                    &packer->blockette_1001_offset, verbose);

    if (packer->dataoffset > 0 && msr->numsamples > 0)
    {
      /* For Steim encodings, align data offset to 64-byte boundary */
      if (packer->encoding == DE_STEIM1 || packer->encoding == DE_STEIM2)
      {
        packer->dataoffset = ((packer->dataoffset + 63) / 64) * 64;
      }

      /* The (possibly aligned) data offset must still leave room for data
       * within the record and fit the 16-bit v2 data-offset field. */
      if ((uint32_t)packer->dataoffset >= packer->maxreclen || packer->dataoffset > UINT16_MAX)
      {
        ms_log (2, "%s: Data offset (%d) does not fit within record length (%u)\n", msr->sid,
                packer->dataoffset, packer->maxreclen);
        return -1;
      }

      /* Set data offset in header */
      STORE2u (pMS2FSDH_DATAOFFSET (packer->rawrec), packer->dataoffset, packer->swapflag);
    }
  }

  if (packer->dataoffset < 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED header\n", msr->sid);
    return -1;
  }

  /* For records with samples, set up encoding buffers and parameters */
  if (msr->numsamples > 0)
  {
    /* Determine the max data bytes and sample count */
    packer->maxdatabytes = packer->maxreclen - packer->dataoffset;
    packer->maxsamples =
        lm_pack_maxsamples (packer->maxdatabytes, packer->encoding, packer->samplesize);

    /* The miniSEED 2 FSDH sample count is a 16-bit field, so a single
     * record cannot represent more than UINT16_MAX samples regardless of
     * the record length. */
    if (packer->formatversion == 2 && packer->maxsamples > UINT16_MAX)
    {
      packer->maxsamples = UINT16_MAX;
    }

    /* Allocate space for encoded data separately for alignment, reusing
     * the existing buffer if already large enough */
    if (packer->encoded_size < packer->maxdatabytes)
    {
      char *encoded = (char *)libmseed_memory.realloc (packer->encoded, packer->maxdatabytes);
      if (!encoded)
      {
        ms_log (2, "%s: Cannot allocate memory for encoded data buffer\n", msr->sid);
        return -1;
      }

      packer->encoded = encoded;
      packer->encoded_size = packer->maxdatabytes;
    }
  }

  packer->nextstarttime = msr->starttime;

  if (verbose > 2)
    ms_log (0, "%s: Initialized packing state for %s records\n", msr->sid,
            (packer->formatversion == 3) ? "miniSEED 3" : "miniSEED 2");

  return 0;
} /* End of lm_pack_state_init() */

/** ************************************************************************
 * @brief Generate next miniSEED record.
 *
 * Pack the next miniSEED record from the packing context initialized with
 * msr3_pack_init(). The record pointer and length are returned via the
 * @p record and @p reclen parameters. The record memory is owned by the
 * @p packer and will be reused or freed on subsequent calls.
 *
 * This function should be called repeatedly until it returns 0 (no more
 * records) or -1 (error).
 *
 * @param[in] packer ::MS3RecordPacker context
 * @param[out] record Pointer to record buffer (owned by packer)
 * @param[out] reclen Length of record in bytes
 *
 * @return 1 when a record is available, 0 when finished, and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 *
 * @see msr3_pack_init()
 * @see msr3_pack_free()
 ***************************************************************************/
int
msr3_pack_next (MS3RecordPacker *packer, char **record, int32_t *reclen)
{
  int64_t samples_packed;
  int64_t packoffset_bytes;
  uint64_t remaining_samples;
  uint32_t datalength;
  uint32_t reclen_generated;
  uint32_t crc;
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;
  uint16_t fsec;
  int8_t usec_offset;

  if (!packer || !record || !reclen)
  {
    ms_log (2, "%s(): Required input not defined\n", __func__);
    return -1;
  }

  if (packer->finished)
    return 0;

  /* Handle header-only records (no data samples) */
  if (packer->msr->numsamples <= 0)
  {
    if (packer->formatversion == 3)
    {
      /* Set encoding to text (value of 0) for consistency */
      *pMS3FSDH_ENCODING (packer->rawrec) = DE_TEXT;

      STORE4u (pMS3FSDH_NUMSAMPLES (packer->rawrec), 0, packer->swapflag);
      STORE4u (pMS3FSDH_DATALENGTH (packer->rawrec), 0, packer->swapflag);

      /* Calculate CRC and set */
      memset (pMS3FSDH_CRC (packer->rawrec), 0, sizeof (uint32_t));
      crc = ms_crc32c ((const uint8_t *)packer->rawrec, packer->dataoffset, 0);
      STORE4u (pMS3FSDH_CRC (packer->rawrec), crc, packer->swapflag);

      if (packer->verbose >= 1)
        ms_log (0, "%s: Packed %d byte record with no payload\n", packer->msr->sid,
                packer->dataoffset);

      *record = packer->rawrec;
      *reclen = packer->dataoffset;
    }
    else /* version 2 */
    {
      /* Set encoding to text (value of 0) for consistency */
      if (packer->blockette_1000_offset)
      {
        *pMS2B1000_ENCODING (packer->rawrec + packer->blockette_1000_offset) = DE_TEXT;
      }

      /* Data segment is not used; clear it */
      if (packer->dataoffset > 0 && (uint32_t)packer->dataoffset < packer->maxreclen)
        memset (packer->rawrec + packer->dataoffset, 0,
                (size_t)(packer->maxreclen - (uint32_t)packer->dataoffset));

      if (packer->verbose >= 1)
        ms_log (0, "%s: Packed %u byte record with no payload\n", packer->msr->sid,
                packer->maxreclen);

      *record = packer->rawrec;
      *reclen = packer->maxreclen;
    }

    packer->recordcount++;
    packer->finished = 1;
    return 1;
  }

  /* Calculate remaining samples */
  remaining_samples = packer->msr->numsamples - packer->packed_samples;

  /* Finished packing if all samples have been packed or less than max samples and not flushing */
  if ((remaining_samples == 0) ||
      (remaining_samples < packer->maxsamples && !(packer->flags & MSF_FLUSHDATA)))
  {
    packer->finished = 1;
    return 0;
  }

  /* Pack data samples */
  packoffset_bytes = packer->packed_samples * packer->samplesize;

  /* miniSEED 2 FSDH sample count is 16-bit; cap so excess spills to next records */
  uint64_t samples_to_pack = remaining_samples;
  if (packer->formatversion == 2 && samples_to_pack > UINT16_MAX)
    samples_to_pack = UINT16_MAX;

  samples_packed = msr_pack_data (
      packer->encoded, (uint8_t *)packer->msr->datasamples + packoffset_bytes, samples_to_pack,
      packer->maxdatabytes, packer->msr->sampletype, packer->encoding, packer->swapflag,
      &datalength, packer->msr->sid, packer->verbose);

  if (samples_packed < 0)
  {
    ms_log (2, "%s: Error packing data samples\n", packer->msr->sid);
    return -1;
  }
  /* Only proceed if samples were actually packed */
  else if (samples_packed == 0)
  {
    packer->finished = 1;
    return 0;
  }

  /* Copy encoded data into record */
  memcpy (packer->rawrec + packer->dataoffset, packer->encoded, datalength);

  /* Version 3 record */
  if (packer->formatversion == 3)
  {
    /* Calculate record length */
    reclen_generated = packer->dataoffset + datalength;

    /* Update number of samples and data length */
    STORE4u (pMS3FSDH_NUMSAMPLES (packer->rawrec), (uint32_t)samples_packed, packer->swapflag);
    STORE4u (pMS3FSDH_DATALENGTH (packer->rawrec), datalength, packer->swapflag);

    /* Update start time if not first record */
    if (packer->recordcount > 0)
    {
      if (ms_nstime2time (packer->nextstarttime, &year, &day, &hour, &min, &sec, &nsec))
      {
        ms_log (2, "%s: Cannot convert record starttime: %" PRId64 "\n", packer->msr->sid,
                packer->nextstarttime);
        return -1;
      }

      STORE4u (pMS3FSDH_NSEC (packer->rawrec), nsec, packer->swapflag);
      STORE2u (pMS3FSDH_YEAR (packer->rawrec), year, packer->swapflag);
      STORE2u (pMS3FSDH_DAY (packer->rawrec), day, packer->swapflag);
      *pMS3FSDH_HOUR (packer->rawrec) = hour;
      *pMS3FSDH_MIN (packer->rawrec) = min;
      *pMS3FSDH_SEC (packer->rawrec) = sec;
    }

    /* Calculate CRC and set */
    memset (pMS3FSDH_CRC (packer->rawrec), 0, sizeof (uint32_t));
    crc = ms_crc32c ((const uint8_t *)packer->rawrec, reclen_generated, 0);
    STORE4u (pMS3FSDH_CRC (packer->rawrec), crc, packer->swapflag);
  }
  /* Version 2 record */
  else
  {
    /* V2 records are always full length */
    reclen_generated = packer->maxreclen;

    /* Update number of samples */
    STORE2u (pMS2FSDH_NUMSAMPLES (packer->rawrec), (uint16_t)samples_packed, packer->swapflag);

    /* Zero any space between encoded data and end of record */
    uint32_t content = packer->dataoffset + datalength;
    if (content < packer->maxreclen)
      memset (packer->rawrec + content, 0, packer->maxreclen - content);

    /* Update start time if not first record */
    if (packer->recordcount > 0)
    {
      nstime_t second_nstime = nstime2fsec_usec_offset (packer->nextstarttime, &fsec, &usec_offset);

      /* Use the (possibly carried) second-resolution time so Y/D/H/M/S stay
       * consistent with fsec/usec_offset when rounding carries into the next second */
      if (second_nstime == NSTERROR ||
          ms_nstime2time (second_nstime, &year, &day, &hour, &min, &sec, NULL))
      {
        ms_log (2, "%s: Cannot convert record starttime: %" PRId64 "\n", packer->msr->sid,
                packer->nextstarttime);
        return -1;
      }

      STORE2u (pMS2FSDH_YEAR (packer->rawrec), year, packer->swapflag);
      STORE2u (pMS2FSDH_DAY (packer->rawrec), day, packer->swapflag);
      *pMS2FSDH_HOUR (packer->rawrec) = hour;
      *pMS2FSDH_MIN (packer->rawrec) = min;
      *pMS2FSDH_SEC (packer->rawrec) = sec;
      STORE2u (pMS2FSDH_FSEC (packer->rawrec), fsec, packer->swapflag);

      if (packer->blockette_1001_offset)
      {
        *pMS2B1001_MICROSECOND (packer->rawrec + packer->blockette_1001_offset) = usec_offset;
      }
    }
  }

  if (packer->verbose >= 1)
    ms_log (0, "%s: Packed %" PRId64 " samples into %u byte record\n", packer->msr->sid,
            samples_packed, reclen_generated);

  *record = packer->rawrec;
  *reclen = reclen_generated;

  packer->packed_samples += samples_packed;
  packer->recordcount++;

  /* Calculate start time for next record */
  if (packer->packed_samples < packer->msr->numsamples)
  {
    packer->nextstarttime =
        ms_sampletime (packer->msr->starttime, packer->packed_samples, packer->msr->samprate);
  }
  else
  {
    packer->finished = 1;
  }

  return 1;
} /* End of msr3_pack_next() */

/** ************************************************************************
 * @brief Free packer and resources
 *
 * Free all memory associated with the ::MS3RecordPacker and set the
 * pointer to NULL.
 *
 * @param[in] packer Pointer to ::MS3RecordPacker pointer
 * @param[out] packedsamples Total number of samples packed, returned to caller
 *
 * @see msr3_pack_init()
 * @see msr3_pack_next()
 ***************************************************************************/
void
msr3_pack_free (MS3RecordPacker **packer, int64_t *packedsamples)
{
  if (!packer || !*packer)
    return;

  lm_pack_state_finish (*packer, packedsamples);
  lm_pack_state_free (*packer);

  libmseed_memory.free (*packer);
  *packer = NULL;
} /* End of msr3_pack_free() */

/***************************************************************************
 * Report the total samples packed by a session and end it.  The packer's
 * rawrec/encoded buffers are retained for reuse by a later
 * lm_pack_state_init() call.
 ***************************************************************************/
void
lm_pack_state_finish (MS3RecordPacker *packer, int64_t *packedsamples)
{
  if (!packer)
    return;

  if (packedsamples)
    *packedsamples = packer->packed_samples;

  packer->finished = 1;
  packer->msr = NULL;
} /* End of lm_pack_state_finish() */

/***************************************************************************
 * Release a packer's rawrec/encoded buffers.
 ***************************************************************************/
void
lm_pack_state_free (MS3RecordPacker *packer)
{
  if (!packer)
    return;

  if (packer->rawrec)
    libmseed_memory.free (packer->rawrec);

  if (packer->encoded)
    libmseed_memory.free (packer->encoded);

  packer->rawrec = NULL;
  packer->rawrec_size = 0;
  packer->encoded = NULL;
  packer->encoded_size = 0;
} /* End of lm_pack_state_free() */

/***************************************************************************
 * Convert little endian Steim frames in place to big endian.
 *
 * Little endian Steim swaps each difference within its 32-bit word, so
 * the swapping of each word depends on its nibble:
 *   W0, X0 and Xn: 32-bit
 *   nibble 01: none (8-bit differences)
 *   nibble 10: Steim1 16-bit halves, Steim2 32-bit
 *   nibble 11: 32-bit
 ***************************************************************************/
static void
repack_steim_to_bigendian (char *data, uint32_t datasize, int8_t encoding)
{
  uint8_t *frame;
  uint32_t frameidx;
  uint32_t nibbles;
  int widx;

  for (frameidx = 0; (frameidx + 1) * 64 <= datasize; frameidx++)
  {
    frame = (uint8_t *)data + (frameidx * 64);

    ms_gswap4 (frame);
    nibbles = ((uint32_t)frame[0] << 24) | ((uint32_t)frame[1] << 16) | ((uint32_t)frame[2] << 8) |
              (uint32_t)frame[3];

    /* X0 and Xn in first frame */
    if (frameidx == 0)
    {
      ms_gswap4 (frame + 4);
      ms_gswap4 (frame + 8);
    }

    for (widx = (frameidx == 0) ? 3 : 1; widx < 16; widx++)
    {
      switch ((nibbles >> (30 - (2 * widx))) & 0x3)
      {
      case 2:
        if (encoding == DE_STEIM1)
        {
          ms_gswap2 (frame + (4 * widx));
          ms_gswap2 (frame + (4 * widx) + 2);
        }
        else
        {
          ms_gswap4 (frame + (4 * widx));
        }
        break;
      case 3:
        ms_gswap4 (frame + (4 * widx));
        break;
      }
    }
  }
} /* End of repack_steim_to_bigendian() */

/***************************************************************************
 * Convert encoded data in place from the byte order of the source record
 * to the target byte order.  Steim targets are always big endian.
 *
 * Only encodings defined by miniSEED 3 are converted.  Text, retired and
 * unrecognized encodings are left unchanged.
 ***************************************************************************/
static void
repack_payload_order (const MS3Record *msr, char *data, uint32_t datasize, int8_t target_bigendian)
{
  uint32_t wordsize;
  uint32_t idx;
  int8_t source_bigendian;

  source_bigendian = (ms_bigendianhost () ^ ((msr->swapflag & MSSWAP_PAYLOAD) != 0)) ? 1 : 0;

  if (source_bigendian == (target_bigendian ? 1 : 0))
    return;

  switch (msr->encoding)
  {
  case DE_INT16:
    wordsize = 2;
    break;
  case DE_INT32:
  case DE_FLOAT32:
    wordsize = 4;
    break;
  case DE_FLOAT64:
    wordsize = 8;
    break;
  case DE_STEIM1:
  case DE_STEIM2:
    if (!source_bigendian)
      repack_steim_to_bigendian (data, datasize, msr->encoding);
    return;
  default:
    return;
  }

  for (idx = 0; idx + wordsize <= datasize; idx += wordsize)
  {
    if (wordsize == 2)
      ms_gswap2 (data + idx);
    else if (wordsize == 4)
      ms_gswap4 (data + idx);
    else
      ms_gswap8 (data + idx);
  }
} /* End of repack_payload_order() */

/** ************************************************************************
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
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_repack_mseed3 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int dataoffset;
  uint32_t origdataoffset;
  uint32_t origdatasize;
  uint32_t crc;
  uint32_t reclen;
  int8_t swapflag;

  if (!msr || !msr->record || !record)
  {
    ms_log (2, "%s(): Required input not defined: 'msr', 'msr->record', or 'record'\n", __func__);
    return -1;
  }

  if (recbuflen < (MS3FSDH_LENGTH + strlen (msr->sid) + msr->extralength))
  {
    ms_log (2,
            "%s: Record buffer length (%u) is not large enough for header (%u), SID (%" PRIsize_t
            "), and extra (%d)\n",
            msr->sid, recbuflen, MS3FSDH_LENGTH, strlen (msr->sid), msr->extralength);
    return -1;
  }

  if (msr->samplecnt > UINT32_MAX)
  {
    ms_log (2, "%s: Too many samples in input record (%" PRId64 ") for a single record)\n",
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

  if (recbuflen < ((uint32_t)dataoffset + origdatasize))
  {
    ms_log (2, "%s: Destination record buffer length (%u) is not large enough for record (%u)\n",
            msr->sid, recbuflen, ((uint32_t)dataoffset + origdatasize));
    return -1;
  }

  reclen = dataoffset + origdatasize;

  /* Copy encoded data into record, Steim is big endian, all others little endian */
  memcpy (record + dataoffset, msr->record + origdataoffset, origdatasize);
  repack_payload_order (msr, record + dataoffset, origdatasize,
                        (msr->encoding == DE_STEIM1 || msr->encoding == DE_STEIM2));

  /* Check to see if byte swapping is needed, miniSEED 3 is little endian */
  swapflag = (ms_bigendianhost ()) ? 1 : 0;

  /* Update number of samples and data length */
  STORE4u (pMS3FSDH_NUMSAMPLES (record), (uint32_t)msr->samplecnt, swapflag);
  STORE4u (pMS3FSDH_DATALENGTH (record), origdatasize, swapflag);

  /* Calculate CRC (with CRC field set to 0) and set */
  memset (pMS3FSDH_CRC (record), 0, sizeof (uint32_t));
  crc = ms_crc32c ((const uint8_t *)record, reclen, 0);
  STORE4u (pMS3FSDH_CRC (record), crc, swapflag);

  if (verbose >= 1)
    ms_log (0, "%s: Repacked %" PRId64 " samples into a %u byte record\n", msr->sid, msr->samplecnt,
            reclen);

  return reclen;
} /* End of msr3_repack_mseed3() */

/** ************************************************************************
 * @brief Pack a miniSEED version 3 header into the specified buffer.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when @p msr.reclen and @p msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record to pack
 * @param[out] record Destination for packed header
 * @param[in] recbuflen Length of destination buffer
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the size of the header (fixed and extra) on success, otherwise -1.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_header3 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int extraoffset = 0;
  size_t sidlength;
  uint32_t maxreclen;
  uint8_t encoding;
  int8_t swapflag;

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t nsec;

  if (!msr || !record)
  {
    ms_log (2, "%s(): Required input not defined: 'msr' or 'record'\n", __func__);
    return -1;
  }

  /* Use default record length and encoding if needed */
  maxreclen = (msr->reclen < 0) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding < 0) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (maxreclen < MINRECLEN || maxreclen > MAXRECLEN)
  {
    ms_log (2, "%s: Record length is out of range: %d\n", msr->sid, maxreclen);
    return -1;
  }

  sidlength = strlen (msr->sid);

  if (recbuflen < (uint32_t)(MS3FSDH_LENGTH + sidlength + msr->extralength))
  {
    ms_log (2,
            "%s: Buffer length (%d) is not large enough for fixed header (%d), SID (%" PRIsize_t
            "), and extra (%d)\n",
            msr->sid, recbuflen, MS3FSDH_LENGTH, sidlength, msr->extralength);
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
    ms_log (2, "%s: Source ID too long: %" PRIsize_t " bytes\n", msr->sid, sidlength);
    return -1;
  }

  extraoffset = MS3FSDH_LENGTH + (int)sidlength;

  /* A NULL extra pointer means no extra headers regardless of extralength */
  uint16_t extralength = (msr->extra) ? msr->extralength : 0;

  /* Build fixed header */
  record[0] = 'M';
  record[1] = 'S';
  *pMS3FSDH_FORMATVERSION (record) = 3;
  *pMS3FSDH_FLAGS (record) = msr->flags;
  STORE4u (pMS3FSDH_NSEC (record), nsec, swapflag);
  STORE2u (pMS3FSDH_YEAR (record), year, swapflag);
  STORE2u (pMS3FSDH_DAY (record), day, swapflag);
  *pMS3FSDH_HOUR (record) = hour;
  *pMS3FSDH_MIN (record) = min;
  *pMS3FSDH_SEC (record) = sec;
  *pMS3FSDH_ENCODING (record) = encoding;

  /* If rate positive and less than one, convert to period notation */
  if (msr->samprate != 0.0 && msr->samprate > 0 && msr->samprate < 1.0)
    STORE8f (pMS3FSDH_SAMPLERATE (record), (-1.0 / msr->samprate), swapflag);
  else
    STORE8f (pMS3FSDH_SAMPLERATE (record), msr->samprate, swapflag);

  *pMS3FSDH_PUBVERSION (record) = msr->pubversion;
  *pMS3FSDH_SIDLENGTH (record) = (uint8_t)sidlength;
  STORE2u (pMS3FSDH_EXTRALENGTH (record), extralength, swapflag);
  memcpy (pMS3FSDH_SID (record), msr->sid, sidlength);

  if (extralength > 0)
    memcpy (record + extraoffset, msr->extra, extralength);

  return (MS3FSDH_LENGTH + (int)sidlength + extralength);
} /* End of msr3_pack_header3() */

/** ************************************************************************
 * @brief Repack a parsed miniSEED record into a version 2 record.
 *
 * Pack the parsed header into a version 2 header and copy the raw
 * encoded data from the original record.  The original record must be
 * available at the ::MS3Record.record pointer.
 *
 * The new record will be the same length as the original record and an
 * error will be returned if the repacked record would not fit.
 * If the new record is shorter than the original record, the extra space
 * will be zeroed.
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
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_repack_mseed2 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  int headerlen;
  uint32_t dataoffset;
  uint32_t origdataoffset;
  uint32_t origdatasize;
  uint32_t totalsize;
  uint8_t swapflag;

  if (!msr || !msr->record || !record)
  {
    ms_log (2, "%s(): Required input not defined: 'msr', 'msr->record', or 'record'\n", __func__);
    return -1;
  }

  if (msr->reclen <= 0)
  {
    ms_log (2, "%s: Record length (%d) is not set to a valid value\n", msr->sid, msr->reclen);
    return -1;
  }

  if ((int64_t)recbuflen < msr->reclen)
  {
    ms_log (2,
            "%s: Record buffer length (%u) is not large enough for requested record length (%d)\n",
            msr->sid, recbuflen, msr->reclen);
    return -1;
  }

  if (msr->samplecnt > UINT16_MAX)
  {
    ms_log (2, "%s: Too many samples in input record (%" PRId64 ") for a single v2 record)\n",
            msr->sid, msr->samplecnt);
    return -1;
  }

  /* Pack fixed header and blockettes */
  headerlen = msr3_pack_header2 (msr, record, recbuflen, verbose);

  if (headerlen < 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED version 2 header\n", msr->sid);
    return -1;
  }

  /* Determine encoded data size */
  if (msr3_data_bounds (msr, &origdataoffset, &origdatasize))
  {
    ms_log (2, "%s: Cannot determine original data bounds\n", msr->sid);
    return -1;
  }

  /* Determine offset to encoded data, rounded up to a 64-byte boundary for
   * Steim encodings, and confirm it fits the 16-bit v2 data-offset field */
  if (msr->encoding == DE_STEIM1 || msr->encoding == DE_STEIM2)
    dataoffset = ((uint32_t)headerlen + 63) / 64 * 64;
  else
    dataoffset = (uint32_t)headerlen;

  if (dataoffset > UINT16_MAX || dataoffset >= recbuflen)
  {
    ms_log (2, "%s: Repacked data offset (%u) does not fit within record length (%u)\n", msr->sid,
            dataoffset, recbuflen);
    return -1;
  }

  totalsize = dataoffset + origdatasize;

  if (totalsize > (uint32_t)msr->reclen)
  {
    ms_log (2, "%s: Repacked record length (%u) is larger than the original record length (%d)\n",
            msr->sid, totalsize, msr->reclen);
    return -1;
  }

  /* Zero memory between blockettes and data if any */
  if (dataoffset > (uint32_t)headerlen)
    memset (record + headerlen, 0, dataoffset - headerlen);

  /* Copy encoded data into record, big endian as declared in Blockette 1000 */
  memcpy (record + dataoffset, msr->record + origdataoffset, origdatasize);
  repack_payload_order (msr, record + dataoffset, origdatasize, 1);

  /* Check if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  /* Update number of samples and data offset */
  STORE2u (pMS2FSDH_NUMSAMPLES (record), (uint16_t)msr->samplecnt, swapflag);
  STORE2u (pMS2FSDH_DATAOFFSET (record), (uint16_t)dataoffset, swapflag);

  /* Zero any space between encoded data and end of record.  totalsize is the
   * end of the encoded data (dataoffset + origdatasize), computed above. */
  if (totalsize < (uint32_t)msr->reclen)
    memset (record + totalsize, 0, (uint32_t)msr->reclen - totalsize);

  if (verbose >= 1)
    ms_log (0, "%s: Repacked %" PRId64 " samples into a %u byte record\n", msr->sid, msr->samplecnt,
            msr->reclen);

  return msr->reclen;
} /* End of msr3_repack_mseed2() */

/** ************************************************************************
 * @brief Pack a miniSEED version 2 header into the specified buffer.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when @p msr.reclen and @p msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record to pack
 * @param[out] record Destination for packed header
 * @param[in] recbuflen Length of destination buffer
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the size of the header (fixed and blockettes) on success, otherwise -1.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
msr3_pack_header2 (const MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose)
{
  return msr3_pack_header2_offsets (msr, record, recbuflen, NULL, NULL, verbose);
}

/** ************************************************************************
 * @internal
 * Pack a miniSEED version 2 header into the specified buffer and return the
 * offsets of the 1000 and 1001 blockettes.
 *
 * Default values are: record length = 4096, encoding = 11 (Steim2).
 * The defaults are triggered when @p msr.reclen and @p msr.encoding
 * are set to -1.
 *
 * @param[in] msr ::MS3Record to pack
 * @param[out] record Destination for packed header
 * @param[in] recbuflen Length of destination buffer
 * @param[out] blockette_1000_offset Byte offset to B1000, 0 if none
 * @param[out] blockette_1001_offset Byte offset to B1001, 0 if none
 * @param[in] verbose Controls logging verbosity, 0 is no diagnostic output
 *
 * @returns the size of the header (fixed and blockettes) on success, otherwise -1.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static int
msr3_pack_header2_offsets (const MS3Record *msr, char *record, uint32_t recbuflen,
                           uint16_t *blockette_1000_offset, uint16_t *blockette_1001_offset,
                           int8_t verbose)
{
  int written = 0;
  int8_t swapflag;
  uint32_t reclen;
  uint8_t encoding;

  char network[64];
  char station[64];
  char location[64];
  char channel[64];

  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint16_t fsec;
  int8_t usec_offset;

  uint32_t reclenexp = 0;
  uint32_t reclenfind;
  int16_t factor;
  int16_t multiplier;
  void *next_blockette = NULL;

  yyjson_doc *ehdoc = NULL;
  yyjson_val *ehroot = NULL;
  yyjson_read_flag flg = YYJSON_READ_NOFLAG;
  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};
  yyjson_read_err err;
  yyjson_val *eharr;
  yyjson_arr_iter ehiter;
  yyjson_val *ehiterval;
  const char *header_string;
  double header_number;
  uint64_t header_uint;
  uint32_t header_u32;
  float header_float;
  bool header_boolean;

  uint16_t blockette_type;
  uint16_t blockette_length;

  if (!msr || !record)
  {
    ms_log (2, "%s(): Required input not defined: 'msr' or 'record'\n", __func__);
    return -1;
  }

  /* Initialize blockette offsets to 0 */
  if (blockette_1000_offset)
    *blockette_1000_offset = 0;
  if (blockette_1001_offset)
    *blockette_1001_offset = 0;

  /* Use default record length and encoding if needed */
  reclen = (msr->reclen < 0) ? MS_PACK_DEFAULT_RECLEN : msr->reclen;
  encoding = (msr->encoding < 0) ? MS_PACK_DEFAULT_ENCODING : msr->encoding;

  if (reclen < 128 || reclen > MAXRECLEN)
  {
    ms_log (2, "%s: Record length is out of range: %u\n", msr->sid, reclen);
    return -1;
  }

  /* Check that record length is a power of 2.
   * Power of two if (X & (X - 1)) == 0 */
  if ((reclen & (reclen - 1)) != 0)
  {
    ms_log (2, "%s: Cannot pack miniSEED 2, record length (%u) is not a power of 2\n", msr->sid,
            reclen);
    return -1;
  }

  /* Ensure the supplied buffer can hold the fixed header and mandatory
   * Blockette 1000 that are written unconditionally below */
  if (recbuflen < (uint32_t)(MS2FSDH_LENGTH + 8))
  {
    ms_log (2, "%s: Buffer length (%u) is not large enough for fixed header and Blockette 1000\n",
            msr->sid, recbuflen);
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
  if (ms_sid2nslc_n (msr->sid, network, sizeof (network), station, sizeof (station), location,
                     sizeof (location), channel, sizeof (channel)))
  {
    ms_log (2, "%s: Cannot parse SEED identifier codes from full identifier\n", msr->sid);
    return -1;
  }

  /* Verify that identifier codes will fit into and are appropriate for miniSEED 2 */
  if (strlen (network) > 2 || strlen (station) > 5 || strlen (location) > 2 ||
      strlen (channel) != 3)
  {
    ms_log (2, "%s: Cannot create miniSEED 2 for N,S,L,C codes: %s, %s, %s, %s\n", msr->sid,
            network, station, location, channel);
    return -1;
  }

  /* Check to see if byte swapping is needed, miniSEED 2 is written big endian */
  swapflag = (ms_bigendianhost ()) ? 0 : 1;

  if (verbose > 2 && swapflag)
    ms_log (0, "%s: Byte swapping needed for packing of header\n", msr->sid);

  /* Calculate time at fractional 100usec resolution and microsecond offset */
  nstime_t second_nstime = nstime2fsec_usec_offset (msr->starttime, &fsec, &usec_offset);

  /* Break down start time into individual components */
  if (ms_nstime2time (second_nstime, &year, &day, &hour, &min, &sec, NULL))
  {
    ms_log (2, "%s: Cannot convert starttime: %" PRId64 "\n", msr->sid, msr->starttime);
    return -1;
  }

  /* Warn if outside the conventional year range assumed by many readers */
  if (year < 1900 || year > 2100)
    ms_log (1, "%s: Unlikely start year for miniSEED 2 (1900-2100): %d\n", msr->sid, year);

  /* Generate factor & multipler representation of sample rate */
  if (ms_genfactmult (msr3_sampratehz (msr), &factor, &multiplier))
  {
    ms_log (2, "%s: Cannot convert sample rate (%g) to factor and multiplier\n", msr->sid,
            msr->samprate);
    return -1;
  }

  /* Parse extra headers if present */
  if (msr->extra && msr->extralength > 0)
  {
    ehdoc = yyjson_read_opts (msr->extra, msr->extralength, flg, &alc, &err);

    if (!ehdoc)
    {
      ms_log (2, "%s() Cannot parse extra header JSON: %s\n", __func__,
              (err.msg) ? err.msg : "Unknown error");
      return -1;
    }

    ehroot = yyjson_doc_get_root (ehdoc);
  }

  /* Build fixed header */

  /* Use sequence number from extra headers if present */
  if (yyjson_ptr_get_uint (ehroot, "/FDSN/Sequence", &header_uint))
  {
    if (header_uint <= 999999)
    {
      char seqnum[7];
      snprintf (seqnum, sizeof (seqnum), "%06" PRIu64, header_uint);
      memcpy (pMS2FSDH_SEQNUM (record), seqnum, 6);
    }
    else
    {
      memcpy (pMS2FSDH_SEQNUM (record), "999999", 6);
    }
  }
  else
  {
    memcpy (pMS2FSDH_SEQNUM (record), "000000", 6);
  }

  /* Use DataQuality indicator in extra headers if present */
  if (yyjson_ptr_get_str (ehroot, "/FDSN/DataQuality", &header_string) &&
      MS2_ISDATAINDICATOR (header_string[0]))
  {
    *pMS2FSDH_DATAQUALITY (record) = header_string[0];
  }
  /* Otherwise map publication version, defaulting to 'D' */
  else
  {
    switch (msr->pubversion)
    {
    case 1:
      *pMS2FSDH_DATAQUALITY (record) = 'R';
      break;
    case 3:
      *pMS2FSDH_DATAQUALITY (record) = 'Q';
      break;
    case 4:
      *pMS2FSDH_DATAQUALITY (record) = 'M';
      break;
    default:
      *pMS2FSDH_DATAQUALITY (record) = 'D';
      break;
    }
  }

  *pMS2FSDH_RESERVED (record) = ' ';
  ms_strncpopen (pMS2FSDH_STATION (record), station, 5);
  ms_strncpopen (pMS2FSDH_LOCATION (record), location, 2);
  ms_strncpopen (pMS2FSDH_CHANNEL (record), channel, 3);
  ms_strncpopen (pMS2FSDH_NETWORK (record), network, 2);

  STORE2u (pMS2FSDH_YEAR (record), year, swapflag);
  STORE2u (pMS2FSDH_DAY (record), day, swapflag);
  *pMS2FSDH_HOUR (record) = hour;
  *pMS2FSDH_MIN (record) = min;
  *pMS2FSDH_SEC (record) = sec;
  *pMS2FSDH_UNUSED (record) = 0;
  STORE2u (pMS2FSDH_FSEC (record), fsec, swapflag);
  STORE2u (pMS2FSDH_NUMSAMPLES (record), 0, 0);

  STORE2d (pMS2FSDH_SAMPLERATEFACT (record), factor, swapflag);
  STORE2d (pMS2FSDH_SAMPLERATEMULT (record), multiplier, swapflag);

  /* Map activity bit flags */
  *pMS2FSDH_ACTFLAGS (record) = 0;
  if (msr->flags & 0x01) /* Bit 0 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x01;

  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/Begin", &header_boolean) &&
      header_boolean) /* Bit 2 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/End", &header_boolean) &&
      header_boolean) /* Bit 3 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x08;

  if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/LeapSecond", &header_number))
  {
    if (header_number > 0) /* Bit 4 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x10;
    else if (header_number < 0) /* Bit 5 */
      *pMS2FSDH_ACTFLAGS (record) |= 0x20;
  }

  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Event/InProgress", &header_boolean) &&
      header_boolean) /* Bit 6 */
    *pMS2FSDH_ACTFLAGS (record) |= 0x40;

  /* Map I/O and clock bit flags */
  *pMS2FSDH_IOFLAGS (record) = 0;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/StationVolumeParityError", &header_boolean) &&
      header_boolean) /* Bit 0 */
    *pMS2FSDH_IOFLAGS (record) |= 0x01;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/LongRecordRead", &header_boolean) &&
      header_boolean) /* Bit 1 */
    *pMS2FSDH_IOFLAGS (record) |= 0x02;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/ShortRecordRead", &header_boolean) &&
      header_boolean) /* Bit 2 */
    *pMS2FSDH_IOFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/StartOfTimeSeries", &header_boolean) &&
      header_boolean) /* Bit 3 */
    *pMS2FSDH_IOFLAGS (record) |= 0x08;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/EndOfTimeSeries", &header_boolean) &&
      header_boolean) /* Bit 4 */
    *pMS2FSDH_IOFLAGS (record) |= 0x10;
  if (msr->flags & 0x04) /* Bit 5 */
    *pMS2FSDH_IOFLAGS (record) |= 0x20;

  /* Map data quality bit flags */
  *pMS2FSDH_DQFLAGS (record) = 0;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/AmplifierSaturation", &header_boolean) &&
      header_boolean) /* Bit 0 */
    *pMS2FSDH_DQFLAGS (record) |= 0x01;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/DigitizerClipping", &header_boolean) &&
      header_boolean) /* Bit 1 */
    *pMS2FSDH_DQFLAGS (record) |= 0x02;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/Spikes", &header_boolean) &&
      header_boolean) /* Bit 2 */
    *pMS2FSDH_DQFLAGS (record) |= 0x04;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/Glitches", &header_boolean) &&
      header_boolean) /* Bit 3 */
    *pMS2FSDH_DQFLAGS (record) |= 0x08;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/MissingData", &header_boolean) &&
      header_boolean) /* Bit 4 */
    *pMS2FSDH_DQFLAGS (record) |= 0x10;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/TelemetrySyncError", &header_boolean) &&
      header_boolean) /* Bit 5 */
    *pMS2FSDH_DQFLAGS (record) |= 0x20;
  if (yyjson_ptr_get_bool (ehroot, "/FDSN/Flags/FilterCharging", &header_boolean) &&
      header_boolean) /* Bit 6 */
    *pMS2FSDH_DQFLAGS (record) |= 0x40;
  if (msr->flags & 0x02) /* Bit 7 */
    *pMS2FSDH_DQFLAGS (record) |= 0x80;

  if (yyjson_ptr_get_num (ehroot, "/FDSN/Time/Correction", &header_number))
  {
    /* Time correction is stored in 0.0001 second units, round-to-nearest w/ ties away from zero */
    double correction = header_number * 10000.0;

    correction += (correction < 0.0) ? -0.5 : 0.5;

    if (!isfinite (correction) || correction <= (double)INT32_MIN - 1.0 ||
        correction >= (double)INT32_MAX + 1.0)
    {
      ms_log (2, "%s: Time correction is not representable: %g\n", msr->sid, header_number);
      yyjson_doc_free (ehdoc);
      return -1;
    }

    STORE4d (pMS2FSDH_TIMECORRECT (record), (int32_t)correction, swapflag);

    /* Set time correction applied bit in activity flags.
       Rationale: V3 records do not allow unapplied time corrections and unapplied
       time corrections in V2 records are always applied on read by this library. */
    *pMS2FSDH_ACTFLAGS (record) |= 0x02;
  }
  else
  {
    STORE4d (pMS2FSDH_TIMECORRECT (record), 0, 0);
  }

  *pMS2FSDH_NUMBLOCKETTES (record) = 1;
  STORE2u (pMS2FSDH_DATAOFFSET (record), 0, 0);
  STORE2u (pMS2FSDH_BLOCKETTEOFFSET (record), 48, swapflag);

  written = 48;

  /* Add mandatory Blockette 1000 */
  next_blockette = pMS2B1000_NEXT (record + written);

  STORE2u (pMS2B1000_TYPE (record + written), 1000, swapflag);
  STORE2u (pMS2B1000_NEXT (record + written), 0, 0);
  *pMS2B1000_ENCODING (record + written) = encoding;
  *pMS2B1000_BYTEORDER (record + written) = 1;
  *pMS2B1000_RECLEN (record + written) = reclenexp;
  *pMS2B1000_RESERVED (record + written) = 0;

  if (blockette_1000_offset)
    *blockette_1000_offset = written;

  written += 8;

  /* Add Blockette 1001 if microsecond offset or timing quality is present on this
   * record, or if the sample period is not an integer number of 100 usec units,
   * in which case continuation records may need a microsecond offset even
   * though this record does not. */
  double sampratehz = msr3_sampratehz (msr);
  int8_t offgrid_possible = 0;

  if (sampratehz > 0.0)
  {
    double units = 10000.0 / sampratehz; /* 100 usec units per sample */

    /* Test for a fractional part, values of 2^53 and larger are always integers */
    if (units < 9.0e15)
    {
      double fraction = units - (double)(int64_t)units;

      offgrid_possible = (fraction > 1.0e-6 && fraction < (1.0 - 1.0e-6));
    }
  }

  if (yyjson_ptr_get_uint (ehroot, "/FDSN/Time/Quality", &header_uint) || usec_offset ||
      offgrid_possible)
  {
    if (written > UINT16_MAX || (uint64_t)written + 8 > recbuflen)
    {
      ms_log (2, "%s: Record length not large enough for B1001\n", msr->sid);
      yyjson_doc_free (ehdoc);
      return -1;
    }

    STORE2u (next_blockette, (uint16_t)written, swapflag);
    next_blockette = pMS2B1001_NEXT (record + written);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;

    STORE2u (pMS2B1001_TYPE (record + written), 1001, swapflag);
    STORE2u (pMS2B1001_NEXT (record + written), 0, 0);

    if (yyjson_ptr_get_uint (ehroot, "/FDSN/Time/Quality", &header_uint) &&
        header_uint <= UINT8_MAX)
      *pMS2B1001_TIMINGQUALITY (record + written) = (uint8_t)(header_uint);
    else
      *pMS2B1001_TIMINGQUALITY (record + written) = 0;

    *pMS2B1001_MICROSECOND (record + written) = usec_offset;
    *pMS2B1001_RESERVED (record + written) = 0;
    *pMS2B1001_FRAMECOUNT (record + written) = 0;

    if (blockette_1001_offset)
      *blockette_1001_offset = written;

    written += 8;
  }

  /* Add Blockette 100 if sample rate is not well represented by factor/multiplier */
  if (fabs (msr3_sampratehz (msr) - ms_nomsamprate (factor, multiplier)) > 0.0001)
  {
    if (written > UINT16_MAX || (uint64_t)written + 12 > recbuflen)
    {
      ms_log (2, "%s: Record length not large enough for B100\n", msr->sid);
      yyjson_doc_free (ehdoc);
      return -1;
    }

    STORE2u (next_blockette, (uint16_t)written, swapflag);
    next_blockette = pMS2B100_NEXT (record + written);
    *pMS2FSDH_NUMBLOCKETTES (record) += 1;

    STORE2u (pMS2B100_TYPE (record + written), 100, swapflag);
    STORE2u (pMS2B100_NEXT (record + written), 0, 0);
    STORE4f (pMS2B100_SAMPRATE (record + written), (float)sampratehz, swapflag);
    *pMS2B100_FLAGS (record + written) = 0;
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

      if (written > UINT16_MAX || (uint64_t)written + blockette_length > recbuflen)
      {
        ms_log (2, "%s: Record length not large enough for B500\n", msr->sid);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      STORE2u (next_blockette, (uint16_t)written, swapflag);
      next_blockette = pMS2B500_NEXT (record + written);
      *pMS2FSDH_NUMBLOCKETTES (record) += 1;

      memset (record + written, 0, blockette_length);
      STORE2u (pMS2B500_TYPE (record + written), 500, swapflag);
      STORE2u (pMS2B500_NEXT (record + written), 0, 0);

      if (yyjson_ptr_get_num (ehiterval, "/VCOCorrection", &header_number))
      {
        if (lm_real2float (header_number, &header_float, msr->sid, "/VCOCorrection"))
          goto error_return;
        STORE4f (pMS2B500_VCOCORRECTION (record + written), header_float, swapflag);
      }

      if (yyjson_ptr_get_str (ehiterval, "/Time", &header_string))
      {
        int8_t l_usec_offset;
        nstime_t l_nstime =
            ms_timestr2btime (header_string, (uint8_t *)pMS2B500_YEAR (record + written),
                              &l_usec_offset, msr->sid, swapflag);

        if (l_nstime == NSTERROR)
        {
          ms_log (2, "%s: Cannot convert B500 time: %s\n", msr->sid, header_string);
          yyjson_doc_free (ehdoc);
          return -1;
        }

        *pMS2B500_MICROSECOND (record + written) = l_usec_offset;
      }

      if (yyjson_ptr_get_uint (ehiterval, "/ReceptionQuality", &header_uint) &&
          header_uint <= UINT8_MAX)
        *pMS2B500_RECEPTIONQUALITY (record + written) = (uint8_t)header_uint;

      if (yyjson_ptr_get_uint (ehiterval, "/Count", &header_uint) && header_uint <= UINT32_MAX)
        STORE4u (pMS2B500_EXCEPTIONCOUNT (record + written), (uint32_t)header_uint, swapflag);

      if (yyjson_ptr_get_str (ehiterval, "/Type", &header_string))
        ms_strncpopen (pMS2B500_EXCEPTIONTYPE (record + written), header_string, 16);

      if (yyjson_ptr_get_str (ehroot, "/FDSN/Clock/Model", &header_string))
        ms_strncpopen (pMS2B500_CLOCKMODEL (record + written), header_string, 32);

      if (yyjson_ptr_get_str (ehiterval, "/ClockStatus", &header_string))
        ms_strncpopen (pMS2B500_CLOCKSTATUS (record + written), header_string, 128);

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
      if (yyjson_ptr_get_str (ehiterval, "/Type", &header_string) &&
          lmp_strncasecmp (header_string, "MURDOCK", 7) == 0)
      {
        blockette_type = 201;
        blockette_length = 60;
      }
      else
      {
        blockette_type = 200;
        blockette_length = 52;
      }

      if (written > UINT16_MAX || (uint64_t)written + blockette_length > recbuflen)
      {
        ms_log (2, "%s: Record length not large enough for B%u\n", msr->sid, blockette_type);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      /* The initial fields of B200 and B201 are the same */
      STORE2u (next_blockette, (uint16_t)written, swapflag);
      next_blockette = pMS2B200_NEXT (record + written);
      *pMS2FSDH_NUMBLOCKETTES (record) += 1;

      memset (record + written, 0, blockette_length);
      STORE2u (pMS2B200_TYPE (record + written), blockette_type, swapflag);
      STORE2u (pMS2B200_NEXT (record + written), 0, 0);

      if (yyjson_ptr_get_num (ehiterval, "/SignalAmplitude", &header_number))
      {
        if (lm_real2float (header_number, &header_float, msr->sid, "/SignalAmplitude"))
          goto error_return;
        STORE4f (pMS2B200_AMPLITUDE (record + written), header_float, swapflag);
      }

      if (yyjson_ptr_get_num (ehiterval, "/SignalPeriod", &header_number))
      {
        if (lm_real2float (header_number, &header_float, msr->sid, "/SignalPeriod"))
          goto error_return;
        STORE4f (pMS2B200_PERIOD (record + written), header_float, swapflag);
      }

      if (yyjson_ptr_get_num (ehiterval, "/BackgroundEstimate", &header_number))
      {
        if (lm_real2float (header_number, &header_float, msr->sid, "/BackgroundEstimate"))
          goto error_return;
        STORE4f (pMS2B200_BACKGROUNDEST (record + written), header_float, swapflag);
      }

      /* Determine which wave: DILATATION versus (assumed) COMPRESSION */
      if (yyjson_ptr_get_str (ehiterval, "/Wave", &header_string))
      {
        if (lmp_strncasecmp (header_string, "DILATATION", 10) == 0)
          *pMS2B200_FLAGS (record + written) |= 0x01;
      }
      else if (blockette_type == 200)
      {
        *pMS2B200_FLAGS (record + written) |= 0x04;
      }

      if (blockette_type == 200 && yyjson_ptr_get_str (ehiterval, "/Units", &header_string) &&
          lmp_strncasecmp (header_string, "COUNT", 5) != 0)
        *pMS2B200_FLAGS (record + written) |= 0x02;

      if (yyjson_ptr_get_str (ehiterval, "/OnsetTime", &header_string))
      {
        if (ms_timestr2btime (header_string, (uint8_t *)pMS2B200_YEAR (record + written), NULL,
                              msr->sid, swapflag) == NSTERROR)
        {
          ms_log (2, "%s: Cannot convert B%u time: %s\n", msr->sid, blockette_type, header_string);
          yyjson_doc_free (ehdoc);
          return -1;
        }
      }

      if (blockette_type == 200)
      {
        if (yyjson_ptr_get_str (ehiterval, "/Detector", &header_string))
          ms_strncpopen (pMS2B200_DETECTOR (record + written), header_string, 24);
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

          while ((ehsubiterval = yyjson_arr_iter_next (&ehsubiter)) && idx < 6)
          {
            if (!yyjson_is_num (ehsubiterval))
              continue;

            if (lm_real2uint32 (yyjson_get_num (ehsubiterval), 1.0, &header_u32, msr->sid,
                                "/MEDSNR"))
              goto error_return;

            if (header_u32 > UINT8_MAX)
            {
              ms_log (2, "%s: Value of /MEDSNR is out of range: %u\n", msr->sid, header_u32);
              goto error_return;
            }

            pMS2B201_MEDSNR (record + written)[idx++] = (uint8_t)header_u32;
          }
        }

        if (yyjson_ptr_get_uint (ehiterval, "/MEDLookback", &header_uint) &&
            header_uint <= UINT8_MAX)
          *pMS2B201_LOOPBACK (record + written) = (uint8_t)header_uint;

        if (yyjson_ptr_get_uint (ehiterval, "/MEDPickAlgorithm", &header_uint) &&
            header_uint <= UINT8_MAX)
          *pMS2B201_PICKALGORITHM (record + written) = (uint8_t)header_uint;

        if (yyjson_ptr_get_str (ehiterval, "/Detector", &header_string))
          ms_strncpopen (pMS2B201_DETECTOR (record + written), header_string, 24);
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

      /* Determine which calibration type: STEP, SINE, PSEUDORANDOM, GENERIC, ABORT */
      blockette_type = 0;
      blockette_length = 0;
      if (yyjson_ptr_get_str (ehiterval, "/Type", &header_string))
      {
        if (lmp_strncasecmp (header_string, "STEP", 4) == 0)
        {
          blockette_type = 300;
          blockette_length = 60;
        }
        else if (lmp_strncasecmp (header_string, "SINE", 4) == 0)
        {
          blockette_type = 310;
          blockette_length = 60;
        }
        else if (lmp_strncasecmp (header_string, "PSEUDORANDOM", 12) == 0)
        {
          blockette_type = 320;
          blockette_length = 64;
        }
        else if (lmp_strncasecmp (header_string, "GENERIC", 7) == 0)
        {
          blockette_type = 390;
          blockette_length = 28;
        }
        else if (lmp_strncasecmp (header_string, "ABORT", 5) == 0)
        {
          blockette_type = 395;
          blockette_length = 16;
        }
      }
      /* A sequence with only an end time is a Blockette 395 alone */
      else if (yyjson_ptr_get (ehiterval, "/EndTime"))
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

      /* An end time is the only content of a B395 */
      if (blockette_type == 395 && !yyjson_ptr_get_str (ehiterval, "/EndTime", &header_string))
      {
        ms_log (2, "%s: Missing or non-string /FDSN/Calibration/Sequence/EndTime for B395\n",
                msr->sid);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      if (written > UINT16_MAX || (uint64_t)written + blockette_length > recbuflen)
      {
        ms_log (2, "%s: Record length not large enough for B%u\n", msr->sid, blockette_type);
        yyjson_doc_free (ehdoc);
        return -1;
      }

      if (blockette_type == 300 || blockette_type == 310 || blockette_type == 320 ||
          blockette_type == 390)
      {
        /* The initial fields of B300, 310, 320, 390 are the same */
        STORE2u (next_blockette, (uint16_t)written, swapflag);
        next_blockette = pMS2B300_NEXT (record + written);
        *pMS2FSDH_NUMBLOCKETTES (record) += 1;

        memset (record + written, 0, blockette_length);
        STORE2u (pMS2B300_TYPE (record + written), blockette_type, swapflag);
        STORE2u (pMS2B300_NEXT (record + written), 0, 0);

        if (yyjson_ptr_get_str (ehiterval, "/BeginTime", &header_string))
        {
          if (ms_timestr2btime (header_string, (uint8_t *)pMS2B300_YEAR (record + written), NULL,
                                msr->sid, swapflag) == NSTERROR)
          {
            ms_log (2, "%s: Cannot convert B%u time: %s\n", msr->sid, blockette_type,
                    header_string);
            yyjson_doc_free (ehdoc);
            return -1;
          }
        }

        if (blockette_type == 300)
        {
          if (yyjson_ptr_get_uint (ehiterval, "/Steps", &header_uint) && header_uint <= UINT8_MAX)
            *pMS2B300_NUMCALIBRATIONS (record + written) = (uint8_t)header_uint;

          if (yyjson_ptr_get_bool (ehiterval, "/StepFirstPulsePositive", &header_boolean) &&
              header_boolean)
            *pMS2B300_FLAGS (record + written) |= 0x01;

          if (yyjson_ptr_get_bool (ehiterval, "/StepAlternateSign", &header_boolean) &&
              header_boolean)
            *pMS2B300_FLAGS (record + written) |= 0x02;

          if (yyjson_ptr_get_str (ehiterval, "/Trigger", &header_string) &&
              lmp_strncasecmp (header_string, "AUTOMATIC", 9) == 0)
            *pMS2B300_FLAGS (record + written) |= 0x04;

          if (yyjson_ptr_get_bool (ehiterval, "/Continued", &header_boolean) && header_boolean)
            *pMS2B300_FLAGS (record + written) |= 0x08;

          if (yyjson_ptr_get_num (ehiterval, "/Duration", &header_number))
          {
            if (lm_real2uint32 (header_number, 10000.0, &header_u32, msr->sid, "/Duration"))
              goto error_return;
            STORE4u (pMS2B300_STEPDURATION (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/StepBetween", &header_number))
          {
            if (lm_real2uint32 (header_number, 10000.0, &header_u32, msr->sid, "/StepBetween"))
              goto error_return;
            STORE4u (pMS2B300_INTERVALDURATION (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/Amplitude", &header_number))
          {
            if (lm_real2float (header_number, &header_float, msr->sid, "/Amplitude"))
              goto error_return;
            STORE4f (pMS2B300_AMPLITUDE (record + written), header_float, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/InputChannel", &header_string))
            ms_strncpopen (pMS2B300_INPUTCHANNEL (record + written), header_string, 3);

          if (yyjson_ptr_get_num (ehiterval, "/ReferenceAmplitude", &header_number))
          {
            if (lm_real2uint32 (header_number, 1.0, &header_u32, msr->sid, "/ReferenceAmplitude"))
              goto error_return;
            STORE4u (pMS2B300_REFERENCEAMPLITUDE (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/Coupling", &header_string))
            ms_strncpopen (pMS2B300_COUPLING (record + written), header_string, 12);

          if (yyjson_ptr_get_str (ehiterval, "/Rolloff", &header_string))
            ms_strncpopen (pMS2B300_ROLLOFF (record + written), header_string, 12);
        }
        else if (blockette_type == 310)
        {
          if (yyjson_ptr_get_str (ehiterval, "/Trigger", &header_string) &&
              lmp_strncasecmp (header_string, "AUTOMATIC", 9) == 0)
            *pMS2B310_FLAGS (record + written) |= 0x04;

          if (yyjson_ptr_get_bool (ehiterval, "/Continued", &header_boolean) && header_boolean)
            *pMS2B310_FLAGS (record + written) |= 0x08;

          if (yyjson_ptr_get_str (ehiterval, "/AmplitudeRange", &header_string))
          {
            if (lmp_strncasecmp (header_string, "PEAKTOPEAK", 10) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x10;
            if (lmp_strncasecmp (header_string, "ZEROTOPEAK", 10) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x20;
            if (lmp_strncasecmp (header_string, "RMS", 3) == 0)
              *pMS2B310_FLAGS (record + written) |= 0x40;
          }

          if (yyjson_ptr_get_num (ehiterval, "/Duration", &header_number))
          {
            if (lm_real2uint32 (header_number, 10000.0, &header_u32, msr->sid, "/Duration"))
              goto error_return;
            STORE4u (pMS2B310_DURATION (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/SinePeriod", &header_number))
          {
            if (lm_real2float (header_number, &header_float, msr->sid, "/SinePeriod"))
              goto error_return;
            STORE4f (pMS2B310_PERIOD (record + written), header_float, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/Amplitude", &header_number))
          {
            if (lm_real2float (header_number, &header_float, msr->sid, "/Amplitude"))
              goto error_return;
            STORE4f (pMS2B310_AMPLITUDE (record + written), header_float, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/InputChannel", &header_string))
            ms_strncpopen (pMS2B310_INPUTCHANNEL (record + written), header_string, 3);

          if (yyjson_ptr_get_num (ehiterval, "/ReferenceAmplitude", &header_number))
          {
            if (lm_real2uint32 (header_number, 1.0, &header_u32, msr->sid, "/ReferenceAmplitude"))
              goto error_return;
            STORE4u (pMS2B310_REFERENCEAMPLITUDE (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/Coupling", &header_string))
            ms_strncpopen (pMS2B310_COUPLING (record + written), header_string, 12);

          if (yyjson_ptr_get_str (ehiterval, "/Rolloff", &header_string))
            ms_strncpopen (pMS2B310_ROLLOFF (record + written), header_string, 12);
        }
        else if (blockette_type == 320)
        {
          if (yyjson_ptr_get_str (ehiterval, "/Trigger", &header_string) &&
              lmp_strncasecmp (header_string, "AUTOMATIC", 9) == 0)
            *pMS2B320_FLAGS (record + written) |= 0x04;

          if (yyjson_ptr_get_bool (ehiterval, "/Continued", &header_boolean) && header_boolean)
            *pMS2B320_FLAGS (record + written) |= 0x08;

          if (yyjson_ptr_get_str (ehiterval, "/AmplitudeRange", &header_string) &&
              lmp_strncasecmp (header_string, "RANDOM", 6) == 0)
            *pMS2B320_FLAGS (record + written) |= 0x10;

          if (yyjson_ptr_get_num (ehiterval, "/Duration", &header_number))
          {
            if (lm_real2uint32 (header_number, 10000.0, &header_u32, msr->sid, "/Duration"))
              goto error_return;
            STORE4u (pMS2B320_DURATION (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/Amplitude", &header_number))
          {
            if (lm_real2float (header_number, &header_float, msr->sid, "/Amplitude"))
              goto error_return;
            STORE4f (pMS2B320_PTPAMPLITUDE (record + written), header_float, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/InputChannel", &header_string))
            ms_strncpopen (pMS2B320_INPUTCHANNEL (record + written), header_string, 3);

          if (yyjson_ptr_get_num (ehiterval, "/ReferenceAmplitude", &header_number))
          {
            if (lm_real2uint32 (header_number, 1.0, &header_u32, msr->sid, "/ReferenceAmplitude"))
              goto error_return;
            STORE4u (pMS2B320_REFERENCEAMPLITUDE (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/Coupling", &header_string))
            ms_strncpopen (pMS2B320_COUPLING (record + written), header_string, 12);

          if (yyjson_ptr_get_str (ehiterval, "/Rolloff", &header_string))
            ms_strncpopen (pMS2B320_ROLLOFF (record + written), header_string, 12);

          if (yyjson_ptr_get_str (ehiterval, "/Noise", &header_string))
            ms_strncpopen (pMS2B320_NOISETYPE (record + written), header_string, 8);
        }
        else if (blockette_type == 390)
        {
          if (yyjson_ptr_get_str (ehiterval, "/Trigger", &header_string) &&
              lmp_strncasecmp (header_string, "AUTOMATIC", 9) == 0)
            *pMS2B390_FLAGS (record + written) |= 0x04;

          if (yyjson_ptr_get_bool (ehiterval, "/Continued", &header_boolean) && header_boolean)
            *pMS2B390_FLAGS (record + written) |= 0x08;

          if (yyjson_ptr_get_num (ehiterval, "/Duration", &header_number))
          {
            if (lm_real2uint32 (header_number, 10000.0, &header_u32, msr->sid, "/Duration"))
              goto error_return;
            STORE4u (pMS2B390_DURATION (record + written), header_u32, swapflag);
          }

          if (yyjson_ptr_get_num (ehiterval, "/Amplitude", &header_number))
          {
            if (lm_real2float (header_number, &header_float, msr->sid, "/Amplitude"))
              goto error_return;
            STORE4f (pMS2B390_AMPLITUDE (record + written), header_float, swapflag);
          }

          if (yyjson_ptr_get_str (ehiterval, "/InputChannel", &header_string))
            ms_strncpopen (pMS2B390_INPUTCHANNEL (record + written), header_string, 3);
        }

        written += blockette_length;
      }

      /* Add Blockette 395 if EndTime is included */
      if (yyjson_ptr_get_str (ehiterval, "/EndTime", &header_string))
      {
        blockette_type = 395;
        blockette_length = 16;

        if (written > UINT16_MAX || (uint64_t)written + blockette_length > recbuflen)
        {
          ms_log (2, "%s: Record length not large enough for B%u\n", msr->sid, blockette_type);
          yyjson_doc_free (ehdoc);
          return -1;
        }

        STORE2u (next_blockette, (uint16_t)written, swapflag);
        next_blockette = pMS2B395_NEXT (record + written);
        *pMS2FSDH_NUMBLOCKETTES (record) += 1;

        memset (record + written, 0, blockette_length);
        STORE2u (pMS2B395_TYPE (record + written), blockette_type, swapflag);
        STORE2u (pMS2B395_NEXT (record + written), 0, 0);

        if (ms_timestr2btime (header_string, (uint8_t *)pMS2B395_YEAR (record + written), NULL,
                              msr->sid, swapflag) == NSTERROR)
        {
          ms_log (2, "%s: Cannot convert B%u time: %s\n", msr->sid, blockette_type, header_string);
          yyjson_doc_free (ehdoc);
          return -1;
        }

        written += blockette_length;
      }
    }
  } /* End if /FDSN/Calibration/Sequence */

  if (ehdoc)
  {
    yyjson_doc_free (ehdoc);
  }

  return written;

error_return:
  yyjson_doc_free (ehdoc);
  return -1;
} /* End of msr3_pack_header2() */

/************************************************************************
 *  Pack data samples.  The input data samples specified as 'src' will
 *  be packed with 'encoding' format and placed in 'dest'.
 *
 *  Return number of samples packed on success and a negative on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ************************************************************************/
static int64_t
msr_pack_data (void *dest, void *src, uint64_t maxsamples, uint64_t maxdatabytes, char sampletype,
               int8_t encoding, int8_t swapflag, uint32_t *byteswritten, const char *sid,
               int8_t verbose)
{
  int64_t nsamples;

  if (byteswritten)
    *byteswritten = 0;

  /* Decide if this is a format that we can encode */
  switch (encoding)
  {
  case DE_TEXT:
    if (sampletype != 't' && sampletype != 'a')
    {
      ms_log (2, "%s: Sample type must be text (t) for text encoding not '%c'\n", sid, sampletype);
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing text data\n", sid);

    nsamples = msr_encode_text ((char *)src, maxsamples, (char *)dest, maxdatabytes);

    if (byteswritten && nsamples > 0)
      *byteswritten = (uint32_t)nsamples;

    break;

  case DE_INT16:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT16 encoding not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof (int16_t))
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for INT16 encoding, need at least %" PRIsize_t " bytes\n",
              sid, maxdatabytes, sizeof (int16_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing INT16 data samples\n", sid);

    nsamples =
        msr_encode_int16 ((int32_t *)src, maxsamples, (int16_t *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = (uint32_t)(nsamples * 2);

    break;

  case DE_INT32:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for INT32 encoding not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof (int32_t))
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for INT32 encoding, need at least %" PRIsize_t " bytes\n",
              sid, maxdatabytes, sizeof (int32_t));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing INT32 data samples\n", sid);

    nsamples =
        msr_encode_int32 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = (uint32_t)(nsamples * 4);

    break;

  case DE_FLOAT32:
    if (sampletype != 'f')
    {
      ms_log (2, "%s: Sample type must be float (f) for FLOAT32 encoding not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof (float))
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for FLOAT32 encoding, need at least %" PRIsize_t " bytes\n",
              sid, maxdatabytes, sizeof (float));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing FLOAT32 data samples\n", sid);

    nsamples = msr_encode_float32 ((float *)src, maxsamples, (float *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = (uint32_t)(nsamples * 4);

    break;

  case DE_FLOAT64:
    if (sampletype != 'd')
    {
      ms_log (2, "%s: Sample type must be double (d) for FLOAT64 encoding not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < sizeof (double))
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for FLOAT64 encoding, need at least %" PRIsize_t " bytes\n",
              sid, maxdatabytes, sizeof (double));
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing FLOAT64 data samples\n", sid);

    nsamples =
        msr_encode_float64 ((double *)src, maxsamples, (double *)dest, maxdatabytes, swapflag);

    if (byteswritten && nsamples > 0)
      *byteswritten = (uint32_t)(nsamples * 8);

    break;

  case DE_STEIM1:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim1 compression not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < 64)
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for STEIM1 encoding, need at least 64 bytes\n",
              sid, maxdatabytes);
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing Steim1 data frames\n", sid);

    /* Always big endian Steim1 */
    swapflag = (ms_bigendianhost ()) ? 0 : 1;

    nsamples = msr_encode_steim1 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, 0,
                                  byteswritten, swapflag);

    break;

  case DE_STEIM2:
    if (sampletype != 'i')
    {
      ms_log (2, "%s: Sample type must be integer (i) for Steim2 compression not '%c'\n", sid,
              sampletype);
      return -1;
    }

    if (maxdatabytes < 64)
    {
      ms_log (2,
              "%s: Not enough space in record (%" PRIu64
              ") for STEIM2 encoding, need at least 64 bytes\n",
              sid, maxdatabytes);
      return -1;
    }

    if (verbose > 1)
      ms_log (0, "%s: Packing Steim2 data frames\n", sid);

    /* Always big endian Steim2 */
    swapflag = (ms_bigendianhost ()) ? 0 : 1;

    nsamples = msr_encode_steim2 ((int32_t *)src, maxsamples, (int32_t *)dest, maxdatabytes, 0,
                                  byteswritten, sid, swapflag);

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
    pos = 1;
    realj = real;
  }
  else
  {
    pos = 0;
    realj = -real;
  }

  preal = realj;

  bj = (int)(realj + precision);
  realj = 1 / (realj - bj);
  Aj = bj;
  Aj1 = 1;
  Bj = 1;
  Bj1 = 0;
  *num = pnum = Aj;
  *den = pden = Bj;
  if (!pos)
    *num = -*num;

  while (fabs (preal - (double)Aj / (double)Bj) > precision && Aj < maxval && Bj < maxval)
  {
    Aj2 = Aj1;
    Aj1 = Aj;
    Bj2 = Bj1;
    Bj1 = Bj;
    bj = (int)(realj + precision);
    realj = 1 / (realj - bj);
    Aj = bj * Aj1 + Aj2;
    Bj = bj * Bj1 + Bj2;
    *num = pnum;
    *den = pden;
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
  y = val;
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
  int32_t intsamprate;

  int32_t searchfactor1;
  int32_t searchfactor2;
  int32_t closestfactor;
  int32_t closestdiff;
  int32_t diff;

  /* Reject a rate that cannot be represented before converting to an integer,
   * where an out-of-range value would be undefined.  The maximum representable
   * nominal rate is 32767 * 32767, higher rates are rejected below in any case.
   * Written as a positive range test so NaN, which compares false, is rejected. */
  if (!(samprate > 0.0 && samprate <= (double)(32767 * 32767)))
    return -1;

  intsamprate = (int32_t)(samprate + 0.5);

  /* Handle case of integer sample values. */
  if (fabs (samprate - intsamprate) < 0.0000001)
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
      closestdiff = searchfactor1;
      closestfactor = searchfactor1;

      while ((intsamprate % searchfactor1) != 0)
      {
        searchfactor1 -= 1;

        /* Track the factor that generates the closest match */
        searchfactor2 = intsamprate / searchfactor1;
        diff = intsamprate - (searchfactor1 * searchfactor2);
        if (diff < closestdiff)
        {
          closestdiff = diff;
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
 * Convert a real value, multiplied by scale, to an unsigned 32-bit integer
 * rounded to nearest.  The name identifies the value in error messages.
 *
 * Returns 0 on success and -1 if the result is not representable.
 ***************************************************************************/
static int
lm_real2uint32 (double real, double scale, uint32_t *value, const char *sid, const char *name)
{
  double scaled = real * scale + 0.5;

  /* Written as a positive range test so NaN, which compares false, is rejected */
  if (!(scaled >= 0.0 && scaled < 4294967296.0))
  {
    ms_log (2, "%s: Value of %s is out of range: %g\n", sid, name, real);
    return -1;
  }

  *value = (uint32_t)scaled;

  return 0;
} /* End of lm_real2uint32() */

/***************************************************************************
 * Convert a real value to single precision.  NaN and infinities are
 * converted, a finite value beyond the single precision range is not.
 * The name identifies the value in error messages.
 *
 * Returns 0 on success and -1 if the value is not representable.
 ***************************************************************************/
static int
lm_real2float (double real, float *value, const char *sid, const char *name)
{
  if (isfinite (real) && fabs (real) > (double)FLT_MAX)
  {
    ms_log (2, "%s: Value of %s is out of range: %g\n", sid, name, real);
    return -1;
  }

  *value = (float)real;

  return 0;
} /* End of lm_real2float() */

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
    *factor = 0;
    *multiplier = 0;
    return 0;
  }
  /* Handle sample rates >= 1.0 with the SAMPLES/SECOND representation */
  else if (samprate >= 1.0)
  {
    if (ms_reduce_rate (samprate, &factor1, &factor2) == 0)
    {
      *factor = factor1;
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
      *factor = -factor1;
      *multiplier = -factor2;
      return 0;
    }
  }

  return -1;
} /* End of ms_genfactmult() */

/***************************************************************************
 * Convenience function to convert a time string to a SEED 2.x "BTIME"
 * structure, and a microsecond offset.
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
 * Return second-resolution nstime_t value on success and NSTERROR on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
static inline nstime_t
ms_timestr2btime (const char *timestr, uint8_t *btime, int8_t *usec_offset, const char *sid,
                  int8_t swapflag)
{
  uint16_t year;
  uint16_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint16_t fsec;
  int8_t l_usec_offset;
  nstime_t nstime;
  nstime_t second_nstime;

  if (!timestr || !btime)
  {
    ms_log (2, "%s(%s): Required input not defined: 'timestr' or 'btime'\n", sid, __func__);
    return NSTERROR;
  }

  if ((nstime = ms_timestr2nstime (timestr)) == NSTERROR)
    return NSTERROR;

  second_nstime = nstime2fsec_usec_offset (nstime, &fsec, &l_usec_offset);

  if (second_nstime == NSTERROR)
    return NSTERROR;

  /* Break down the (possibly carried) second-resolution time so Y/D/H/M/S stay
   * consistent with fsec/usec_offset when rounding carries into the next second */
  if (ms_nstime2time (second_nstime, &year, &day, &hour, &min, &sec, NULL))
    return NSTERROR;

  STORE2u (btime, year, swapflag);
  STORE2u (btime + 2, day, swapflag);
  *((uint8_t *)(btime + 4)) = hour;
  *((uint8_t *)(btime + 5)) = min;
  *((uint8_t *)(btime + 6)) = sec;
  *((uint8_t *)(btime + 7)) = 0;
  STORE2u (btime + 8, fsec, swapflag);

  if (usec_offset)
    *usec_offset = l_usec_offset;

  return second_nstime;
} /* End of timestr2btime() */

/***************************************************************************
 * nstime2fsec_usec_offset
 *
 * Convert a nstime_t value to a time value in tenths of milliseconds (fsec) and
 * a microsecond offset (usec_offset) from the fsec value.
 *
 * The nstime_t value returned is the nstime_t value at second resolution and
 * the appropriate value to be combined with fsec and usec_offset to recover the
 * time value (in microseconds).  Nanosecond resolution is lost in this case.
 *
 * When converting nstime_t values with sub-microsecond resolution, the result
 * will be rounded to the nearest microsecond to retain as much accuracy as
 * possible.
 *
 * The tenths of milliseconds value will be rounded to the nearest value having
 * a microsecond offset value between -50 to +49.
 *
 * Returns second-resolution nstime_t value on success and NSTERROR on error.
 ***************************************************************************/
static nstime_t
nstime2fsec_usec_offset (nstime_t nstime, uint16_t *fsec, int8_t *usec_offset)
{
  if (fsec == NULL || usec_offset == NULL)
    return NSTERROR;

  /* Round to nearest microsecond (loses nanosecond precision) */
  nstime_t usec_time = (nstime + (nstime >= 0 ? 500 : -500)) / 1000 * 1000;

  /* Convert to microseconds and tenths of milliseconds */
  int64_t total_usec = usec_time / 1000;
  int64_t total_fsec = usec_time / 100000;

  /* Calculate microsecond offset from tenths of milliseconds */
  *usec_offset = (int8_t)(total_usec - total_fsec * 100);

  /* Adjust to keep usec_offset in range [-50, +49] */
  if (*usec_offset > 49)
  {
    total_fsec += 1;
    *usec_offset -= 100;
  }
  else if (*usec_offset < -50)
  {
    total_fsec -= 1;
    *usec_offset += 100;
  }

  /* Extract fsec within current second (0-9999) */
  int64_t fsec_remainder = total_fsec % 10000;
  int64_t seconds = total_fsec / 10000;

  /* Handle negative modulo properly */
  if (fsec_remainder < 0)
  {
    fsec_remainder += 10000;
    seconds -= 1;
  }
  *fsec = (uint16_t)fsec_remainder;

  /* Return second-resolution time */
  return seconds * NSTMODULUS;
} /* End of nstime2fsec_usec_offset() */
