/***************************************************************************
 * unpack.c:
 *
 * Generic routines to unpack miniSEED records.
 *
 * Appropriate values from the record header will be byte-swapped to
 * the host order.  The purpose of this code is to provide a portable
 * way of accessing common SEED data record header information.  All
 * data structures in SEED 2.4 data records are supported.  The data
 * samples are optionally decompressed/unpacked.
 *
 * Written by Chad Trabant,
 *   IRIS Data Management Center
 ***************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "unpack.h"
#include "mseedformat.h"
#include "unpackdata.h"

/***************************************************************************
 * msr3_unpack_mseed3:
 *
 * Unpack a miniSEED 3.x data record and populate a MS3Record struct.
 * All approriate fields are byteswapped, if needed, and pointers to
 * structured data are set.
 *
 * If 'dataflag' is true the data samples are unpacked/decompressed
 * and the MS3Record->datasamples pointer is set appropriately.  The
 * data samples will be either 32-bit integers, 32-bit floats or
 * 64-bit floats (doubles) with the same byte order as the host
 * machine.  The MS3Record->numsamples will be set to the actual
 * number of samples unpacked/decompressed and MS3Record->sampletype
 * will indicated the sample type.
 *
 * All appropriate values will be byte-swapped to the host order,
 * including the data samples.
 *
 * All MS3Record struct values, including data samples and data
 * samples will be overwritten by subsequent calls to this function.
 *
 * If the 'msr' struct is NULL it will be allocated.
 *
 * Returns MS_NOERROR and populates the MS3Record struct at *ppmsr on
 * success, otherwise returns a libmseed error code (listed in
 * libmseed.h).
 ***************************************************************************/
int
msr3_unpack_mseed3 (char *record, int reclen, MS3Record **ppmsr,
                    int8_t dataflag, int8_t verbose)
{
  //int8_t headerswapflag = 0;
  //int8_t dataswapflag = 0;
  //int retval;

  //MS3Record *msr = NULL;

  if (!ppmsr)
  {
    ms_log (2, "msr3_unpack_mseed3(): ppmsr argument cannot be NULL\n");
    return MS_GENERROR;
  }

  return MS_NOERROR;
} /* End of msr3_unpack_mseed3() */

/***************************************************************************
 * msr3_unpack_mseed2:
 *
 * Unpack a miniSEED 2.x data record and populate a MS3Record struct.
 * All approriate fields are byteswapped, if needed, and pointers to
 * structured data are set.
 *
 * If 'dataflag' is true the data samples are unpacked/decompressed
 * and the MS3Record->datasamples pointer is set appropriately.  The
 * data samples will be either 32-bit integers, 32-bit floats or
 * 64-bit floats (doubles) with the same byte order as the host
 * machine.  The MS3Record->numsamples will be set to the actual
 * number of samples unpacked/decompressed and MS3Record->sampletype
 * will indicated the sample type.
 *
 * All appropriate values will be byte-swapped to the host order,
 * including the data samples.
 *
 * All MS3Record struct values, including data samples and data
 * samples will be overwritten by subsequent calls to this function.
 *
 * If the 'msr' struct is NULL it will be allocated.
 *
 * Returns MS_NOERROR and populates the MS3Record struct at *ppmsr on
 * success, otherwise returns a libmseed error code (listed in
 * libmseed.h).
 ***************************************************************************/
int
msr3_unpack_mseed2 (char *record, int reclen, MS3Record **ppmsr,
                    int8_t dataflag, int8_t verbose)
{
  int8_t headerswapflag = 0;
  int8_t dataswapflag = 0;
  int B1000offset = 0;
  int B1001offset = 0;
  int retval;

  MS3Record *msr = NULL;
  char errortsid[50];

  /* For blockette parsing */
  int blkt_offset;
  int blkt_count = 0;
  int blkt_length;
  int blkt_end = 0;
  uint16_t blkt_type;
  uint16_t next_blkt;

  if (!record)
  {
    ms_log (2, "msr3_unpack_mseed2(): record argument must be specified\n");
    return MS_GENERROR;
  }

  if (!ppmsr)
  {
    ms_log (2, "msr3_unpack_mseed2(): ppmsr argument must be specified\n");
    return MS_GENERROR;
  }

  /* Verify that passed record length is within supported range */
  if (reclen < 64 || reclen > MAXRECLEN)
  {
    ms2_recordtsid (record, errortsid);
    ms_log (2, "msr3_unpack_mseed2(%s): Record length is out of allowd range: %d\n",
            errortsid, reclen);

    return MS_OUTOFRANGE;
  }

  /* Verify that record includes a valid header */
  if (!MS2_ISVALIDHEADER (record))
  {
    ms2_recordtsid (record, errortsid);
    ms_log (2, "msr3_unpack_mseed2(%s) Record header unrecognized, not a valid miniSEED record\n",
            errortsid);

    return MS_NOTSEED;
  }

  /* Initialize the MS3Record */
  if (!(*ppmsr = msr3_init (*ppmsr)))
    return MS_GENERROR;

  /* Shortcut pointer, historical and helps readability */
  msr = *ppmsr;

  /* Set raw record pointer and record length */
  msr->record = record;
  msr->reclen = reclen;

  /* Check to see if byte swapping is needed by testing the year and day */
  if (!MS_ISVALIDYEARDAY (*pMS2FSDH_YEAR (record), *pMS2FSDH_DAY (record)))
    headerswapflag = dataswapflag = 1;

  /* Populate some of the common header fields */
  ms2_recordtsid (record, msr->tsid);
  msr->formatversion = 2;
  msr->samprate = ms_nomsamprate (HO2d(*pMS2FSDH_SAMPLERATEFACT (record), headerswapflag),
                                  HO2d(*pMS2FSDH_SAMPLERATEMULT (record), headerswapflag));
  msr->samplecnt = HO2u(*pMS2FSDH_NUMSAMPLES (record), headerswapflag);

  /* Map data quality indicator to publication version */
  if (*pMS2FSDH_DATAQUALITY (record) == 'M')
    msr->pubversion = 3;
  if (*pMS2FSDH_DATAQUALITY (record) == 'Q')
    msr->pubversion = 2;
  if (*pMS2FSDH_DATAQUALITY (record) == 'R')
    msr->pubversion = 1;
  else
    msr->pubversion = 0;

  //CHAD
  /* Combine 3 flags fields into new one */

  //EXTRA
  //  msr->dataquality = msr->fsdh->dataquality;
  //  FSDH flags not combined into remaining flag

  /* Report byte swapping status */
  if (verbose > 2)
  {
    if (headerswapflag)
      ms_log (1, "%s: Byte swapping needed for unpacking of header\n", msr->tsid);
    else
      ms_log (1, "%s: Byte swapping NOT needed for unpacking of header\n", msr->tsid);
  }

  /* Traverse the blockettes */
  blkt_offset = HO2u(*pMS2FSDH_BLOCKETTEOFFSET (record), headerswapflag);

  while ((blkt_offset != 0) &&
         (blkt_offset < reclen) &&
         (blkt_offset < MAXRECLEN))
  {
    /* Every blockette has a similar 4 byte header: type and next */
    memcpy (&blkt_type, record + blkt_offset, 2);
    memcpy (&next_blkt, record + blkt_offset + 2, 2);

    if (headerswapflag)
    {
      ms_gswap2 (&blkt_type);
      ms_gswap2 (&next_blkt);
    }

    /* Get blockette length */
    blkt_length = ms2_blktlen (blkt_type, record + blkt_offset, headerswapflag);

    if (blkt_length == 0)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Unknown blockette length for type %d\n",
              msr->tsid, blkt_type);
      break;
    }

    /* Make sure blockette is contained within the msrecord buffer */
    if ((blkt_offset + blkt_length) > reclen)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Blockette %d extends beyond record size, truncated?\n",
              msr->tsid, blkt_type);
      break;
    }

    blkt_end = blkt_offset + blkt_length;

    if (blkt_type == 100)
    {
      /* Set actual sample rate */
      msr->samprate = HO4f(*pMS2B100_SAMPRATE(record + blkt_offset), headerswapflag);

      if (headerswapflag)
      {
        ms_gswap4 (&msr->samprate);
      }
    }

    else if (blkt_type == 200)
    {
      //EXTRA B200 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap4 (pMS2B200_AMPLITUDE(record + blkt_offset));
        //ms_gswap4 (pMS2B200_PERIOD(record + blkt_offset));
        //ms_gswap4 (pMS2B200_BACKGROUNDEST(record + blkt_offset));
        //ms_gswap2 (pMS2B200_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B200_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B200_FSEC (record + blkt_offset));
      }
    }

    else if (blkt_type == 201)
    {
      //EXTRA B201 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap4 (pMS2B201_AMPLITUDE(record + blkt_offset));
        //ms_gswap4 (pMS2B201_PERIOD(record + blkt_offset));
        //ms_gswap4 (pMS2B201_BACKGROUNDEST(record + blkt_offset));
        //ms_gswap2 (pMS2B201_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B201_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B201_FSEC (record + blkt_offset));
      }
    }

    else if (blkt_type == 300)
    {
      //EXTRA B300 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B300_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B300_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B300_FSEC (record + blkt_offset));
        //ms_gswap4 (pMS2B300_STEPDURATION (record + blkt_offset));
        //ms_gswap4 (pMS2B300_INTERVALDURATION (record + blkt_offset));
        //ms_gswap4 (pMS2B300_AMPLITUDE (record + blkt_offset));
        //ms_gswap4 (pMS2B300_REFERENCEAMPLITUDE (record + blkt_offset));
      }
    }

    else if (blkt_type == 310)
    {
      //EXTRA B310 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B310_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B310_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B310_FSEC (record + blkt_offset));
        //ms_gswap4 (pMS2B310_DURATION (record + blkt_offset));
        //ms_gswap4 (pMS2B310_PERIOD (record + blkt_offset));
        //ms_gswap4 (pMS2B310_AMPLITUDE (record + blkt_offset));
        //ms_gswap4 (pMS2B310_REFERENCEAMPLITUDE (record + blkt_offset));
      }
    }

    else if (blkt_type == 320)
    {
      //EXTRA B320 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B320_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B320_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B320_FSEC (record + blkt_offset));
        //ms_gswap4 (pMS2B320_DURATION (record + blkt_offset));
        //ms_gswap4 (pMS2B320_PTPAMPLITUDE (record + blkt_offset));
        //ms_gswap4 (pMS2B320_REFERENCEAMPLITUDE (record + blkt_offset));
      }
    }

    else if (blkt_type == 390)
    {
      //EXTRA B390 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B390_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B390_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B390_FSEC (record + blkt_offset));
        //ms_gswap4 (pMS2B390_DURATION (record + blkt_offset));
        //ms_gswap4 (pMS2B390_AMPLITUDE (record + blkt_offset));
      }
    }

    else if (blkt_type == 395)
    {
      //EXTRA B395 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B395_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B395_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B395_FSEC (record + blkt_offset));
      }
    }

    else if (blkt_type == 400)
    {
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING Blockette 400 is present but discarded\n", msr->tsid);
    }

    else if (blkt_type == 405)
    {
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING Blockette 405 is present but discarded\n", msr->tsid);
    }

    else if (blkt_type == 500)
    {
      //EXTRA B500 at (record + blkt_offset)

      if (headerswapflag)
      {
        //ms_gswap4 (pMS2B500_VCOCORRECTION (record + blkt_offset));
        //ms_gswap2 (pMS2B500_YEAR (record + blkt_offset));
        //ms_gswap2 (pMS2B500_DAY (record + blkt_offset));
        //ms_gswap2 (pMS2B500_FSEC (record + blkt_offset));
        //ms_gswap4 (pMS2B500_EXCEPTIONCOUNT (record + blkt_offset));
      }
    }

    else if (blkt_type == 1000)
    {
      B1000offset = blkt_offset;

      /* Calculate record length in bytes as 2^(B1000->reclen) */
      msr->reclen = (uint32_t)1 << *pMS2B1000_RECLEN (record + blkt_offset);

      /* Compare against the specified length */
      if (msr->reclen != reclen && verbose)
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Record length in Blockette 1000 (%d) != specified length (%d)\n",
                msr->tsid, msr->reclen, reclen);
      }

      msr->encoding = *pMS2B1000_ENCODING (record + blkt_offset);
    }

    else if (blkt_type == 1001)
    {
      B1001offset = blkt_offset;

      //EXTRA B1001 at (record + blkt_offset), timing quality
    }

    else if (blkt_type == 2000)
    {
      //EXTRA B2000 at (record + blkt_offset), ?? What to do with this?

      if (headerswapflag)
      {
        //ms_gswap2 (pMS2B2000_LENGTH (record + blkt_offset));
        //ms_gswap2 (pMS2B2000_DATAOFFSET (record + blkt_offset));
        //ms_gswap4 (pMS2B2000_RECNUM (record + blkt_offset));
      }
    }

    else
    { /* Unknown blockette type */
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING, unsupported blockette type %d, skipping\n", msr->tsid);
    }

    /* Check that the next blockette offset is beyond the current blockette */
    if (next_blkt && next_blkt < (blkt_offset + blkt_length))
    {
      ms_log (2, "msr2_unpack_mseed2(%s): Offset to next blockette (%d) is within current blockette ending at byte %d\n",
              msr->tsid, next_blkt, (blkt_offset + blkt_length));

      blkt_offset = 0;
    }
    /* Check that the offset is within record length */
    else if (next_blkt && next_blkt > reclen)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Offset to next blockette (%d) from type %d is beyond record length\n",
              msr->tsid, next_blkt, blkt_type);

      blkt_offset = 0;
    }
    else
    {
      blkt_offset = next_blkt;
    }

    blkt_count++;
  } /* End of while looping through blockettes */

  /* Check for a Blockette 1000 */
  if (B1000offset == 0)
  {
    if (verbose > 1)
    {
      ms_log (1, "%s: Warning: No Blockette 1000 found\n", msr->tsid);
    }
  }

  /* Check that the data offset is after the blockette chain */
  if (blkt_end &&
      HO2u(*pMS2FSDH_NUMSAMPLES(record), headerswapflag) &&
      HO2u(*pMS2FSDH_DATAOFFSET(record), headerswapflag) < blkt_end)
  {
    ms_log (1, "%s: Warning: Data offset in fixed header (%d) is within the blockette chain ending at %d\n",
            msr->tsid, HO2u(*pMS2FSDH_DATAOFFSET(record), headerswapflag), blkt_end);
  }

  /* Check that the blockette count matches the number parsed */
  if (*pMS2FSDH_NUMBLOCKETTES(record) != blkt_count)
  {
    ms_log (1, "%s: Warning: Number of blockettes in fixed header (%d) does not match the number parsed (%d)\n",
            msr->tsid, *pMS2FSDH_NUMBLOCKETTES(record), blkt_count);
  }

  /* Calculate start time */
  msr->starttime = ms_time2nstime (HO2u(*pMS2FSDH_YEAR (record), headerswapflag),
                                   HO2u(*pMS2FSDH_DAY (record), headerswapflag),
                                   *pMS2FSDH_HOUR (record),
                                   *pMS2FSDH_MIN (record),
                                   *pMS2FSDH_SEC (record),
                                   (uint32_t)HO2u(*pMS2FSDH_FSEC (record), headerswapflag) * (NSTMODULUS / 10000));
  if (msr->starttime == NSTERROR)
  {
    ms_log (2, "%s: Cannot time values to internal time: %d,%d,%d,%d,%d,%d\n",
            HO2u(*pMS2FSDH_YEAR (record), headerswapflag),
            HO2u(*pMS2FSDH_DAY (record), headerswapflag),
            *pMS2FSDH_HOUR (record),
            *pMS2FSDH_MIN (record),
            *pMS2FSDH_SEC (record),
            HO2u(*pMS2FSDH_FSEC (record), headerswapflag));
    return MS_GENERROR;
  }

  /* Check if a time correction is included and if it has been applied,
   * bit 1 of activity flags indicates if it has been appiled */
  if (HO4d (*pMS2FSDH_TIMECORRECT (record), headerswapflag) != 0 &&
      !(*pMS2FSDH_ACTFLAGS (record) & 0x02))
  {
    msr->starttime += (nstime_t)HO4d (*pMS2FSDH_TIMECORRECT (record), headerswapflag) * (NSTMODULUS / 10000);
  }

  /* Apply microsecond precision if Blockette 1001 is present */
  if (B1001offset)
  {
    msr->starttime += (nstime_t)*pMS2B1001_MICROSECOND (record + B1001offset) * (NSTMODULUS / 1000000);
  }

  /* Unpack the data samples if requested */
  if (dataflag && msr->samplecnt > 0)
  {
    int8_t dswapflag = headerswapflag;
    int8_t bigendianhost = ms_bigendianhost ();

    /* Determine byte order of the data and set the dswapflag as
       needed; if no Blkt1000 or UNPACK_DATA_BYTEORDER environment
       variable setting assume the order is the same as the header */
    if (B1000offset)
    {
      dswapflag = 0;

      /* If BE host and LE data need swapping */
      if (bigendianhost && *pMS2B1000_BYTEORDER (record + B1000offset) == 0)
        dswapflag = 1;
      /* If LE host and BE data (or bad byte order value) need swapping */
      else if (!bigendianhost && *pMS2B1000_BYTEORDER (record + B1000offset) > 0)
        dswapflag = 1;
    }

    if (verbose > 2 && dswapflag)
      ms_log (1, "%s: Byte swapping needed for unpacking of data samples\n", msr->tsid);
    else if (verbose > 2)
      ms_log (1, "%s: Byte swapping NOT needed for unpacking of data samples\n", msr->tsid);

    retval = msr3_unpack_data (msr, dswapflag, verbose);

    if (retval < 0)
      return retval;
    else
      msr->numsamples = retval;
  }
  else
  {
    if (msr->datasamples)
      free (msr->datasamples);

    msr->datasamples = 0;
    msr->numsamples = 0;
  }

  return MS_NOERROR;
} /* End of msr3_unpack_mseed2() */

/************************************************************************
 *  msr3_unpack_data:
 *
 *  Unpack miniSEED data samples for a given MS3Record.  The packed
 *  data is accessed in the record indicated by MS3Record->record and
 *  the unpacked samples are placed in MS3Record->datasamples.  The
 *  resulting data samples are either 32-bit integers, 32-bit floats
 *  or 64-bit floats in host byte order.
 *
 *  Return number of samples unpacked or negative libmseed error code.
 ************************************************************************/
int
msr3_unpack_data (MS3Record *msr, int swapflag, int8_t verbose)
{
  int64_t datasize; /* byte size of data samples in record */
  int64_t nsamples; /* number of samples unpacked */
  size_t unpacksize; /* byte size of unpacked samples */
  int32_t samplesize = 0; /* size of the data samples in bytes */
  int32_t dataoffset = 0;
  const char *dbuf;

  if (!msr)
    return MS_GENERROR;

  if (!msr->record)
  {
    ms_log (2, "msr3_unpack_data(%s): Raw record pointer is unset\n", msr->tsid);
    return MS_GENERROR;
  }

  /* Check for decode debugging environment variable */
  if (getenv ("DECODE_DEBUG"))
    decodedebug = 1;

  /* Sanity check record length */
  if (msr->reclen == -1)
  {
    ms_log (2, "msr3_unpack_data(%s): Record size unknown\n", msr->tsid);
    return MS_NOTSEED;
  }
  else if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_unpack_data(%s): Unsupported record length: %d\n",
            msr->tsid, msr->reclen);
    return MS_OUTOFRANGE;
  }

  /* Determine offset to data */
  if (msr->formatversion == 3)
  {
    // These accessors do not exist as of their writing
    //dataoffset = 36 + *pMS3FSDH_TSIDLENGTH(msr->record) + *pMS3FSDH_EXTRALENGTH(msr->record);
    ms_log (2, "msr3_unpack_data(%s): Unpacking of miniSEED 3 not yet supported\n", msr->tsid);
    return MS_GENERROR;
  }
  else if (msr->formatversion == 2)
  {
    dataoffset = HO2u(*pMS2FSDH_DATAOFFSET(msr->record), swapflag);
  }
  else
  {
    ms_log (2, "msr3_unpack_data(%s): Unrecognized format version: %d\n",
            msr->tsid, msr->formatversion);
    return MS_GENERROR;
  }

  /* Sanity check data offset before creating a pointer based on the value */
  if (dataoffset < MINRECLEN || dataoffset >= msr->reclen)
  {
    ms_log (2, "msr3_unpack_data(%s): data offset value is not valid: %d\n",
            msr->tsid, dataoffset);
    return MS_GENERROR;
  }

  dbuf = msr->record + dataoffset;
  datasize = msr->reclen - dataoffset;

  switch (msr->encoding)
  {
  case DE_ASCII:
    samplesize = 1;
    break;
  case DE_INT16:
  case DE_INT32:
  case DE_FLOAT32:
  case DE_STEIM1:
  case DE_STEIM2:
  case DE_GEOSCOPE24:
  case DE_GEOSCOPE163:
  case DE_GEOSCOPE164:
  case DE_CDSN:
  case DE_SRO:
  case DE_DWWSSN:
    samplesize = 4;
    break;
  case DE_FLOAT64:
    samplesize = 8;
    break;
  default:
    samplesize = 0;
    break;
  }

  /* Calculate buffer size needed for unpacked samples */
  unpacksize = msr->samplecnt * samplesize;

  /* (Re)Allocate space for the unpacked data */
  if (unpacksize > 0)
  {
    msr->datasamples = realloc (msr->datasamples, unpacksize);

    if (msr->datasamples == NULL)
    {
      ms_log (2, "msr3_unpack_data(%s): Cannot (re)allocate memory\n", msr->tsid);
      return MS_GENERROR;
    }
  }
  else
  {
    if (msr->datasamples)
      free (msr->datasamples);
    msr->datasamples = 0;
    msr->numsamples = 0;
  }

  if (verbose > 2)
    ms_log (1, "%s: Unpacking %" PRId64 " samples\n", msr->tsid, msr->samplecnt);

  /* Decode data samples according to encoding */
  switch (msr->encoding)
  {
  case DE_ASCII:
    if (verbose > 1)
      ms_log (1, "%s: Found ASCII data\n", msr->tsid);

    nsamples = msr->samplecnt;
    memcpy (msr->datasamples, dbuf, nsamples);
    msr->sampletype = 'a';
    break;

  case DE_INT16:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking INT16 data samples\n", msr->tsid);

    nsamples = msr_decode_int16 ((int16_t *)dbuf, msr->samplecnt,
                                 msr->datasamples, unpacksize, swapflag);

    msr->sampletype = 'i';
    break;

  case DE_INT32:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking INT32 data samples\n", msr->tsid);

    nsamples = msr_decode_int32 ((int32_t *)dbuf, msr->samplecnt,
                                 msr->datasamples, unpacksize, swapflag);

    msr->sampletype = 'i';
    break;

  case DE_FLOAT32:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking FLOAT32 data samples\n", msr->tsid);

    nsamples = msr_decode_float32 ((float *)dbuf, msr->samplecnt,
                                   msr->datasamples, unpacksize, swapflag);

    msr->sampletype = 'f';
    break;

  case DE_FLOAT64:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking FLOAT64 data samples\n", msr->tsid);

    nsamples = msr_decode_float64 ((double *)dbuf, msr->samplecnt,
                                   msr->datasamples, unpacksize, swapflag);

    msr->sampletype = 'd';
    break;

  case DE_STEIM1:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking Steim1 data frames\n", msr->tsid);

    nsamples = msr_decode_steim1 ((int32_t *)dbuf, datasize, msr->samplecnt,
                                  msr->datasamples, unpacksize, msr->tsid, swapflag);

    if (nsamples < 0)
      return MS_GENERROR;

    msr->sampletype = 'i';
    break;

  case DE_STEIM2:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking Steim2 data frames\n", msr->tsid);

    nsamples = msr_decode_steim2 ((int32_t *)dbuf, datasize, msr->samplecnt,
                                  msr->datasamples, unpacksize, msr->tsid, swapflag);

    if (nsamples < 0)
      return MS_GENERROR;

    msr->sampletype = 'i';
    break;

  case DE_GEOSCOPE24:
  case DE_GEOSCOPE163:
  case DE_GEOSCOPE164:
    if (verbose > 1)
    {
      if (msr->encoding == DE_GEOSCOPE24)
        ms_log (1, "%s: Unpacking GEOSCOPE 24bit integer data samples\n", msr->tsid);
      if (msr->encoding == DE_GEOSCOPE163)
        ms_log (1, "%s: Unpacking GEOSCOPE 16bit gain ranged/3bit exponent data samples\n", msr->tsid);
      if (msr->encoding == DE_GEOSCOPE164)
        ms_log (1, "%s: Unpacking GEOSCOPE 16bit gain ranged/4bit exponent data samples\n", msr->tsid);
    }

    nsamples = msr_decode_geoscope ((char *)dbuf, msr->samplecnt, msr->datasamples,
                                    unpacksize, msr->encoding, msr->tsid, swapflag);

    msr->sampletype = 'f';
    break;

  case DE_CDSN:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking CDSN encoded data samples\n", msr->tsid);

    nsamples = msr_decode_cdsn ((int16_t *)dbuf, msr->samplecnt, msr->datasamples,
                                unpacksize, swapflag);

    msr->sampletype = 'i';
    break;

  case DE_SRO:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking SRO encoded data samples\n", msr->tsid);

    nsamples = msr_decode_sro ((int16_t *)dbuf, msr->samplecnt, msr->datasamples,
                               unpacksize, msr->tsid, swapflag);

    msr->sampletype = 'i';
    break;

  case DE_DWWSSN:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking DWWSSN encoded data samples\n", msr->tsid);

    nsamples = msr_decode_dwwssn ((int16_t *)dbuf, msr->samplecnt, msr->datasamples,
                                  unpacksize, swapflag);

    msr->sampletype = 'i';
    break;

  default:
    ms_log (2, "%s: Unsupported encoding format %d (%s)\n",
            msr->tsid, msr->encoding, (char *)ms_encodingstr (msr->encoding));

    return MS_UNKNOWNFORMAT;
  }

  if (nsamples != msr->samplecnt)
  {
    ms_log (2, "msr3_unpack_data(%s): only decoded %d samples of %d expected\n",
            msr->tsid, nsamples, msr->samplecnt);
    return MS_GENERROR;
  }

  return nsamples;
} /* End of msr3_unpack_data() */


/***************************************************************************
 * ms_nomsamprate:
 *
 * Calculate a sample rate from SEED sample rate factor and multiplier
 * as stored in the fixed section header of data records.
 *
 * Returns the positive sample rate.
 ***************************************************************************/
double
ms_nomsamprate (int factor, int multiplier)
{
  double samprate = 0.0;

  if (factor > 0)
    samprate = (double)factor;
  else if (factor < 0)
    samprate = -1.0 / (double)factor;
  if (multiplier > 0)
    samprate = samprate * (double)multiplier;
  else if (multiplier < 0)
    samprate = -1.0 * (samprate / (double)multiplier);

  return samprate;
} /* End of ms_nomsamprate() */

/***************************************************************************
 * ms2_recordtsid:
 *
 * Generate a time series identifier string for a specified raw
 * miniSEED 2.x data record in the format: 'FDSN:NET.STA.[LOC:]CHAN'.
 * The supplied tsid buffer must have enough room for the resulting
 * string.
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms2_recordtsid (char *record, char *tsid)
{
  int idx;

  if (!record || !tsid)
    return NULL;

  idx = 0;
  idx += ms_strncpclean (tsid+idx,  "FDSN:", 5);
  idx += ms_strncpclean (tsid+idx,  pMS2FSDH_NETWORK(record), 2);
  idx += ms_strncpclean (tsid+idx,  ".", 1);
  idx += ms_strncpclean (tsid+idx,  pMS2FSDH_STATION(record), 5);
  idx += ms_strncpclean (tsid+idx,  ".", 1);

  if ( pMS2FSDH_LOCATION(record)[0] != ' ' &&
       pMS2FSDH_LOCATION(record)[0] != '\0')
  {
    idx += ms_strncpclean (tsid+idx,  pMS2FSDH_LOCATION(record), 2);
    idx += ms_strncpclean (tsid+idx,  ":", 1);
  }
  idx += ms_strncpclean (tsid+idx,  pMS2FSDH_CHANNEL(record), 3);

  return tsid;
} /* End of ms2_recordtsid() */

/***************************************************************************
 * ms2_blktdesc():
 *
 * Return a string describing a given blockette type or NULL if the
 * type is unknown.
 ***************************************************************************/
char *
ms2_blktdesc (uint16_t blkttype)
{
  switch (blkttype)
  {
  case 100:
    return "Sample Rate";
  case 200:
    return "Generic Event Detection";
  case 201:
    return "Murdock Event Detection";
  case 300:
    return "Step Calibration";
  case 310:
    return "Sine Calibration";
  case 320:
    return "Pseudo-random Calibration";
  case 390:
    return "Generic Calibration";
  case 395:
    return "Calibration Abort";
  case 400:
    return "Beam";
  case 500:
    return "Timing";
  case 1000:
    return "Data Only SEED";
  case 1001:
    return "Data Extension";
  case 2000:
    return "Opaque Data";
  } /* end switch */

  return NULL;

} /* End of ms2_blktdesc() */

/***************************************************************************
 * ms2_blktlen():
 *
 * Returns the total length of a given blockette type in bytes or 0 if
 * type unknown.
 ***************************************************************************/
uint16_t
ms2_blktlen (uint16_t blkttype, const char *blkt, flag swapflag)
{
  uint16_t blktlen = 0;

  switch (blkttype)
  {
  case 100: /* Sample Rate */
    blktlen = 12;
    break;
  case 200: /* Generic Event Detection */
    blktlen = 28;
    break;
  case 201: /* Murdock Event Detection */
    blktlen = 36;
    break;
  case 300: /* Step Calibration */
    blktlen = 32;
    break;
  case 310: /* Sine Calibration */
    blktlen = 32;
    break;
  case 320: /* Pseudo-random Calibration */
    blktlen = 28;
    break;
  case 390: /* Generic Calibration */
    blktlen = 28;
    break;
  case 395: /* Calibration Abort */
    blktlen = 16;
    break;
  case 400: /* Beam */
    blktlen = 16;
    break;
  case 500: /* Timing */
    blktlen = 8;
    break;
  case 1000: /* Data Only SEED */
    blktlen = 8;
    break;
  case 1001: /* Data Extension */
    blktlen = 8;
    break;
  case 2000: /* Opaque Data */
    /* First 2-byte field after the blockette header is the length */
    if (blkt)
    {
      memcpy ((void *)&blktlen, blkt + 4, sizeof (int16_t));
      if (swapflag)
        ms_gswap2 (&blktlen);
    }
    break;
  } /* end switch */

  return blktlen;

} /* End of ms2_blktlen() */
