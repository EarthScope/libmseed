/***************************************************************************
 *
 * Routines to parse miniSEED.
 *
 * Written by Chad Trabant
 *   IRIS Data Management Center
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "unpack.h"
#include "mseedformat.h"

/**********************************************************************
 * msr3_parse:
 *
 * This routine will attempt to parse (detect and unpack) a miniSEED
 * record from a specified memory buffer and populate a supplied
 * MS3Record structure.  Both miniSEED 2.x and 3.x records are
 * supported by this routine.
 *
 * The record length is always automatically detected.  For miniSEED
 * 2.x this means the record must contain a 1000 blockette.
 *
 * dataflag will be passed directly to msr_unpack().
 *
 * Return values:
 *   0 : Success, populates the supplied MS3Record.
 *  >0 : Data record detected but not enough data is present, the
 *       return value is a hint of how many more bytes are needed.
 *  <0 : libmseed error code (listed in libmseed.h) is returned.
 *********************************************************************/
int
msr3_parse (char *record, uint64_t recbuflen, MS3Record **ppmsr,
            int8_t dataflag, int8_t verbose)
{
  int reclen  = 0;
  int retcode = MS_NOERROR;
  uint8_t formatversion;

  if (!ppmsr || !record)
    return MS_GENERROR;

  /* Detect record, determine length and format version */
  reclen = ms3_detect (record, recbuflen, &formatversion);

  /* No data record detected */
  if (reclen < 0)
  {
    return MS_NOTSEED;
  }

  /* Found record but could not determine length */
  if (reclen == 0)
  {
    return MINRECLEN;
  }

  if (verbose > 2)
  {
    ms_log (1, "Detected record length of %d bytes\n", reclen);
  }

  /* Check that record length is in supported range */
  if (reclen < MINRECLEN || reclen > MAXRECLEN)
  {
    ms_log (2, "Record length is out of range: %d (allowed: %d to %d)\n",
            reclen, MINRECLEN, MAXRECLEN);

    return MS_OUTOFRANGE;
  }

  /* Check if more data is required, return hint */
  if (reclen > recbuflen)
  {
    if (verbose > 2)
      ms_log (1, "Detected %d byte record, need %d more bytes\n",
              reclen, (reclen - recbuflen));

    return (reclen - recbuflen);
  }

  /* Unpack record */
  if (formatversion == 3)
  {
    retcode = msr3_unpack_mseed3 (record, reclen, ppmsr, dataflag, verbose);
  }
  else if (formatversion == 2)
  {
    retcode = msr3_unpack_mseed2 (record, reclen, ppmsr, dataflag, verbose);
  }
  else
  {
    ms_log (2, "Unrecognized format version: %d\n", formatversion);

    return MS_GENERROR;
  }

  if (retcode != MS_NOERROR)
  {
    msr3_free (ppmsr);

    return retcode;
  }

  return MS_NOERROR;
} /* End of msr3_parse() */

/********************************************************************
 * ms3_detect:
 *
 * Determine that the buffer contains a miniSEED data record by
 * verifying known signatures (fields with known limited values).
 *
 * If miniSEED 2.x is detected, search the record up to recbuflen
 * bytes for a 1000 blockette. If no blockette 1000 is found, search
 * at 64-byte offsets for the fixed section of the next header,
 * thereby implying the record length.
 *
 * Returns:
 * -1 : data record not detected or error
 *  0 : data record detected but could not determine length
 * >0 : size of the record in bytes
 *********************************************************************/
int
ms3_detect (const char *record, int recbuflen, uint8_t *formatversion)
{
  uint8_t swapflag = 0; /* Byte swapping flag */
  uint8_t foundlen = 0; /* Found record length */
  int32_t reclen = -1; /* Size of record in bytes */

  uint16_t blkt_offset; /* Byte offset for next blockette */
  uint16_t blkt_type;
  uint16_t next_blkt;
  uint16_t year;
  uint16_t day;
  const char *nextfsdh;

  if (!record || !formatversion)
    return -1;

  /* Buffer must be at least MINRECLEN */
  if (recbuflen < MINRECLEN)
    return -1;

  /* Check for valid header, set format version */
  *formatversion = 0;
  if (MS3_ISVALIDHEADER (record))
  {
    *formatversion = 3;

    reclen = 36 /* Length of fixed portion of header */
             + (uint8_t) * (record + 33) /* Length of time series identifier */
             + (uint16_t) * (record + 34) /* Length of extra headers */
             + (uint16_t) * (record + 36); /* Length of data payload */
  }
  else if (MS2_ISVALIDHEADER (record))
  {
    *formatversion = 2;

    year = (uint16_t)*(record + 20);
    day = (uint16_t)*(record + 22);

    /* Check to see if byte swapping is needed by checking for sane year and day */
    if (!MS_ISVALIDYEARDAY (year, day))
      swapflag = 1;

    blkt_offset = (uint16_t)*(record + 46);

    /* Swap order of blkt_offset if needed */
    if (swapflag)
      ms_gswap2 (&blkt_offset);

    /* Loop through blockettes as long as number is non-zero and viable */
    while (blkt_offset != 0 &&
           blkt_offset > 47 &&
           blkt_offset <= recbuflen)
    {
      memcpy (&blkt_type, record + blkt_offset, 2);
      memcpy (&next_blkt, record + blkt_offset + 2, 2);

      if (swapflag)
      {
        ms_gswap2 (&blkt_type);
        ms_gswap2 (&next_blkt);
      }

      /* Found a 1000 blockette, not truncated */
      if (blkt_type == 1000 &&
          (int)(blkt_offset + 4 + 8) <= recbuflen)
      {
        foundlen = 1;

        /* Field 3 of B1000 is a uint8_t value describing the record
         * length as 2^(value).  Calculate 2-raised with a shift. */
        reclen = (unsigned int)1 << (uint8_t)*(record + blkt_offset + 6);

        break;
      }

      /* Safety check for invalid offset */
      if (next_blkt != 0 && (next_blkt < 4 || (next_blkt - 4) <= blkt_offset))
      {
        ms_log (2, "Invalid blockette offset (%d) less than or equal to current offset (%d)\n",
                next_blkt, blkt_offset);
        return -1;
      }

      blkt_offset = next_blkt;
    }

    /* If record length was not determined by a 1000 blockette scan the buffer
     * and search for the next record */
    if (reclen == -1)
    {
      nextfsdh = record + 64;

      /* Check for record header or blank/noise record at MINRECLEN byte offsets */
      while (((nextfsdh - record) + 48) < recbuflen)
      {
        if (MS2_ISVALIDHEADER (nextfsdh))
        {
          foundlen = 1;
          reclen = nextfsdh - record;
          break;
        }

        nextfsdh += 64;
      }
    }
  } /* End of miniSEED 2.x detection */

  if (!foundlen)
    return 0;
  else
    return reclen;
} /* End of ms3_detect() */

/***************************************************************************
 * ms2_parse_raw:
 *
 * Parse and verify a miniSEED 2.x data record header (fixed section and
 * blockettes) at the lowest level, printing error messages for
 * invalid header values and optionally print raw header values.  The
 * memory at 'record' is assumed to be a miniSEED record.  Not every
 * possible test is performed, common errors and those causing
 * libmseed parsing to fail should be detected.
 *
 * The 'details' argument is interpreted as follows:
 *
 * details:
 *  0 = only print error messages for invalid header fields
 *  1 = print basic fields in addition to invalid field errors
 *  2 = print all fields in addition to invalid field errors
 *
 * The 'swapflag' argument is interpreted as follows:
 *
 * swapflag:
 *  1 = swap multibyte quantities
 *  0 = do no swapping
 * -1 = autodetect byte order using year test, swap if needed
 *
 * WARNING: This record may be modified as any byte swapping performed
 * by this routine is applied directly to the memory reference by the
 * record pointer.
 *
 * This routine is primarily intended to diagnose invalid miniSEED headers.
 *
 * Returns 0 when no errors were detected or a positive count of
 * errors detected.
 ***************************************************************************/
int
ms2_parse_raw (char *record, int maxreclen, int8_t details, int8_t swapflag)
{
  double nomsamprate;
  char tsid[21] = {0};
  char *cp;
  char *X;
  char b;
  int retval = 0;
  int b1000encoding = -1;
  int b1000reclen = -1;
  int endofblockettes = -1;
  int idx;

  uint16_t blkt_offset; /* Byte offset for next blockette */
  uint16_t blkt_type;
  uint16_t next_blkt;

  if (!record)
    return 1;

  if (maxreclen < 48)
    return 1;

  /* Build time series identifier for this record */
  ms2_recordtsid (record, tsid);

  /* Check to see if byte swapping is needed by testing the year and day */
  if (swapflag == -1 && !MS_ISVALIDYEARDAY (MS2FSDH_YEAR (record), MS2FSDH_DAY (record)))
    swapflag = 1;
  else
    swapflag = 0;

  if (details > 1)
  {
    if (swapflag == 1)
      ms_log (0, "Swapping multi-byte quantities in header\n");
    else
      ms_log (0, "Not swapping multi-byte quantities in header\n");
  }

  /* Swap byte order */
  if (swapflag)
  {
    ms_gswap2a (&MS2FSDH_YEAR (record));
    ms_gswap2a (&MS2FSDH_DAY (record));
    ms_gswap2a (&MS2FSDH_FSEC (record));
    ms_gswap2a (&MS2FSDH_NUMSAMPLES (record));
    ms_gswap2a (&MS2FSDH_SAMPLERATEFACT (record));
    ms_gswap2a (&MS2FSDH_SAMPLERATEMULT (record));
    ms_gswap4a (&MS2FSDH_TIMECORRECT (record));
    ms_gswap2a (&MS2FSDH_DATAOFFSET (record));
    ms_gswap2a (&MS2FSDH_BLOCKETTEOFFSET (record));
  }

  /* Validate fixed section header fields */
  X = record; /* Pointer of convenience */

  /* Check record sequence number, 6 ASCII digits */
  if (!isdigit ((int)*(X)) || !isdigit ((int)*(X + 1)) ||
      !isdigit ((int)*(X + 2)) || !isdigit ((int)*(X + 3)) ||
      !isdigit ((int)*(X + 4)) || !isdigit ((int)*(X + 5)))
  {
    ms_log (2, "%s: Invalid sequence number: '%c%c%c%c%c%c'\n", tsid, X, X + 1, X + 2, X + 3, X + 4, X + 5);
    retval++;
  }

  /* Check header/quality indicator */
  if (!MS2_ISDATAINDICATOR (*(X + 6)))
  {
    ms_log (2, "%s: Invalid header indicator (DRQM): '%c'\n", tsid, X + 6);
    retval++;
  }

  /* Check reserved byte, space or NULL */
  if (!(*(X + 7) == ' ' || *(X + 7) == '\0'))
  {
    ms_log (2, "%s: Invalid fixed section reserved byte (Space): '%c'\n", tsid, X + 7);
    retval++;
  }

  /* Check station code, 5 alphanumerics or spaces */
  if (!(isalnum ((unsigned char)*(X + 8)) || *(X + 8) == ' ') ||
      !(isalnum ((unsigned char)*(X + 9)) || *(X + 9) == ' ') ||
      !(isalnum ((unsigned char)*(X + 10)) || *(X + 10) == ' ') ||
      !(isalnum ((unsigned char)*(X + 11)) || *(X + 11) == ' ') ||
      !(isalnum ((unsigned char)*(X + 12)) || *(X + 12) == ' '))
  {
    ms_log (2, "%s: Invalid station code: '%c%c%c%c%c'\n", tsid, X + 8, X + 9, X + 10, X + 11, X + 12);
    retval++;
  }

  /* Check location ID, 2 alphanumerics or spaces */
  if (!(isalnum ((unsigned char)*(X + 13)) || *(X + 13) == ' ') ||
      !(isalnum ((unsigned char)*(X + 14)) || *(X + 14) == ' '))
  {
    ms_log (2, "%s: Invalid location ID: '%c%c'\n", tsid, X + 13, X + 14);
    retval++;
  }

  /* Check channel codes, 3 alphanumerics or spaces */
  if (!(isalnum ((unsigned char)*(X + 15)) || *(X + 15) == ' ') ||
      !(isalnum ((unsigned char)*(X + 16)) || *(X + 16) == ' ') ||
      !(isalnum ((unsigned char)*(X + 17)) || *(X + 17) == ' '))
  {
    ms_log (2, "%s: Invalid channel codes: '%c%c%c'\n", tsid, X + 15, X + 16, X + 17);
    retval++;
  }

  /* Check network code, 2 alphanumerics or spaces */
  if (!(isalnum ((unsigned char)*(X + 18)) || *(X + 18) == ' ') ||
      !(isalnum ((unsigned char)*(X + 19)) || *(X + 19) == ' '))
  {
    ms_log (2, "%s: Invalid network code: '%c%c'\n", tsid, X + 18, X + 19);
    retval++;
  }

  /* Check start time fields */
  if (MS2FSDH_YEAR (record) < 1900 || MS2FSDH_YEAR (record) > 2100)
  {
    ms_log (2, "%s: Unlikely start year (1900-2100): '%d'\n", tsid, MS2FSDH_YEAR (record));
    retval++;
  }
  if (MS2FSDH_DAY (record) < 1 || MS2FSDH_DAY (record) > 366)
  {
    ms_log (2, "%s: Invalid start day (1-366): '%d'\n", tsid, MS2FSDH_DAY (record));
    retval++;
  }
  if (MS2FSDH_HOUR (record) > 23)
  {
    ms_log (2, "%s: Invalid start hour (0-23): '%d'\n", tsid, MS2FSDH_HOUR (record));
    retval++;
  }
  if (MS2FSDH_MIN (record) > 59)
  {
    ms_log (2, "%s: Invalid start minute (0-59): '%d'\n", tsid, MS2FSDH_MIN (record));
    retval++;
  }
  if (MS2FSDH_SEC (record) > 60)
  {
    ms_log (2, "%s: Invalid start second (0-60): '%d'\n", tsid, MS2FSDH_SEC (record));
    retval++;
  }
  if (MS2FSDH_FSEC (record) > 9999)
  {
    ms_log (2, "%s: Invalid start fractional seconds (0-9999): '%d'\n", tsid, MS2FSDH_FSEC (record));
    retval++;
  }

  /* Check number of samples, max samples in 4096-byte Steim-2 encoded record: 6601 */
  if (fsdh->numsamples > 20000)
  {
    ms_log (2, "%s: Unlikely number of samples (>20000): '%d'\n", tsid, fsdh->numsamples);
    retval++;
  }

  /* Sanity check that there is space for blockettes when both data and blockettes are present */
  if (MS2FSDH_NUMSAMPLES(record) > 0 &&
      MS2FSDH_NUMBLOCKETTES(record) > 0 &&
      MS2FSDH_DATAOFFSET(record) <= MS2FSDH_BLOCKETTEOFFSET(record))
  {
    ms_log (2, "%s: No space for %d blockettes, data offset: %d, blockette offset: %d\n", tsid,
            MS2FSDH_NUMBLOCKETTES(record), MS2FSDH_DATAOFFSET(record), MS2FSDH_BLOCKETTEOFFSET(record));
    retval++;
  }

  /* Print raw header details */
  if (details >= 1)
  {
    /* Determine nominal sample rate */
    nomsamprate = ms_nomsamprate (MS2FSDH_SAMPLERATEFACT (record), MS2FSDH_SAMPLERATEMULT (record));

    /* Print header values */
    ms_log (0, "RECORD -- %s\n", tsid);
    ms_log (0, "        sequence number: '%c%c%c%c%c%c'\n", MS2FSDH_SEQNUM (record)[0], MS2FSDH_SEQNUM (record)[1],
            MS2FSDH_SEQNUM (record)[2], MS2FSDH_SEQNUM (record)[3], MS2FSDH_SEQNUM (record)[4], MS2FSDH_SEQNUM (record)[5]);
    ms_log (0, " data quality indicator: '%c'\n", MS2FSDH_DATAQUALTIY (record));
    if (details > 0)
      ms_log (0, "               reserved: '%c'\n", MS2FSDH_RESERVED (record));
    ms_log (0, "           station code: '%c%c%c%c%c'\n", MS2FSDH_STATION (record)[0], MS2FSDH_STATION (record)[1],
            MS2FSDH_STATION (record)[2], MS2FSDH_STATION (record)[3], MS2FSDH_STATION (record)[4]);
    ms_log (0, "            location ID: '%c%c'\n", MS2FSDH_LOCATION (record)[0], MS2FSDH_LOCATION (record)[1]);
    ms_log (0, "          channel codes: '%c%c%c'\n", MS2FSDH_CHANNEL (record)[0], MS2FSDH_CHANNEL (record)[1], MS2FSDH_CHANNEL (record)[2]);
    ms_log (0, "           network code: '%c%c'\n", MS2FSDH_NETWORK (record)[0], MS2FSDH_NETWORK (record)[1]);
    ms_log (0, "             start time: %d,%d,%d:%d:%d.%04d (unused: %d)\n", MS2FSDH_YEAR (record), MS2FSDH_DAY (record),
            MS2FSDH_HOUR (record), MS2FSDH_MIN (record), MS2FSDH_SEC (record), MS2FSDH_FSEC (record), MS2FSDH_UNUSED (record));
    ms_log (0, "      number of samples: %d\n", MS2FSDH_NUMSAMPLES (record));
    ms_log (0, "     sample rate factor: %d  (%.10g samples per second)\n",
            MS2FSDH_SAMPLERATEFACT (record), nomsamprate);
    ms_log (0, " sample rate multiplier: %d\n", MS2FSDH_SAMPLERATEMULT (record));

    /* Print flag details if requested */
    if (details > 1)
    {
      /* Activity flags */
      b = MS2FSDH_ACTFLAGS (record);
      ms_log (0, "         activity flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
              bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
              bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
      if (b & 0x01)
        ms_log (0, "                         [Bit 0] Calibration signals present\n");
      if (b & 0x02)
        ms_log (0, "                         [Bit 1] Time correction applied\n");
      if (b & 0x04)
        ms_log (0, "                         [Bit 2] Beginning of an event, station trigger\n");
      if (b & 0x08)
        ms_log (0, "                         [Bit 3] End of an event, station detrigger\n");
      if (b & 0x10)
        ms_log (0, "                         [Bit 4] A positive leap second happened in this record\n");
      if (b & 0x20)
        ms_log (0, "                         [Bit 5] A negative leap second happened in this record\n");
      if (b & 0x40)
        ms_log (0, "                         [Bit 6] Event in progress\n");
      if (b & 0x80)
        ms_log (0, "                         [Bit 7] Undefined bit set\n");

      /* I/O and clock flags */
      b = MS2FSDH_IOFLAGS (record);
      ms_log (0, "    I/O and clock flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
              bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
              bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
      if (b & 0x01)
        ms_log (0, "                         [Bit 0] Station volume parity error possibly present\n");
      if (b & 0x02)
        ms_log (0, "                         [Bit 1] Long record read (possibly no problem)\n");
      if (b & 0x04)
        ms_log (0, "                         [Bit 2] Short record read (record padded)\n");
      if (b & 0x08)
        ms_log (0, "                         [Bit 3] Start of time series\n");
      if (b & 0x10)
        ms_log (0, "                         [Bit 4] End of time series\n");
      if (b & 0x20)
        ms_log (0, "                         [Bit 5] Clock locked\n");
      if (b & 0x40)
        ms_log (0, "                         [Bit 6] Undefined bit set\n");
      if (b & 0x80)
        ms_log (0, "                         [Bit 7] Undefined bit set\n");

      /* Data quality flags */
      b = MS2FSDH_DQFLAGS (record);
      ms_log (0, "     data quality flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
              bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
              bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
      if (b & 0x01)
        ms_log (0, "                         [Bit 0] Amplifier saturation detected\n");
      if (b & 0x02)
        ms_log (0, "                         [Bit 1] Digitizer clipping detected\n");
      if (b & 0x04)
        ms_log (0, "                         [Bit 2] Spikes detected\n");
      if (b & 0x08)
        ms_log (0, "                         [Bit 3] Glitches detected\n");
      if (b & 0x10)
        ms_log (0, "                         [Bit 4] Missing/padded data present\n");
      if (b & 0x20)
        ms_log (0, "                         [Bit 5] Telemetry synchronization error\n");
      if (b & 0x40)
        ms_log (0, "                         [Bit 6] A digital filter may be charging\n");
      if (b & 0x80)
        ms_log (0, "                         [Bit 7] Time tag is questionable\n");
    }

    ms_log (0, "   number of blockettes: %d\n", MS2FSDH_NUMBLOCKETTES (record));
    ms_log (0, "        time correction: %ld\n", (long int)MS2FSDH_TIMECORRECT (record));
    ms_log (0, "            data offset: %d\n", MS2FSDH_DATAOFFSET (record));
    ms_log (0, " first blockette offset: %d\n", MS2FSDH_BLOCKETTEOFFSET (record));
  } /* Done printing raw header details */

  /* Validate and report information in the blockette chain */
  if (MS2FSDH_BLOCKETTEOFFSET (record) > 46 && MS2FSDH_BLOCKETTEOFFSET (record) < maxreclen)
  {
    int blkt_offset = MS2FSDH_BLOCKETTEOFFSET (record);
    int blkt_count  = 0;
    int blkt_length;
    uint16_t blkt_type;
    uint16_t next_blkt;
    char *blkt_desc;

    /* Traverse blockette chain */
    while (blkt_offset != 0 && blkt_offset < maxreclen)
    {
      /* Every blockette has a similar 4 byte header: type and next */
      memcpy (&blkt_type, record + blkt_offset, 2);
      memcpy (&next_blkt, record + blkt_offset + 2, 2);

      if (swapflag)
      {
        ms_gswap2 (&blkt_type);
        ms_gswap2 (&next_blkt);
      }

      /* Print common header fields */
      if (details >= 1)
      {
        blkt_desc = ms_blktdesc (blkt_type);
        ms_log (0, "          BLOCKETTE %u: (%s)\n", blkt_type, (blkt_desc) ? blkt_desc : "Unknown");
        ms_log (0, "              next blockette: %u\n", next_blkt);
      }

      blkt_length = ms_blktlen (blkt_type, record + blkt_offset, swapflag);
      if (blkt_length == 0)
      {
        ms_log (2, "%s: Unknown blockette length for type %d\n", tsid, blkt_type);
        retval++;
      }

      /* Track end of blockette chain */
      endofblockettes = blkt_offset + blkt_length - 1;

      /* Sanity check that the blockette is contained in the record */
      if (endofblockettes > maxreclen)
      {
        ms_log (2, "%s: Blockette type %d at offset %d with length %d does not fit in record (%d)\n",
                tsid, blkt_type, blkt_offset, blkt_length, maxreclen);
        retval++;
        break;
      }

      if (blkt_type == 100)
      {
        if (swapflag)
          ms_gswap4 (&MS2B100_SAMPRATE(record + blkt_offset));

        if (details >= 1)
        {
          ms_log (0, "          actual sample rate: %.10g\n", MS2B100_SAMPRATE(record + blkt_offset));

          if (details > 1)
          {
            b = MS2B100_FLAGS(record + blkt_offset);
            ms_log (0, "             undefined flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                    bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                    bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));

            ms_log (0, "          reserved bytes (3): %u,%u,%u\n",
                    MS2B100_RESERVED(record + blkt_offset)[0],
                    MS2B100_RESERVED(record + blkt_offset)[1],
                    MS2B100_RESERVED(record + blkt_offset)[2]);
          }
        }
      }

      else if (blkt_type == 200)
      {
        if (swapflag)
        {
          ms_gswap4 (&MS2B200_AMPLITUDE(record + blkt_offset));
          ms_gswap4 (&MS2B200_PERIOD(record + blkt_offset));
          ms_gswap4 (&MS2B200_BACKGROUNDEST(record + blkt_offset));
          ms_gswap2 (&MS2B200_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B200_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B200_FSEC (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "            signal amplitude: %g\n", MS2B200_AMPLITUDE(record + blkt_offset));
          ms_log (0, "               signal period: %g\n", MS2B200_PERIOD(record + blkt_offset));
          ms_log (0, "         background estimate: %g\n", MS2B200_BACKGROUNDEST(record + blkt_offset));

          if (details > 1)
          {
            b = MS2B200_FLAGS(record + blkt_offset);
            ms_log (0, "       event detection flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                    bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                    bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
            if (b & 0x01)
              ms_log (0, "                         [Bit 0] 1: Dilatation wave\n");
            else
              ms_log (0, "                         [Bit 0] 0: Compression wave\n");
            if (b & 0x02)
              ms_log (0, "                         [Bit 1] 1: Units after deconvolution\n");
            else
              ms_log (0, "                         [Bit 1] 0: Units are digital counts\n");
            if (b & 0x04)
              ms_log (0, "                         [Bit 2] Bit 0 is undetermined\n");
            ms_log (0, "               reserved byte: %u\n", MS2B200_RESERVED (record + blkt_offset));
          }

          ms_log (0, "           signal onset time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B200_YEAR (record + blkt_offset), MS2B200_DAY (record + blkt_offset),
                  MS2B200_HOUR (record + blkt_offset), MS2B200_MIN (record + blkt_offset),
                  MS2B200_SEC (record + blkt_offset), MS2B200_FSEC (record + blkt_offset),
                  MS2B200_UNUSED (record + blkt_offset));
          ms_log (0, "               detector name: %.24s\n", MS2B200_DETECTOR (record + blkt_offset));
        }
      }

      else if (blkt_type == 201)
      {
        if (swapflag)
        {
          ms_gswap4 (&MS2B201_AMPLITUDE(record + blkt_offset));
          ms_gswap4 (&MS2B201_PERIOD(record + blkt_offset));
          ms_gswap4 (&MS2B201_BACKGROUNDEST(record + blkt_offset));
          ms_gswap2 (&MS2B201_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B201_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B201_FSEC (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "            signal amplitude: %g\n", MS2B201_AMPLITUDE(record + blkt_offset));
          ms_log (0, "               signal period: %g\n", MS2B201_PERIOD(record + blkt_offset));
          ms_log (0, "         background estimate: %g\n", MS2B201_BACKGROUNDEST(record + blkt_offset));

          b = MS2B201_FLAGS(record + blkt_offset);
          ms_log (0, "       event detection flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
          if (b & 0x01)
            ms_log (0, "                         [Bit 0] 1: Dilation wave\n");
          else
            ms_log (0, "                         [Bit 0] 0: Compression wave\n");

          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B201_RESERVED(record + blkt_offset));
          ms_log (0, "           signal onset time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B201_YEAR (record + blkt_offset), MS2B201_DAY (record + blkt_offset),
                  MS2B201_HOUR (record + blkt_offset), MS2B201_MIN (record + blkt_offset),
                  MS2B201_SEC (record + blkt_offset), MS2B201_FSEC (record + blkt_offset),
                  MS2B201_UNUSED (record + blkt_offset));
          ms_log (0, "                  SNR values: ");

          for (idx = 0; idx < 6; idx++)
            ms_log (0, "%u  ", MS2B201_SNRVALUES (record + blkt_offset)[idx]);
          ms_log (0, "\n");
          ms_log (0, "              loopback value: %u\n", MS2B201_LOOPBACK (record + blkt_offset));
          ms_log (0, "              pick algorithm: %u\n", MS2B201_PICKALGORITHM (record + blkt_offset));
          ms_log (0, "               detector name: %.24s\n", MS2B201_DETECTOR (record + blkt_offset));
        }
      }

      else if (blkt_type == 300)
      {
        if (swapflag)
        {
          ms_gswap2 (&MS2B300_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B300_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B300_FSEC (record + blkt_offset));
          ms_gswap4 (&MS2B300_STEPDURATION (record + blkt_offset));
          ms_gswap4 (&MS2B300_INTERVALDURATION (record + blkt_offset));
          ms_gswap4 (&MS2B300_AMPLITUDE (record + blkt_offset));
          ms_gswap4 (&MS2B300_REFERENCEAMPLITUDE (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "      calibration start time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B300_YEAR (record + blkt_offset), MS2B300_DAY (record + blkt_offset),
                  MS2B300_HOUR (record + blkt_offset), MS2B300_MIN (record + blkt_offset),
                  MS2B300_SEC (record + blkt_offset), MS2B300_FSEC (record + blkt_offset),
                  MS2B300_UNUSED (record + blkt_offset));
          ms_log (0, "      number of calibrations: %u\n", MS2B300_NUMCALIBRATIONS (record + blkt_offset));

          b = MS2B300_FLAGS (record + blkt_offset);
          ms_log (0, "           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
          if (b & 0x01)
            ms_log (0, "                         [Bit 0] First pulse is positive\n");
          if (b & 0x02)
            ms_log (0, "                         [Bit 1] Calibration's alternate sign\n");
          if (b & 0x04)
            ms_log (0, "                         [Bit 2] Calibration was automatic\n");
          if (b & 0x08)
            ms_log (0, "                         [Bit 3] Calibration continued from previous record(s)\n");

          ms_log (0, "               step duration: %u\n", MS2B300_STEPDURATION (record + blkt_offset));
          ms_log (0, "           interval duration: %u\n", MS2B300_INTERVALDURATION (record + blkt_offset));
          ms_log (0, "            signal amplitude: %g\n", MS2B300_AMPLITUDE (record + blkt_offset));
          ms_log (0, "        input signal channel: %.3s", MS2B300_INPUTCHANNEL (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B300_RESERVED (record + blkt_offset));
          ms_log (0, "         reference amplitude: %u\n", MS2B300_REFERENCEAMPLITUDE (record + blkt_offset));
          ms_log (0, "                    coupling: %.12s\n", MS2B300_COUPLING (record + blkt_offset));
          ms_log (0, "                     rolloff: %.12s\n", MS2B300_ROLLOFF (record + blkt_offset));
        }
      }

      else if (blkt_type == 310)
      {
        if (swapflag)
        {
          ms_gswap2 (&MS2B310_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B310_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B310_FSEC (record + blkt_offset));
          ms_gswap4 (&MS2B310_DURATION (record + blkt_offset));
          ms_gswap4 (&MS2B310_PERIOD (record + blkt_offset));
          ms_gswap4 (&MS2B310_AMPLITUDE (record + blkt_offset));
          ms_gswap4 (&MS2B310_REFERENCEAMPLITUDE (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "      calibration start time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B310_YEAR (record + blkt_offset), MS2B310_DAY (record + blkt_offset),
                  MS2B310_HOUR (record + blkt_offset), MS2B310_MIN (record + blkt_offset),
                  MS2B310_SEC (record + blkt_offset), MS2B310_FSEC (record + blkt_offset),
                  MS2B310_UNUSED (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B310_RESERVED1 (record + blkt_offset));

          b = MS2B310_FLAGS (record + blkt_offset);
          ms_log (0, "           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
          if (b & 0x04)
            ms_log (0, "                         [Bit 2] Calibration was automatic\n");
          if (b & 0x08)
            ms_log (0, "                         [Bit 3] Calibration continued from previous record(s)\n");
          if (b & 0x10)
            ms_log (0, "                         [Bit 4] Peak-to-peak amplitude\n");
          if (b & 0x20)
            ms_log (0, "                         [Bit 5] Zero-to-peak amplitude\n");
          if (b & 0x40)
            ms_log (0, "                         [Bit 6] RMS amplitude\n");

          ms_log (0, "        calibration duration: %u\n", MS2B310_DURATION (record + blkt_offset));
          ms_log (0, "               signal period: %g\n", MS2B310_PERIOD (record + blkt_offset));
          ms_log (0, "            signal amplitude: %g\n", MS2B310_AMPLITUDE (record + blkt_offset));
          ms_log (0, "        input signal channel: %.3s", MS2B310_INPUTCHANNEL (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B310_RESERVED2 (record + blkt_offset));
          ms_log (0, "         reference amplitude: %u\n", MS2B310_REFERENCEAMPLITUDE (record + blkt_offset));
          ms_log (0, "                    coupling: %.12s\n", MS2B310_COUPLING (record + blkt_offset));
          ms_log (0, "                     rolloff: %.12s\n", MS2B310_ROLLOFF (record + blkt_offset));
        }
      }

      else if (blkt_type == 320)
      {
        struct blkt_320_s *blkt_320 = (struct blkt_320_s *)(record + blkt_offset + 4);

        if (swapflag)
        {
          ms_gswap2 (&MS2B320_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B320_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B320_FSEC (record + blkt_offset));
          ms_gswap4 (&MS2B320_DURATION (record + blkt_offset));
          ms_gswap4 (&MS2B320_PTPAMPLITUDE (record + blkt_offset));
          ms_gswap4 (&MS2B320_REFERENCEAMPLITUDE (record + blkt_offset));

        if (details >= 1)
        {
          ms_log (0, "      calibration start time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B320_YEAR (record + blkt_offset), MS2B320_DAY (record + blkt_offset),
                  MS2B320_HOUR (record + blkt_offset), MS2B320_MIN (record + blkt_offset),
                  MS2B320_SEC (record + blkt_offset), MS2B320_FSEC (record + blkt_offset),
                  MS2B320_UNUSED (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B320_RESERVED1 (record + blkt_offset));

          b = MS2B320_FLAGS (record + blkt_offset);
          ms_log (0, "           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
          if (b & 0x04)
            ms_log (0, "                         [Bit 2] Calibration was automatic\n");
          if (b & 0x08)
            ms_log (0, "                         [Bit 3] Calibration continued from previous record(s)\n");
          if (b & 0x10)
            ms_log (0, "                         [Bit 4] Random amplitudes\n");

          ms_log (0, "        calibration duration: %u\n", MS2B320_DURATION (record + blkt_offset));
          ms_log (0, "      peak-to-peak amplitude: %g\n", MS2B320_PTPAMPLITUDE (record + blkt_offset));
          ms_log (0, "        input signal channel: %.3s", MS2B320_INPUTCHANNEL (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B320_RESERVED2 (record + blkt_offset));
          ms_log (0, "         reference amplitude: %u\n", MS2B320_REFERENCEAMPLITUDE (record + blkt_offset));
          ms_log (0, "                    coupling: %.12s\n", MS2B320_COUPLING (record + blkt_offset));
          ms_log (0, "                     rolloff: %.12s\n", MS2B320_ROLLOFF (record + blkt_offset));
          ms_log (0, "                  noise type: %.8s\n", MS2B320_NOISETYPE (record + blkt_offset));
        }
      }

      else if (blkt_type == 390)
      {
        if (swapflag)
        {
          ms_gswap2 (&MS2B390_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B390_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B390_FSEC (record + blkt_offset));
          ms_gswap4 (&MS2B390_DURATION (record + blkt_offset));
          ms_gswap4 (&MS2B390_AMPLITUDE (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "      calibration start time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B390_YEAR (record + blkt_offset), MS2B390_DAY (record + blkt_offset),
                  MS2B390_HOUR (record + blkt_offset), MS2B390_MIN (record + blkt_offset),
                  MS2B390_SEC (record + blkt_offset), MS2B390_FSEC (record + blkt_offset),
                  MS2B390_UNUSED (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B390_RESERVED1 (record + blkt_offset));

          b = MS2B390_FLAGS (record + blkt_offset);
          ms_log (0, "           calibration flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));
          if (b & 0x04)
            ms_log (0, "                         [Bit 2] Calibration was automatic\n");
          if (b & 0x08)
            ms_log (0, "                         [Bit 3] Calibration continued from previous record(s)\n");

          ms_log (0, "        calibration duration: %u\n", MS2B390_DURATION (record + blkt_offset));
          ms_log (0, "            signal amplitude: %g\n", MS2B390_AMPLITUDE (record + blkt_offset));
          ms_log (0, "        input signal channel: %.3s", MS2B390_INPUTCHANEL (record + blkt_offset));
          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B390_RESERVED2 (record + blkt_offset));
        }
      }

      else if (blkt_type == 395)
      {
        if (swapflag)
        {
          ms_gswap2 (&MS2B395_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B395_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B395_FSEC (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "        calibration end time: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B395_YEAR (record + blkt_offset), MS2B395_DAY (record + blkt_offset),
                  MS2B395_HOUR (record + blkt_offset), MS2B395_MIN (record + blkt_offset),
                  MS2B395_SEC (record + blkt_offset), MS2B395_FSEC (record + blkt_offset),
                  MS2B395_UNUSED (record + blkt_offset));
          if (details > 1)
            ms_log (0, "          reserved bytes (2): %u,%u\n",
                    MS2B395_RESERVED (record + blkt_offset)[0], MS2B395_RESERVED (record + blkt_offset)[1]);
        }
      }

      else if (blkt_type == 400)
      {
        if (swapflag)
        {
          ms_gswap4 (&MS2B400_AZIMUTH (record + blkt_offset));
          ms_gswap4 (&MS2B400_SLOWNESS (record + blkt_offset));
          ms_gswap4 (&MS2B400_CONFIGURATION (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "      beam azimuth (degrees): %g\n", MS2B400_AZIMUTH (record + blkt_offset));
          ms_log (0, "  beam slowness (sec/degree): %g\n", MS2B400_SLOWNESS (record + blkt_offset));
          ms_log (0, "               configuration: %u\n", MS2B400_CONFIGURATION (record + blkt_offset));
          if (details > 1)
            ms_log (0, "          reserved bytes (2): %u,%u\n",
                    MS2B400_RESERVED (record + blkt_offset)[0], MS2B400_RESERVED (record + blkt_offset)[1]);
        }
      }

      else if (blkt_type == 405)
      {
        uint16_t firstvalue = MS2B400_DELAYVALUES (record + blkt_offset)[0]; /* Work on a private copy */

        if (swapflag)
          ms_gswap2 (&firstvalue);

        if (details >= 1)
          ms_log (0, "           first delay value: %u\n", firstvalue);
      }

      else if (blkt_type == 500)
      {
        if (swapflag)
        {
          ms_gswap4 (&MS2B500_VCOCORRECTION (record + blkt_offset));
          ms_gswap2 (&MS2B500_YEAR (record + blkt_offset));
          ms_gswap2 (&MS2B500_DAY (record + blkt_offset));
          ms_gswap2 (&MS2B500_FSEC (record + blkt_offset));
          ms_gswap4 (&MS2B500_EXCEPTIONCOUNT (record + blkt_offset));
        }

        if (details >= 1)
        {
          ms_log (0, "              VCO correction: %g%%\n", MS2B500_VCOCORRECTION (record + blkt_offset));
          ms_log (0, "           time of exception: %d,%d,%d:%d:%d.%04d (unused: %d)\n",
                  MS2B500_YEAR (record + blkt_offset), MS2B500_DAY (record + blkt_offset),
                  MS2B500_HOUR (record + blkt_offset), MS2B500_MIN (record + blkt_offset),
                  MS2B500_SEC (record + blkt_offset), MS2B500_FSEC (record + blkt_offset),
                  MS2B500_UNUSED (record + blkt_offset));
          ms_log (0, "                        usec: %d\n", MS2B500_MICROSECOND (record + blkt_offset));
          ms_log (0, "           reception quality: %u%%\n", MS2B500_RECEPTIONQUALITY (record + blkt_offset));
          ms_log (0, "             exception count: %u\n", MS2B500_EXCEPTIONCOUNT (record + blkt_offset));
          ms_log (0, "              exception type: %.16s\n", MS2B500_EXCEPTIONTYPE (record + blkt_offset));
          ms_log (0, "                 clock model: %.32s\n", MS2B500_CLOCKMODEL (record + blkt_offset));
          ms_log (0, "                clock status: %.128s\n", MS2B500_CLOCKSTATUS (record + blkt_offset));
        }
      }

      else if (blkt_type == 1000)
      {
        char order[40];

        /* Calculate record size in bytes as 2^(blkt_1000->rec_len) */
        b1000reclen = (unsigned int)1 << MS2B1000_RECLEN (record + blkt_offset);

        /* Big or little endian? */
        if (MS2B1000_BYTEORDER (record + blkt_offset) == 0)
          strncpy (order, "Little endian", sizeof (order) - 1);
        else if (MS2B1000_BYTEORDER (record + blkt_offset) == 1)
          strncpy (order, "Big endian", sizeof (order) - 1);
        else
          strncpy (order, "Unknown value", sizeof (order) - 1);

        if (details >= 1)
        {
          ms_log (0, "                    encoding: %s (val:%u)\n",
                  (char *)ms_encodingstr (MS2B1000_ENCODING (record + blkt_offset)),
                  MS2B1000_ENCODING (record + blkt_offset));
          ms_log (0, "                  byte order: %s (val:%u)\n",
                  order, MS2B1000_BYTEORDER (record + blkt_offset));
          ms_log (0, "               record length: %d (val:%u)\n",
                  b1000reclen, MS2B1000_RECLEN (record + blkt_offset));

          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B1000_RESERVED (record + blkt_offset));
        }

        /* Save encoding format */
        b1000encoding = MS2B1000_ENCODING (record + blkt_offset);

        /* Sanity check encoding format */
        if (!(b1000encoding >= 0 && b1000encoding <= 5) &&
            !(b1000encoding >= 10 && b1000encoding <= 19) &&
            !(b1000encoding >= 30 && b1000encoding <= 33))
        {
          ms_log (2, "%s: Blockette 1000 encoding format invalid (0-5,10-19,30-33): %d\n", tsid, b1000encoding);
          retval++;
        }

        /* Sanity check byte order flag */
        if (MS2B1000_BYTEORDER (record + blkt_offset) != 0 &&
            MS2B1000_BYTEORDER (record + blkt_offset) != 1)
        {
          ms_log (2, "%s: Blockette 1000 byte order flag invalid (0 or 1): %d\n", tsid,
                  MS2B1000_BYTEORDER (record + blkt_offset));
          retval++;
        }
      }

      else if (blkt_type == 1001)
      {
        if (details >= 1)
        {
          ms_log (0, "              timing quality: %u%%\n", MS2B1001_TIMINGQUALITY (record + blkt_offset));
          ms_log (0, "                micro second: %d\n", MS2B1001_MICROSECOND (record + blkt_offset));

          if (details > 1)
            ms_log (0, "               reserved byte: %u\n", MS2B1001_RESERVED (record + blkt_offset));

          ms_log (0, "                 frame count: %u\n", MS2B1001_FRAMECOUNT (record + blkt_offset));
        }
      }

      else if (blkt_type == 2000)
      {
        char order[40];

        if (swapflag)
        {
          ms_gswap2 (&MS2B2000_LENGTH (record + blkt_offset));
          ms_gswap2 (&MS2B2000_DATAOFFSET (record + blkt_offset));
          ms_gswap4 (&MS2B2000_RECNUM (record + blkt_offset));
        }

        /* Big or little endian? */
        if (MS2B2000_BYTEORDER (record + blkt_offset) == 0)
          strncpy (order, "Little endian", sizeof (order) - 1);
        else if (MS2B2000_BYTEORDER (record + blkt_offset) == 1)
          strncpy (order, "Big endian", sizeof (order) - 1);
        else
          strncpy (order, "Unknown value", sizeof (order) - 1);

        if (details >= 1)
        {
          ms_log (0, "            blockette length: %u\n", MS2B2000_LENGTH (record + blkt_offset));
          ms_log (0, "                 data offset: %u\n", MS2B2000_DATAOFFSET (record + blkt_offset));
          ms_log (0, "               record number: %u\n", MS2B2000_RECNUM (record + blkt_offset));
          ms_log (0, "                  byte order: %s (val:%u)\n",
                  order, MS2B2000_BYTEORDER (record + blkt_offset));
          b = MS2B2000_FLAGS (record + blkt_offset);
          ms_log (0, "                  data flags: [%u%u%u%u%u%u%u%u] 8 bits\n",
                  bit (b, 0x01), bit (b, 0x02), bit (b, 0x04), bit (b, 0x08),
                  bit (b, 0x10), bit (b, 0x20), bit (b, 0x40), bit (b, 0x80));

          if (details > 1)
          {
            if (b & 0x01)
              ms_log (0, "                         [Bit 0] 1: Stream oriented\n");
            else
              ms_log (0, "                         [Bit 0] 0: Record oriented\n");
            if (b & 0x02)
              ms_log (0, "                         [Bit 1] 1: Blockette 2000s may NOT be packaged\n");
            else
              ms_log (0, "                         [Bit 1] 0: Blockette 2000s may be packaged\n");
            if (!(b & 0x04) && !(b & 0x08))
              ms_log (0, "                      [Bits 2-3] 00: Complete blockette\n");
            else if (!(b & 0x04) && (b & 0x08))
              ms_log (0, "                      [Bits 2-3] 01: First blockette in span\n");
            else if ((b & 0x04) && (b & 0x08))
              ms_log (0, "                      [Bits 2-3] 11: Continuation blockette in span\n");
            else if ((b & 0x04) && !(b & 0x08))
              ms_log (0, "                      [Bits 2-3] 10: Final blockette in span\n");
            if (!(b & 0x10) && !(b & 0x20))
              ms_log (0, "                      [Bits 4-5] 00: Not file oriented\n");
            else if (!(b & 0x10) && (b & 0x20))
              ms_log (0, "                      [Bits 4-5] 01: First blockette of file\n");
            else if ((b & 0x10) && !(b & 0x20))
              ms_log (0, "                      [Bits 4-5] 10: Continuation of file\n");
            else if ((b & 0x10) && (b & 0x20))
              ms_log (0, "                      [Bits 4-5] 11: Last blockette of file\n");
          }

          ms_log (0, "           number of headers: %u\n", MS2B2000_NUMHEADERS (record + blkt_offset));

          /* Crude display of the opaque data headers */
          if (details > 1)
            ms_log (0, "                     headers: %.*s\n",
                    (MS2B2000_DATAOFFSET (record + blkt_offset) - 15), MS2B2000_PAYLOAD (record + blkt_offset));
        }
      }

      else
      {
        ms_log (2, "%s: Unrecognized blockette type: %d\n", tsid, blkt_type);
        retval++;
      }

      /* Sanity check the next blockette offset */
      if (next_blkt && next_blkt <= endofblockettes)
      {
        ms_log (2, "%s: Next blockette offset (%d) is within current blockette ending at byte %d\n",
                tsid, next_blkt, endofblockettes);
        blkt_offset = 0;
      }
      else
      {
        blkt_offset = next_blkt;
      }

      blkt_count++;
    } /* End of looping through blockettes */

    /* Check that the blockette offset is within the maximum record size */
    if (blkt_offset > maxreclen)
    {
      ms_log (2, "%s: Blockette offset (%d) beyond maximum record length (%d)\n",
              tsid, blkt_offset, maxreclen);
      retval++;
    }

    /* Check that the data and blockette offsets are within the record */
    if (b1000reclen && MS2FSDH_DATAOFFSET (record) > b1000reclen)
    {
      ms_log (2, "%s: Data offset (%d) beyond record length (%d)\n",
              tsid, MS2FSDH_DATAOFFSET (record), b1000reclen);
      retval++;
    }
    if (b1000reclen && fsdh->blockette_offset > b1000reclen)
    {
      ms_log (2, "%s: Blockette offset (%d) beyond record length (%d)\n",
              tsid, MS2FSDH_BLOCKETTEOFFSET (record), b1000reclen);
      retval++;
    }

    /* Check that the data offset is beyond the end of the blockettes */
    if (fsdh->numsamples && fsdh->data_offset <= endofblockettes)
    {
      ms_log (2, "%s: Data offset (%d) is within blockette chain (end of blockettes: %d)\n",
              tsid, MS2FSDH_DATAOFFSET (record), endofblockettes);
      retval++;
    }

    /* Check that the correct number of blockettes were parsed */
    if (fsdh->numblockettes != blkt_count)
    {
      ms_log (2, "%s: Specified number of blockettes (%d) not equal to those parsed (%d)\n",
              tsid, MS2FSDH_NUMBLOCKETTES (record), blkt_count);
      retval++;
    }
  }

  return retval;
} /* End of ms2_parse_raw() */
