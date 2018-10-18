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
#include <math.h>

#include "libmseed.h"
#include "mseedformat.h"
#include "unpack.h"
#include "unpackdata.h"

/* Test POINTER for alignment with BYTE_COUNT sized quantities */
#define is_aligned(POINTER, BYTE_COUNT) \
  (((uintptr_t) (const void *)(POINTER)) % (BYTE_COUNT) == 0)

/***************************************************************************
 * msr3_unpack_mseed3:
 *
 * Unpack a miniSEED 3.x data record and populate a MS3Record struct.
 *
 * If MSF_UNPACKDATA is set in flags, the data samples are
 * unpacked/decompressed and the MS3Record->datasamples pointer is set
 * appropriately.  The data samples will be either 32-bit integers,
 * 32-bit floats or 64-bit floats (doubles) with the same byte order
 * as the host machine.  The MS3Record->numsamples will be set to the
 * actual number of samples unpacked/decompressed and
 * MS3Record->sampletype will indicated the sample type.
 *
 * If MSF_VALIDATECRC is set in flags, the CRC in the record will be
 * validated.  If the calculated CRC does not match, the MS_INVALIDCRC
 * error is returned.
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
                    uint32_t flags, int8_t verbose)
{
  MS3Record *msr = NULL;
  uint32_t calculated_crc;
  uint32_t header_crc;
  uint8_t sidlength = 0;
  int8_t swapflag;
  int bigendianhost = ms_bigendianhost ();
  int retval;

  if (!record)
  {
    ms_log (2, "msr3_unpack_mseed3(): record argument must be specified\n");
    return MS_GENERROR;
  }

  if (!ppmsr)
  {
    ms_log (2, "msr3_unpack_mseed3(): ppmsr argument cannot be NULL\n");
    return MS_GENERROR;
  }

  /* Verify that passed record length is within supported range */
  if (reclen < MINRECLEN || reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_unpack_mseed2(): Record length is out of allowed range: %d\n", reclen);
    return MS_OUTOFRANGE;
  }

  /* Verify that record includes a valid header */
  if (!MS3_ISVALIDHEADER (record))
  {
    ms_log (2, "msr3_unpack_mseed3() Record header unrecognized, not a valid miniSEED record\n");
    return MS_NOTSEED;
  }

  /* miniSEED 3 is little endian */
  swapflag = (bigendianhost) ? 1 : 0;

  if (verbose > 2)
  {
    if (swapflag)
      ms_log (1, "Byte swapping needed for unpacking of header\n");
    else
      ms_log (1, "Byte swapping NOT needed for unpacking of header\n");
  }

  sidlength = *pMS3FSDH_SIDLENGTH (record);

  /* Record SID length must be at most one less than maximum size to leave a byte for termination */
  if (sidlength >= sizeof (msr->sid))
  {
    ms_log (2, "msr3_unpack_mseed3(%.*s): Source identifier is longer (%d) than supported (%d)\n",
            sidlength, pMS3FSDH_SID (record), sidlength, (int)sizeof (msr->sid) - 1);
    return MS_GENERROR;
  }

  /* Validate the CRC */
  if (flags & MSF_VALIDATECRC)
  {
    /* Save header CRC, set value to 0, calculate CRC, restore CRC */
    header_crc = HO4u (*pMS3FSDH_CRC (record), swapflag);
    memset (pMS3FSDH_CRC(record), 0, sizeof(uint32_t));
    calculated_crc = ms_crc32c ((const uint8_t*)record, reclen, 0);
    *pMS3FSDH_CRC(record) = HO4u (header_crc, swapflag);

    if (header_crc != calculated_crc)
    {
      ms_log (2, "msr3_unpack_mseed3(%.*s) CRC is invalid, miniSEED record may be corrupt\n",
              sidlength, pMS3FSDH_SID (record));
      return MS_INVALIDCRC;
    }
  }

  /* Initialize the MS3Record */
  if (!(*ppmsr = msr3_init (*ppmsr)))
    return MS_GENERROR;

  /* Shortcut pointer, historical and helps readability */
  msr = *ppmsr;

  /* Set raw record pointer and record length */
  msr->record = record;
  msr->reclen = reclen;

  /* Populate the header fields */
  msr->swapflag = (swapflag) ? MSSWAP_HEADER : 0;
  msr->formatversion = *pMS3FSDH_FORMATVERSION (record);
  msr->flags = *pMS3FSDH_FLAGS (record);

  memcpy (msr->sid, pMS3FSDH_SID (record), sidlength);
  msr->starttime = ms_time2nstime (HO2u (*pMS3FSDH_YEAR (record), msr->swapflag),
                                   HO2u (*pMS3FSDH_DAY (record), msr->swapflag),
                                   *pMS3FSDH_HOUR (record),
                                   *pMS3FSDH_MIN (record),
                                   *pMS3FSDH_SEC (record),
                                   HO4u (*pMS3FSDH_NSEC (record), msr->swapflag));
  if (msr->starttime == NSTERROR)
  {
    ms_log (2, "msr3_unpack_mseed3(%.*s): Cannot convert start time to internal time stamp\n",
            sidlength, pMS3FSDH_SID (record));
    return MS_GENERROR;
  }

  msr->encoding = *pMS3FSDH_ENCODING (record);
  msr->samprate = HO8f (*pMS3FSDH_SAMPLERATE (record), msr->swapflag);
  msr->samplecnt = HO4u (*pMS3FSDH_NUMSAMPLES (record), msr->swapflag);
  msr->crc = HO4u (*pMS3FSDH_CRC (record), msr->swapflag);
  msr->pubversion = *pMS3FSDH_PUBVERSION (record);

  msr->extralength = HO2u (*pMS3FSDH_EXTRALENGTH (record), msr->swapflag);
  if (msr->extralength)
  {
    if ((msr->extra = malloc (msr->extralength)) == NULL)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Cannot allocate memory for extra headers\n", msr->sid);
      return MS_GENERROR;
    }

    memcpy (msr->extra, record + MS3FSDH_LENGTH + sidlength, msr->extralength);
  }

  msr->datalength = HO2u (*pMS3FSDH_DATALENGTH (record), msr->swapflag);

  /* Determine data payload byte swapping.
     Steim encodings are big endian.
     All other encodings are little endian, matching the header. */
  if (msr->encoding == DE_STEIM1 || msr->encoding == DE_STEIM2)
  {
    if (! bigendianhost)
      msr->swapflag |= MSSWAP_PAYLOAD;
  }
  else if (msr->swapflag & MSSWAP_HEADER)
  {
    msr->swapflag |= MSSWAP_PAYLOAD;
  }

  /* Unpack the data samples if requested */
  if ((flags & MSF_UNPACKDATA) && msr->samplecnt > 0)
  {
    retval = msr3_unpack_data (msr, verbose);

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
} /* End of msr3_unpack_mseed3() */

/***************************************************************************
 * msr3_unpack_mseed2:
 *
 * Unpack a miniSEED 2.x data record and populate a MS3Record struct.
 *
 * If MSF_UNPACKDATA is set in flags the data samples are
 * unpacked/decompressed and the MS3Record->datasamples pointer is set
 * appropriately.  The data samples will be either 32-bit integers,
 * 32-bit floats or 64-bit floats (doubles) with the same byte order
 * as the host machine.  The MS3Record->numsamples will be set to the
 * actual number of samples unpacked/decompressed and
 * MS3Record->sampletype will indicated the sample type.
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
                    uint32_t flags, int8_t verbose)
{
  int B1000offset = 0;
  int B1001offset = 0;
  int bigendianhost = ms_bigendianhost ();
  int retval;

  MS3Record *msr = NULL;
  char errorsid[64];

  int ione = 1;
  double dval;
  char sval[64];

  /* For blockette parsing */
  int blkt_offset;
  int blkt_count = 0;
  int blkt_length;
  int blkt_end = 0;
  uint16_t blkt_type;
  uint16_t next_blkt;

  MSEHEventDetection eventdetection;
  MSEHCalibration calibration;
  MSEHTimingException exception;

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
    ms2_recordsid (record, errorsid, sizeof (errorsid));
    ms_log (2, "msr3_unpack_mseed2(%s): Record length is out of allowd range: %d\n",
            errorsid, reclen);

    return MS_OUTOFRANGE;
  }

  /* Verify that record includes a valid header */
  if (!MS2_ISVALIDHEADER (record))
  {
    ms2_recordsid (record, errorsid, sizeof (errorsid));
    ms_log (2, "msr3_unpack_mseed2(%s) Record header unrecognized, not a valid miniSEED record\n",
            errorsid);

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
    msr->swapflag = MSSWAP_HEADER;

  /* Report byte swapping status */
  if (verbose > 2)
  {
    if (msr->swapflag)
      ms_log (1, "Byte swapping needed for unpacking of header\n");
    else
      ms_log (1, "Byte swapping NOT needed for unpacking of header\n");
  }

  /* Populate some of the common header fields */
  ms2_recordsid (record, msr->sid, sizeof (msr->sid));
  msr->formatversion = 2;
  msr->samprate = ms_nomsamprate (HO2d (*pMS2FSDH_SAMPLERATEFACT (record), msr->swapflag),
                                  HO2d (*pMS2FSDH_SAMPLERATEMULT (record), msr->swapflag));
  msr->samplecnt = HO2u (*pMS2FSDH_NUMSAMPLES (record), msr->swapflag);

  /* Map data quality indicator to publication version */
  if (*pMS2FSDH_DATAQUALITY (record) == 'M')
    msr->pubversion = 4;
  else if (*pMS2FSDH_DATAQUALITY (record) == 'Q')
    msr->pubversion = 3;
  else if (*pMS2FSDH_DATAQUALITY (record) == 'D')
    msr->pubversion = 2;
  else if (*pMS2FSDH_DATAQUALITY (record) == 'R')
    msr->pubversion = 1;
  else
    msr->pubversion = 0;

  /* Map activity bits */
  if (*pMS2FSDH_ACTFLAGS (record) & 0x01) /* Bit 0 */
    msr->flags |= 0x01;
  if (*pMS2FSDH_ACTFLAGS (record) & 0x04) /* Bit 2 */
    mseh_set_boolean (msr, "FDSN.Event.Begin", &ione);
  if (*pMS2FSDH_ACTFLAGS (record) & 0x08) /* Bit 3 */
    mseh_set_boolean (msr, "FDSN.Event.End", &ione);
  if (*pMS2FSDH_ACTFLAGS (record) & 0x10) /* Bit 4 */
  {
    dval = 1;
    mseh_set_number (msr, "FDSN.Time.LeapSecond", &dval);
  }
  if (*pMS2FSDH_ACTFLAGS (record) & 0x20) /* Bit 5 */
  {
    dval = -1;
    mseh_set_number (msr, "FDSN.Time.LeapSecond", &dval);
  }
  if (*pMS2FSDH_ACTFLAGS (record) & 0x40) /* Bit 6 */
    mseh_set_boolean (msr, "FDSN.Event.InProgress", &ione);

  /* Map I/O and clock flags */
  if (*pMS2FSDH_IOFLAGS (record) & 0x01) /* Bit 0 */
    mseh_set_boolean (msr, "FDSN.Flags.StationVolumeParityError", &ione);
  if (*pMS2FSDH_IOFLAGS (record) & 0x02) /* Bit 1 */
    mseh_set_boolean (msr, "FDSN.Flags.LongRecordRead", &ione);
  if (*pMS2FSDH_IOFLAGS (record) & 0x04) /* Bit 2 */
    mseh_set_boolean (msr, "FDSN.Flags.ShortRecordRead", &ione);
  if (*pMS2FSDH_IOFLAGS (record) & 0x08) /* Bit 3 */
    mseh_set_boolean (msr, "FDSN.Flags.StartOfTimeSeries", &ione);
  if (*pMS2FSDH_IOFLAGS (record) & 0x10) /* Bit 4 */
    mseh_set_boolean (msr, "FDSN.Flags.EndOfTimeSeries", &ione);
  if (*pMS2FSDH_IOFLAGS (record) & 0x20) /* Bit 5 */
    msr->flags |= 0x04;

  /* Map data quality flags */
  if (*pMS2FSDH_DQFLAGS (record) & 0x01) /* Bit 0 */
    mseh_set_boolean (msr, "FDSN.Flags.AmplifierSaturation", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x02) /* Bit 1 */
    mseh_set_boolean (msr, "FDSN.Flags.DigitizerClipping", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x04) /* Bit 2 */
    mseh_set_boolean (msr, "FDSN.Flags.Spikes", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x08) /* Bit 3 */
    mseh_set_boolean (msr, "FDSN.Flags.Glitches", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x10) /* Bit 4 */
    mseh_set_boolean (msr, "FDSN.Flags.MissingData", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x20) /* Bit 5 */
    mseh_set_boolean (msr, "FDSN.Flags.TelemetrySyncError", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x40) /* Bit 6 */
    mseh_set_boolean (msr, "FDSN.Flags.FilterCharging", &ione);
  if (*pMS2FSDH_DQFLAGS (record) & 0x80) /* Bit 7 */
    msr->flags |= 0x02;

  dval = (double) HO4u (*pMS2FSDH_TIMECORRECT (record), msr->swapflag);
  if (dval != 0.0)
  {
    dval = dval / 10000.0;
    mseh_set_number (msr, "FDSN.Time.Correction", &dval);
  }

  /* Traverse the blockettes */
  blkt_offset = HO2u (*pMS2FSDH_BLOCKETTEOFFSET (record), msr->swapflag);

  while ((blkt_offset != 0) &&
         (blkt_offset < reclen) &&
         (blkt_offset < MAXRECLEN))
  {
    /* Every blockette has a similar 4 byte header: type and next */
    memcpy (&blkt_type, record + blkt_offset, 2);
    memcpy (&next_blkt, record + blkt_offset + 2, 2);

    if (msr->swapflag)
    {
      ms_gswap2 (&blkt_type);
      ms_gswap2 (&next_blkt);
    }

    /* Get blockette length */
    blkt_length = ms2_blktlen (blkt_type, record + blkt_offset, msr->swapflag);

    if (blkt_length == 0)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Unknown blockette length for type %d\n",
              msr->sid, blkt_type);
      break;
    }

    /* Make sure blockette is contained within the msrecord buffer */
    if ((blkt_offset + blkt_length) > reclen)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Blockette %d extends beyond record size, truncated?\n",
              msr->sid, blkt_type);
      break;
    }

    blkt_end = blkt_offset + blkt_length;

    if (blkt_type == 100)
    {
      msr->samprate = HO4f (*pMS2B100_SAMPRATE (record + blkt_offset), msr->swapflag);
    }

    /* Blockette 200, generic event detection */
    else if (blkt_type == 200)
    {
      memset (&eventdetection, 0, sizeof(eventdetection));

      strncpy (eventdetection.type, "GENERIC", sizeof (eventdetection.type));
      ms_strncpcleantail (eventdetection.detector, pMS2B200_DETECTOR (record + blkt_offset), 24);
      eventdetection.signalamplitude = HO4f (*pMS2B200_AMPLITUDE (record + blkt_offset), msr->swapflag);
      eventdetection.signalperiod = HO4f (*pMS2B200_PERIOD (record + blkt_offset), msr->swapflag);
      eventdetection.backgroundestimate = HO4f (*pMS2B200_BACKGROUNDEST (record + blkt_offset), msr->swapflag);

      /* If bit 2 is set, set compression wave according to bit 0 */
      if (*pMS2B200_FLAGS (record + blkt_offset) & 0x04)
      {
        if (*pMS2B200_FLAGS (record + blkt_offset) & 0x01)
          strncpy (eventdetection.detectionwave, "DILATATION", sizeof (eventdetection.detectionwave));
        else
          strncpy (eventdetection.detectionwave, "COMPRESSION", sizeof (eventdetection.detectionwave));
      }
      else
        eventdetection.detectionwave[0] = '\0';

      if (*pMS2B200_FLAGS (record + blkt_offset) & 0x02)
        strncpy (eventdetection.units, "DECONVOLVED", sizeof (eventdetection.units));
      else
        strncpy (eventdetection.units, "COUNTS", sizeof (eventdetection.units));

      eventdetection.onsettime = ms_btime2nstime ((uint8_t*)pMS2B200_YEAR (record + blkt_offset), msr->swapflag);
      if (eventdetection.onsettime == NSTERROR)
        return MS_GENERROR;

      memset (eventdetection.medsnr, 0, 6);
      eventdetection.medlookback = -1;
      eventdetection.medpickalgorithm = -1;
      eventdetection.next = NULL;

      if (mseh_add_event_detection (msr, NULL, &eventdetection))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 200 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 201, Murdock event detection */
    else if (blkt_type == 201)
    {
      memset (&eventdetection, 0, sizeof(eventdetection));

      strncpy (eventdetection.type, "MURDOCK", sizeof (eventdetection.type));
      ms_strncpcleantail (eventdetection.detector, pMS2B201_DETECTOR (record + blkt_offset), 24);
      eventdetection.signalamplitude = HO4f (*pMS2B201_AMPLITUDE (record + blkt_offset), msr->swapflag);
      eventdetection.signalperiod = HO4f (*pMS2B201_PERIOD (record + blkt_offset), msr->swapflag);
      eventdetection.backgroundestimate = HO4f (*pMS2B201_BACKGROUNDEST (record + blkt_offset), msr->swapflag);

      /* If bit 0 is set, dilatation wave otherwise compression */
      if (*pMS2B201_FLAGS (record + blkt_offset) & 0x01)
        strncpy (eventdetection.detectionwave, "DILATATION", sizeof (eventdetection.detectionwave));
      else
        strncpy (eventdetection.detectionwave, "COMPRESSION", sizeof (eventdetection.detectionwave));

      eventdetection.onsettime = ms_btime2nstime ((uint8_t*)pMS2B201_YEAR (record + blkt_offset), msr->swapflag);
      if (eventdetection.onsettime == NSTERROR)
        return MS_GENERROR;

      memcpy (eventdetection.medsnr, pMS2B201_MEDSNR (record + blkt_offset), 6);
      eventdetection.medlookback = *pMS2B201_LOOPBACK (record + blkt_offset);
      eventdetection.medpickalgorithm = *pMS2B201_PICKALGORITHM (record + blkt_offset);
      eventdetection.next = NULL;

      if (mseh_add_event_detection (msr, NULL, &eventdetection))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 201 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 300, step calibration */
    else if (blkt_type == 300)
    {
      memset (&calibration, 0, sizeof(calibration));

      strncpy (calibration.type, "STEP", sizeof (calibration.type));

      calibration.begintime = ms_btime2nstime ((uint8_t*)pMS2B300_YEAR (record + blkt_offset), msr->swapflag);
      if (calibration.begintime == NSTERROR)
        return MS_GENERROR;

      calibration.endtime = NSTERROR;
      calibration.steps = *pMS2B300_NUMCALIBRATIONS (record + blkt_offset);

      /* If bit 0 is set, first puluse is positive */
      calibration.firstpulsepositive = -1;
      if (*pMS2B300_FLAGS (record + blkt_offset) & 0x01)
        calibration.firstpulsepositive = 1;

      /* If bit 1 is set, calibration's alternate sign */
      calibration.alternatesign = -1;
      if (*pMS2B300_FLAGS (record + blkt_offset) & 0x02)
        calibration.alternatesign = 1;

      /* If bit 2 is set, calibration is automatic, otherwise manual */
      if (*pMS2B300_FLAGS (record + blkt_offset) & 0x04)
        strncpy (calibration.trigger, "AUTOMATIC", sizeof (calibration.trigger));
      else
        strncpy (calibration.trigger, "MANUAL", sizeof (calibration.trigger));

      /* If bit 3 is set, continued from previous record */
      calibration.continued = -1;
      if (*pMS2B300_FLAGS (record + blkt_offset) & 0x08)
        calibration.continued = 1;

      calibration.duration = (double)(HO4u (*pMS2B300_STEPDURATION (record + blkt_offset), msr->swapflag) / 10000.0);
      calibration.stepbetween = (double)(HO4u (*pMS2B300_INTERVALDURATION (record + blkt_offset), msr->swapflag) / 10000.0);
      calibration.amplitude = HO4f (*pMS2B300_AMPLITUDE (record + blkt_offset), msr->swapflag);
      ms_strncpcleantail (calibration.inputchannel, pMS2B300_INPUTCHANNEL (record + blkt_offset), 3);
      calibration.inputunits[0] = '\0';
      calibration.amplituderange[0] = '\0';
      calibration.sineperiod = 0.0;
      calibration.refamplitude = (double)(HO4u (*pMS2B300_REFERENCEAMPLITUDE (record + blkt_offset), msr->swapflag));
      ms_strncpcleantail (calibration.coupling, pMS2B300_COUPLING (record + blkt_offset), 12);
      ms_strncpcleantail (calibration.rolloff, pMS2B300_ROLLOFF (record + blkt_offset), 12);
      calibration.noise[0] = '\0';
      calibration.next = NULL;

      if (mseh_add_calibration (msr, NULL, &calibration))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 300 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 310, sine calibration */
    else if (blkt_type == 310)
    {
      memset (&calibration, 0, sizeof(calibration));

      strncpy (calibration.type, "SINE", sizeof (calibration.type));

      calibration.begintime = ms_btime2nstime ((uint8_t*)pMS2B310_YEAR (record + blkt_offset), msr->swapflag);
      if (calibration.begintime == NSTERROR)
        return MS_GENERROR;

      calibration.endtime = NSTERROR;
      calibration.steps = -1;
      calibration.firstpulsepositive = -1;
      calibration.alternatesign = -1;

      /* If bit 2 is set, calibration is automatic, otherwise manual */
      if (*pMS2B310_FLAGS (record + blkt_offset) & 0x04)
        strncpy (calibration.trigger, "AUTOMATIC", sizeof (calibration.trigger));
      else
        strncpy (calibration.trigger, "MANUAL", sizeof (calibration.trigger));

      /* If bit 3 is set, continued from previous record */
      calibration.continued = -1;
      if (*pMS2B310_FLAGS (record + blkt_offset) & 0x08)
        calibration.continued = 1;

      calibration.amplituderange[0] = '\0';
      /* If bit 4 is set, peak to peak amplitude */
      if (*pMS2B310_FLAGS (record + blkt_offset) & 0x10)
        strncpy (calibration.amplituderange, "PEAKTOPEAK", sizeof (calibration.amplituderange));
      /* Otherwise, if bit 5 is set, zero to peak amplitude */
      else if (*pMS2B310_FLAGS (record + blkt_offset) & 0x20)
        strncpy (calibration.amplituderange, "ZEROTOPEAK", sizeof (calibration.amplituderange));
      /* Otherwise, if bit 6 is set, RMS amplitude */
      else if (*pMS2B310_FLAGS (record + blkt_offset) & 0x40)
        strncpy (calibration.amplituderange, "RMS", sizeof (calibration.amplituderange));

      calibration.duration = (double)(HO4u (*pMS2B310_DURATION (record + blkt_offset), msr->swapflag) / 10000.0);
      calibration.sineperiod = HO4f (*pMS2B310_PERIOD (record + blkt_offset), msr->swapflag);
      calibration.amplitude = HO4f (*pMS2B310_AMPLITUDE (record + blkt_offset), msr->swapflag);
      ms_strncpcleantail (calibration.inputchannel, pMS2B310_INPUTCHANNEL (record + blkt_offset), 3);
      calibration.refamplitude = (double)(HO4u (*pMS2B310_REFERENCEAMPLITUDE (record + blkt_offset), msr->swapflag));
      calibration.stepbetween = 0.0;
      calibration.inputunits[0] = '\0';
      ms_strncpcleantail (calibration.coupling, pMS2B310_COUPLING (record + blkt_offset), 12);
      ms_strncpcleantail (calibration.rolloff, pMS2B310_ROLLOFF (record + blkt_offset), 12);
      calibration.noise[0] = '\0';
      calibration.next = NULL;

      if (mseh_add_calibration (msr, NULL, &calibration))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 310 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 320, pseudo-random calibration */
    else if (blkt_type == 320)
    {
      memset (&calibration, 0, sizeof(calibration));

      strncpy (calibration.type, "PSEUDORANDOM", sizeof (calibration.type));

      calibration.begintime = ms_btime2nstime ((uint8_t*)pMS2B320_YEAR (record + blkt_offset), msr->swapflag);
      if (calibration.begintime == NSTERROR)
        return MS_GENERROR;

      calibration.endtime = NSTERROR;
      calibration.steps = -1;
      calibration.firstpulsepositive = -1;
      calibration.alternatesign = -1;

      /* If bit 2 is set, calibration is automatic, otherwise manual */
      if (*pMS2B320_FLAGS (record + blkt_offset) & 0x04)
        strncpy (calibration.trigger, "AUTOMATIC", sizeof (calibration.trigger));
      else
        strncpy (calibration.trigger, "MANUAL", sizeof (calibration.trigger));

      /* If bit 3 is set, continued from previous record */
      calibration.continued = -1;
      if (*pMS2B320_FLAGS (record + blkt_offset) & 0x08)
        calibration.continued = 1;

      calibration.amplituderange[0] = '\0';
      /* If bit 4 is set, peak to peak amplitude */
      if (*pMS2B320_FLAGS (record + blkt_offset) & 0x10)
        strncpy (calibration.amplituderange, "RANDOM", sizeof (calibration.amplituderange));

      calibration.duration = (double)(HO4u (*pMS2B320_DURATION (record + blkt_offset), msr->swapflag) / 10000.0);
      calibration.amplitude = HO4f (*pMS2B320_PTPAMPLITUDE (record + blkt_offset), msr->swapflag);
      ms_strncpcleantail (calibration.inputchannel, pMS2B320_INPUTCHANNEL (record + blkt_offset), 3);
      calibration.refamplitude = (double)(HO4u (*pMS2B320_REFERENCEAMPLITUDE (record + blkt_offset), msr->swapflag));
      calibration.sineperiod = 0.0;
      calibration.stepbetween = 0.0;
      calibration.inputunits[0] = '\0';
      ms_strncpcleantail (calibration.coupling, pMS2B320_COUPLING (record + blkt_offset), 12);
      ms_strncpcleantail (calibration.rolloff, pMS2B320_ROLLOFF (record + blkt_offset), 12);
      ms_strncpcleantail (calibration.noise, pMS2B320_NOISETYPE (record + blkt_offset), 8);
      calibration.next = NULL;

      if (mseh_add_calibration (msr, NULL, &calibration))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 320 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 390, generic calibration */
    else if (blkt_type == 390)
    {
      memset (&calibration, 0, sizeof(calibration));

      strncpy (calibration.type, "GENERIC", sizeof (calibration.type));

      calibration.begintime = ms_btime2nstime ((uint8_t*)pMS2B390_YEAR (record + blkt_offset), msr->swapflag);
      if (calibration.begintime == NSTERROR)
        return MS_GENERROR;

      calibration.endtime = NSTERROR;
      calibration.steps = -1;
      calibration.firstpulsepositive = -1;
      calibration.alternatesign = -1;

      /* If bit 2 is set, calibration is automatic, otherwise manual */
      if (*pMS2B390_FLAGS (record + blkt_offset) & 0x04)
        strncpy (calibration.trigger, "AUTOMATIC", sizeof (calibration.trigger));
      else
        strncpy (calibration.trigger, "MANUAL", sizeof (calibration.trigger));

      /* If bit 3 is set, continued from previous record */
      calibration.continued = -1;
      if (*pMS2B390_FLAGS (record + blkt_offset) & 0x08)
        calibration.continued = 1;

      calibration.amplituderange[0] = '\0';
      calibration.duration = (double)(HO4u (*pMS2B390_DURATION (record + blkt_offset), msr->swapflag) / 10000.0);
      calibration.amplitude = HO4f (*pMS2B390_AMPLITUDE (record + blkt_offset), msr->swapflag);
      ms_strncpcleantail (calibration.inputchannel, pMS2B390_INPUTCHANNEL (record + blkt_offset), 3);
      calibration.refamplitude = 0.0;
      calibration.sineperiod = 0.0;
      calibration.stepbetween = 0.0;
      calibration.inputunits[0] = '\0';
      calibration.coupling[0] = '\0';
      calibration.rolloff[0] = '\0';
      calibration.noise[0] = '\0';
      calibration.next = NULL;

      if (mseh_add_calibration (msr, NULL, &calibration))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 390 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 395, calibration abort */
    else if (blkt_type == 395)
    {
      memset (&calibration, 0, sizeof(calibration));

      strncpy (calibration.type, "ABORT", sizeof (calibration.type));

      calibration.begintime = NSTERROR;

      calibration.endtime = ms_btime2nstime ((uint8_t*)pMS2B395_YEAR (record + blkt_offset), msr->swapflag);
      if (calibration.endtime == NSTERROR)
        return MS_GENERROR;

      calibration.steps = -1;
      calibration.firstpulsepositive = -1;
      calibration.alternatesign = -1;
      calibration.trigger[0] = '\0';
      calibration.continued = -1;
      calibration.amplituderange[0] = '\0';
      calibration.duration = 0.0;
      calibration.amplitude = 0.0;
      calibration.inputchannel[0] = '\0';
      calibration.refamplitude = 0.0;
      calibration.sineperiod = 0.0;
      calibration.stepbetween = 0.0;
      calibration.inputunits[0] = '\0';
      calibration.coupling[0] = '\0';
      calibration.rolloff[0] = '\0';
      calibration.noise[0] = '\0';
      calibration.next = NULL;

      if (mseh_add_calibration (msr, NULL, &calibration))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 395 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }
    }

    /* Blockette 400, beam blockette */
    else if (blkt_type == 400)
    {
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING Blockette 400 is present but discarded\n", msr->sid);
    }

    /* Blockette 400, beam delay blockette */
    else if (blkt_type == 405)
    {
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING Blockette 405 is present but discarded\n", msr->sid);
    }

    /* Blockette 500, timing blockette */
    else if (blkt_type == 500)
    {
      memset (&exception, 0, sizeof(exception));

      exception.vcocorrection = HO4f (*pMS2B500_VCOCORRECTION (record + blkt_offset), msr->swapflag);

      exception.time = ms_btime2nstime ((uint8_t*)pMS2B500_YEAR (record + blkt_offset), msr->swapflag);
      if (exception.time == NSTERROR)
        return MS_GENERROR;

      exception.usec = *pMS2B500_MICROSECOND (record + blkt_offset);
      exception.receptionquality = *pMS2B500_RECEPTIONQUALITY (record + blkt_offset);
      exception.count = HO4u (*pMS2B500_EXCEPTIONCOUNT (record + blkt_offset), msr->swapflag);
      ms_strncpcleantail (exception.type, pMS2B500_EXCEPTIONTYPE (record + blkt_offset), 16);
      ms_strncpcleantail (exception.clockstatus, pMS2B500_CLOCKSTATUS (record + blkt_offset), 128);

      if (mseh_add_timing_exception (msr, NULL, &exception))
      {
        ms_log (2, "msr3_unpack_mseed2(%s): Problem mapping Blockette 500 to extra headers\n", msr->sid);
        return MS_GENERROR;
      }

      /* Clock model maps to a single value at FDSN.Clock.Model */
      ms_strncpcleantail (sval, pMS2B500_CLOCKMODEL (record + blkt_offset), 32);
      mseh_set_string (msr, "FDSN.Clock.Model", sval);
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
                msr->sid, msr->reclen, reclen);
      }

      msr->encoding = *pMS2B1000_ENCODING (record + blkt_offset);
    }

    else if (blkt_type == 1001)
    {
      B1001offset = blkt_offset;

      dval = (double) *pMS2B1001_TIMINGQUALITY (record + blkt_offset);
      mseh_set_number (msr, "FDSN.Time.Quality", &dval);
    }

    else if (blkt_type == 2000)
    {
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING Blockette 2000 is present but discarded\n", msr->sid);
    }

    else
    { /* Unknown blockette type */
      ms_log (1, "msr3_unpack_mseed2(%s): WARNING, unsupported blockette type %d, skipping\n", msr->sid);
    }

    /* Check that the next blockette offset is beyond the current blockette */
    if (next_blkt && next_blkt < (blkt_offset + blkt_length))
    {
      ms_log (2, "msr2_unpack_mseed2(%s): Offset to next blockette (%d) is within current blockette ending at byte %d\n",
              msr->sid, next_blkt, (blkt_offset + blkt_length));

      blkt_offset = 0;
    }
    /* Check that the offset is within record length */
    else if (next_blkt && next_blkt > reclen)
    {
      ms_log (2, "msr3_unpack_mseed2(%s): Offset to next blockette (%d) from type %d is beyond record length\n",
              msr->sid, next_blkt, blkt_type);

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
      ms_log (1, "%s: Warning: No Blockette 1000 found\n", msr->sid);
    }
  }

  /* Check that the data offset is after the blockette chain */
  if (blkt_end &&
      HO2u (*pMS2FSDH_NUMSAMPLES (record), msr->swapflag) &&
      HO2u (*pMS2FSDH_DATAOFFSET (record), msr->swapflag) < blkt_end)
  {
    ms_log (1, "%s: Warning: Data offset in fixed header (%d) is within the blockette chain ending at %d\n",
            msr->sid, HO2u (*pMS2FSDH_DATAOFFSET (record), msr->swapflag), blkt_end);
  }

  /* Check that the blockette count matches the number parsed */
  if (*pMS2FSDH_NUMBLOCKETTES (record) != blkt_count)
  {
    ms_log (1, "%s: Warning: Number of blockettes in fixed header (%d) does not match the number parsed (%d)\n",
            msr->sid, *pMS2FSDH_NUMBLOCKETTES (record), blkt_count);
  }

  /* Calculate start time */
  msr->starttime = ms_btime2nstime ((uint8_t*)pMS2FSDH_YEAR (record), msr->swapflag);
  if (msr->starttime == NSTERROR)
  {
    ms_log (2, "msr3_unpack_mseed2(%s): Cannot convert start time to internal time stamp\n",
            msr->sid);
    return MS_GENERROR;
  }

  /* Check if a time correction is included and if it has been applied,
   * bit 1 of activity flags indicates if it has been appiled */
  if (HO4d (*pMS2FSDH_TIMECORRECT (record), msr->swapflag) != 0 &&
      !(*pMS2FSDH_ACTFLAGS (record) & 0x02))
  {
    msr->starttime += (nstime_t)HO4d (*pMS2FSDH_TIMECORRECT (record), msr->swapflag) * (NSTMODULUS / 10000);
  }

  /* Apply microsecond precision if Blockette 1001 is present */
  if (B1001offset)
  {
    msr->starttime += (nstime_t)*pMS2B1001_MICROSECOND (record + B1001offset) * (NSTMODULUS / 1000000);
  }

  msr->datalength = msr->reclen - HO2u (*pMS2FSDH_DATAOFFSET (record), msr->swapflag);

  /* Determine byte order of the data and set the swapflag as needed;
     if no Blkt1000, assume the order is the same as the header */
  if (B1000offset)
  {
    /* If BE host and LE data need swapping */
    if (bigendianhost && *pMS2B1000_BYTEORDER (record + B1000offset) == 0)
      msr->swapflag |= MSSWAP_PAYLOAD;
    /* If LE host and BE data (or bad byte order value) need swapping */
    else if (!bigendianhost && *pMS2B1000_BYTEORDER (record + B1000offset) > 0)
      msr->swapflag |= MSSWAP_PAYLOAD;
  }
  else if (msr->swapflag & MSSWAP_HEADER)
  {
    msr->swapflag |= MSSWAP_PAYLOAD;
  }

  /* Unpack the data samples if requested */
  if ((flags & MSF_UNPACKDATA) && msr->samplecnt > 0)
  {
    if (verbose > 2 && msr->swapflag & MSSWAP_PAYLOAD)
      ms_log (1, "%s: Byte swapping needed for unpacking of data samples\n", msr->sid);
    else if (verbose > 2)
      ms_log (1, "%s: Byte swapping NOT needed for unpacking of data samples\n", msr->sid);

    retval = msr3_unpack_data (msr, verbose);

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
 * msr3_data_bounds:
 *
 * Determine the data payload bounds, starting offset in record and
 * size, for a specified MS3Record.  For miniSEED 2.x the raw record
 * is expected to be located at the MS3Record->record pointer.
 *
 * When the encoding is a fixed length per sample (ascii, integers or
 * floats), calculate the data size based on the sample count and use
 * if less than size determined otherwise.
 *
 * When the encoding is Steim1 or Steim2, search for 64-byte padding
 * frames (all zeros) at the end of the payload and remove from the
 * reported size.
 *
 * Return 0 on success or negative libmseed error code.
 ************************************************************************/
int
msr3_data_bounds (MS3Record *msr, uint32_t *dataoffset, uint16_t *datasize)
{
  uint8_t nullframe[64] = {0};
  uint8_t samplebytes = 0;
  uint16_t rawsize;

  if (!msr || !dataoffset || !datasize)
    return MS_GENERROR;

  /* Determine offset to data */
  if (msr->formatversion == 3)
  {
    *dataoffset = MS3FSDH_LENGTH + strlen (msr->sid) + msr->extralength;
    *datasize = msr->datalength;
  }
  else if (msr->formatversion == 2)
  {
    *dataoffset = HO2u (*pMS2FSDH_DATAOFFSET (msr->record), msr->swapflag & MSSWAP_HEADER);
    *datasize = msr->reclen - *dataoffset;
  }
  else
  {
    ms_log (2, "msr3_data_bounds(%s): Unrecognized format version: %d\n",
            msr->sid, msr->formatversion);
    return MS_GENERROR;
  }

  /* If a fixed sample length encoding, calculate size and use if less
   * than otherwise determined. */
  if (msr->encoding == DE_ASCII ||
      msr->encoding == DE_INT16 || msr->encoding == DE_INT32 ||
      msr->encoding == DE_FLOAT32 || msr->encoding == DE_FLOAT64)
  {
    switch (msr->encoding)
    {
    case DE_ASCII:
      samplebytes = 1;
      break;
    case DE_INT16:
      samplebytes = 2;
      break;
    case DE_INT32:
    case DE_FLOAT32:
      samplebytes = 4;
      break;
    case DE_FLOAT64:
      samplebytes = 8;
      break;
    }

    rawsize = msr->samplecnt * samplebytes;

    if (rawsize < *datasize)
      *datasize = rawsize;
  }

  /* If datasize is a multiple of 64-bytes and a Steim encoding, test for
   * trailing, zeroed (empty) frames and subtract them from the size. */
  if (*datasize % 64 == 0 &&
      (msr->encoding == DE_STEIM1 || msr->encoding == DE_STEIM2))
  {
    while (*datasize > 0 &&
           memcmp (msr->record + (*datasize - 64), nullframe, 64) == 0)
    {
      *datasize -= 64;
    }
  }

  return 0;
} /* End of msr3_data_bounds() */

/************************************************************************
 * msr3_unpack_data:
 *
 * Unpack miniSEED data samples for a given MS3Record.  The
 * packed/encoded data is accessed in the record indicated by
 * MS3Record->record and the unpacked samples are placed in
 * MS3Record->datasamples.  The resulting data samples are either text
 * characters, 32-bit integers, 32-bit floats or 64-bit floats in host
 * byte order.
 *
 * An internal buffer is allocated if the encoded data is not aligned
 * for the sample size, which is a decent indicator of the alignment
 * needed for decoding efficiently.
 *
 * Return number of samples unpacked or negative libmseed error code.
 ************************************************************************/
int
msr3_unpack_data (MS3Record *msr, int8_t verbose)
{
  uint16_t datasize; /* byte size of data samples in record */
  int nsamples; /* number of samples unpacked */
  size_t unpacksize; /* byte size of unpacked samples */
  int8_t samplesize = 0; /* size of the data samples in bytes */
  uint32_t dataoffset = 0;
  const char *encoded = NULL;
  char *encoded_allocated = NULL;

  if (!msr)
    return MS_GENERROR;

  if (!msr->record)
  {
    ms_log (2, "msr3_unpack_data(%s): Raw record pointer is unset\n", msr->sid);
    return MS_GENERROR;
  }

  /* Check for decode debugging environment variable */
  if (getenv ("DECODE_DEBUG"))
    decodedebug = 1;

  /* Sanity check record length */
  if (msr->reclen == -1)
  {
    ms_log (2, "msr3_unpack_data(%s): Record size unknown\n", msr->sid);
    return MS_NOTSEED;
  }
  else if (msr->reclen < MINRECLEN || msr->reclen > MAXRECLEN)
  {
    ms_log (2, "msr3_unpack_data(%s): Unsupported record length: %d\n",
            msr->sid, msr->reclen);
    return MS_OUTOFRANGE;
  }

  /* Determine offset to data and length of data payload */
  if (msr3_data_bounds (msr, &dataoffset, &datasize))
    return MS_GENERROR;

  /* Sanity check data offset before creating a pointer based on the value */
  if (dataoffset < MINRECLEN || dataoffset >= msr->reclen)
  {
    ms_log (2, "msr3_unpack_data(%s): data offset value is not valid: %d\n",
            msr->sid, dataoffset);
    return MS_GENERROR;
  }

  /* Fallback encoding for when no blockette 1000 is present */
  if (msr->encoding < 0)
  {
    if (verbose > 2)
      ms_log (1, "%s: No data encoding (no blockette 1000?), assuming Steim-1\n", msr->sid);

    msr->encoding = DE_STEIM1;
  }

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
  }

  encoded = msr->record + dataoffset;

  /* Copy encoded data to aligned/malloc'd buffer if not aligned for sample size */
  if (samplesize && !is_aligned (encoded, samplesize))
  {
    if ((encoded_allocated = malloc (datasize)) == NULL)
    {
      ms_log (2, "msr3_unpack_data(): Cannot allocate memory for encoded data\n");
      return MS_GENERROR;
    }

    memcpy (encoded_allocated, encoded, datasize);
    encoded = encoded_allocated;
  }

  /* Calculate buffer size needed for unpacked samples */
  unpacksize = msr->samplecnt * samplesize;

  /* (Re)Allocate space for the unpacked data */
  if (unpacksize > 0)
  {
    msr->datasamples = realloc (msr->datasamples, unpacksize);

    if (msr->datasamples == NULL)
    {
      ms_log (2, "msr3_unpack_data(%s): Cannot (re)allocate memory\n", msr->sid);
      if (encoded_allocated)
        free (encoded_allocated);
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
    ms_log (1, "%s: Unpacking %" PRId64 " samples\n", msr->sid, msr->samplecnt);

  /* Decode data samples according to encoding */
  switch (msr->encoding)
  {
  case DE_ASCII:
    if (verbose > 1)
      ms_log (1, "%s: Found ASCII data\n", msr->sid);

    nsamples = msr->samplecnt;
    if (nsamples > 0)
    {
      memcpy (msr->datasamples, encoded, nsamples);
    }
    else
    {
      nsamples = 0;
    }
    msr->sampletype = 'a';
    break;

  case DE_INT16:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking INT16 data samples\n", msr->sid);

    nsamples = msr_decode_int16 ((int16_t *)encoded, msr->samplecnt,
                                 msr->datasamples, unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'i';
    break;

  case DE_INT32:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking INT32 data samples\n", msr->sid);

    nsamples = msr_decode_int32 ((int32_t *)encoded, msr->samplecnt,
                                 msr->datasamples, unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'i';
    break;

  case DE_FLOAT32:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking FLOAT32 data samples\n", msr->sid);

    nsamples = msr_decode_float32 ((float *)encoded, msr->samplecnt,
                                   msr->datasamples, unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'f';
    break;

  case DE_FLOAT64:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking FLOAT64 data samples\n", msr->sid);

    nsamples = msr_decode_float64 ((double *)encoded, msr->samplecnt,
                                   msr->datasamples, unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'd';
    break;

  case DE_STEIM1:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking Steim1 data frames\n", msr->sid);

    nsamples = msr_decode_steim1 ((int32_t *)encoded, datasize, msr->samplecnt,
                                  msr->datasamples, unpacksize, msr->sid, msr->swapflag & MSSWAP_PAYLOAD);

    if (nsamples < 0)
    {
      nsamples = MS_GENERROR;
      break;
    }

    msr->sampletype = 'i';
    break;

  case DE_STEIM2:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking Steim2 data frames\n", msr->sid);

    nsamples = msr_decode_steim2 ((int32_t *)encoded, datasize, msr->samplecnt,
                                  msr->datasamples, unpacksize, msr->sid, msr->swapflag & MSSWAP_PAYLOAD);

    if (nsamples < 0)
    {
      nsamples = MS_GENERROR;
      break;
    }

    msr->sampletype = 'i';
    break;

  case DE_GEOSCOPE24:
  case DE_GEOSCOPE163:
  case DE_GEOSCOPE164:
    if (verbose > 1)
    {
      if (msr->encoding == DE_GEOSCOPE24)
        ms_log (1, "%s: Unpacking GEOSCOPE 24bit integer data samples\n", msr->sid);
      if (msr->encoding == DE_GEOSCOPE163)
        ms_log (1, "%s: Unpacking GEOSCOPE 16bit gain ranged/3bit exponent data samples\n", msr->sid);
      if (msr->encoding == DE_GEOSCOPE164)
        ms_log (1, "%s: Unpacking GEOSCOPE 16bit gain ranged/4bit exponent data samples\n", msr->sid);
    }

    nsamples = msr_decode_geoscope ((char *)encoded, msr->samplecnt, msr->datasamples,
                                    unpacksize, msr->encoding, msr->sid, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'f';
    break;

  case DE_CDSN:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking CDSN encoded data samples\n", msr->sid);

    nsamples = msr_decode_cdsn ((int16_t *)encoded, msr->samplecnt, msr->datasamples,
                                unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'i';
    break;

  case DE_SRO:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking SRO encoded data samples\n", msr->sid);

    nsamples = msr_decode_sro ((int16_t *)encoded, msr->samplecnt, msr->datasamples,
                               unpacksize, msr->sid, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'i';
    break;

  case DE_DWWSSN:
    if (verbose > 1)
      ms_log (1, "%s: Unpacking DWWSSN encoded data samples\n", msr->sid);

    nsamples = msr_decode_dwwssn ((int16_t *)encoded, msr->samplecnt, msr->datasamples,
                                  unpacksize, msr->swapflag & MSSWAP_PAYLOAD);

    msr->sampletype = 'i';
    break;

  default:
    ms_log (2, "%s: Unsupported encoding format %d (%s)\n",
            msr->sid, msr->encoding, (char *)ms_encodingstr (msr->encoding));

    nsamples = MS_UNKNOWNFORMAT;
    break;
  }

  if (encoded_allocated)
    free (encoded_allocated);

  if (nsamples >= 0 && nsamples != msr->samplecnt)
  {
    ms_log (2, "msr3_unpack_data(%s): only decoded %d samples of %d expected\n",
            msr->sid, nsamples, msr->samplecnt);
    return MS_GENERROR;
  }

  if (nsamples > 0)
    msr->numsamples = nsamples;

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
 * ms2_recordsid:
 *
 * Generate an XFDSN: source identifier string for a specified raw
 * miniSEED 2.x data record.
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms2_recordsid (char *record, char *sid, int sidlen)
{
  char net[3] = {0};
  char sta[6] = {0};
  char loc[3] = {0};
  char chan[6] = {0};

  if (!record || !sid)
    return NULL;

  ms_strncpclean (net, pMS2FSDH_NETWORK (record), 2);
  ms_strncpclean (sta, pMS2FSDH_STATION (record), 5);
  ms_strncpclean (loc, pMS2FSDH_LOCATION (record), 2);

  /* Map 3 channel codes to BAND_SOURCE_POSITION */
  chan[0] = *pMS2FSDH_CHANNEL (record);
  chan[1] = '_';
  chan[2] = *(pMS2FSDH_CHANNEL (record) + 1);
  chan[3] = '_';
  chan[4] = *(pMS2FSDH_CHANNEL (record) + 2);

  if (ms_nslc2sid (sid, sidlen, 0, net, sta, loc, chan) < 0)
    return NULL;

  return sid;
} /* End of ms2_recordsid() */

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
    break;
  case 200:
    return "Generic Event Detection";
    break;
  case 201:
    return "Murdock Event Detection";
    break;
  case 300:
    return "Step Calibration";
    break;
  case 310:
    return "Sine Calibration";
    break;
  case 320:
    return "Pseudo-random Calibration";
    break;
  case 390:
    return "Generic Calibration";
    break;
  case 395:
    return "Calibration Abort";
    break;
  case 400:
    return "Beam";
    break;
  case 500:
    return "Timing";
    break;
  case 1000:
    return "Data Only SEED";
    break;
  case 1001:
    return "Data Extension";
    break;
  case 2000:
    return "Opaque Data";
    break;
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
