#include <tau/tau.h>
#include <libmseed.h>

#include "testdata.h"

extern int cmpfiles (char *fileA, char *fileB);

/* Write test output files.  Reference files are at "data/reference-<name>" */
#define TESTFILE_TEXT_V2    "testdata-text.mseed2"
#define TESTFILE_FLOAT32_V2 "testdata-float32.mseed2"
#define TESTFILE_FLOAT64_V2 "testdata-float64.mseed2"
#define TESTFILE_INT16_V2   "testdata-int16.mseed2"
#define TESTFILE_INT32_V2   "testdata-int32.mseed2"
#define TESTFILE_STEIM1_V2  "testdata-steim1.mseed2"
#define TESTFILE_STEIM2_V2  "testdata-steim2.mseed2"
#define TESTFILE_DEFAULTS_V2  "testdata-defaults.mseed2"
#define TESTFILE_NSEC_V2    "testdata-nsec.mseed2"
#define TESTFILE_OLDEN_V2   "testdata-olden.mseed2"
#define TESTFILE_MSTLPACK_V2  "testdata-mstlpack.mseed2"

#define TESTFILE_TEXT_V3    "testdata-text.mseed3"
#define TESTFILE_FLOAT32_V3 "testdata-float32.mseed3"
#define TESTFILE_FLOAT64_V3 "testdata-float64.mseed3"
#define TESTFILE_INT16_V3   "testdata-int16.mseed3"
#define TESTFILE_INT32_V3   "testdata-int32.mseed3"
#define TESTFILE_STEIM1_V3  "testdata-steim1.mseed3"
#define TESTFILE_STEIM2_V3  "testdata-steim2.mseed3"
#define TESTFILE_DEFAULTS_V3  "testdata-defaults.mseed3"
#define TESTFILE_NSEC_V3    "testdata-nsec.mseed3"
#define TESTFILE_OLDEN_V3   "testdata-olden.mseed3"
#define TESTFILE_MSTLPACK_V3  "testdata-mstlpack.mseed3"

TEST (write, msr3_writemseed_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  double dsinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
    dsinedata[idx] = (double)(fsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  /* Set up record parameters */
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");

  /* Text encoding */
  strcpy (msr->sid, "FDSN:XX_TEST__L_O_G");
  msr->samprate    = 0;
  msr->encoding    = DE_TEXT;
  msr->numsamples  = strlen (textdata);
  msr->datasamples = textdata;
  msr->sampletype  = 't';

  rv = msr3_writemseed (msr, TESTFILE_TEXT_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_TEXT_V3, "data/reference-" TESTFILE_TEXT_V3), "Text encoding write mismatch");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;

  /* Float32 encoding*/
  msr->encoding = DE_FLOAT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = fsinedata;
  msr->sampletype  = 'f';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT32_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT32_V3, "data/reference-" TESTFILE_FLOAT32_V3), "Float32 encoding write mismatch");

  /* Float64 encoding */
  msr->encoding = DE_FLOAT64;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = dsinedata;
  msr->sampletype  = 'd';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT64_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT64_V3, "data/reference-" TESTFILE_FLOAT64_V3), "Float64 encoding write mismatch");

  /* Int16 encoding */
  msr->encoding = DE_INT16;
  msr->numsamples  = 220; /* Limit to first 220 samples, which can be represented in 16-bits */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V3, "data/reference-" TESTFILE_INT16_V3), "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V3, "data/reference-" TESTFILE_INT32_V3), "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V3, "data/reference-" TESTFILE_STEIM1_V3), "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V3, "data/reference-" TESTFILE_STEIM2_V3), "Steim2 encoding write mismatch");

  /* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_DEFAULTS_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_DEFAULTS_V3, "data/reference-" TESTFILE_DEFAULTS_V3), "Default encoding/reclen write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;

  /* Set miniSEED v2 flag */
  flags |= MSF_PACKVER2;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");
  msr->reclen = 512;

  /* Text encoding */
  strcpy (msr->sid, "FDSN:XX_TEST__L_O_G");
  msr->samprate    = 0;
  msr->encoding    = DE_TEXT;
  msr->numsamples  = strlen (textdata);
  msr->datasamples = textdata;
  msr->sampletype  = 't';

  rv = msr3_writemseed (msr, TESTFILE_TEXT_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_TEXT_V2, "data/reference-" TESTFILE_TEXT_V2), "Text encoding write mismatch");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;

  /* Float32 encoding*/
  msr->encoding = DE_FLOAT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = fsinedata;
  msr->sampletype  = 'f';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT32_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT32_V2, "data/reference-" TESTFILE_FLOAT32_V2), "Float32 encoding write mismatch");

  /* Float64 encoding */
  msr->encoding = DE_FLOAT64;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = dsinedata;
  msr->sampletype  = 'd';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT64_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT64_V2, "data/reference-" TESTFILE_FLOAT64_V2), "Float64 encoding write mismatch");

  /* Int16 encoding */
  msr->encoding = DE_INT16;
  msr->numsamples  = 220; /* Limit to first 220 samples, which can be represented in 16-bits */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V2, "data/reference-" TESTFILE_INT16_V2), "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V2, "data/reference-" TESTFILE_INT32_V2), "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V2, "data/reference-" TESTFILE_STEIM1_V2), "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V2, "data/reference-" TESTFILE_STEIM2_V2), "Steim2 encoding write mismatch");

/* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_DEFAULTS_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_DEFAULTS_V2, "data/reference-" TESTFILE_DEFAULTS_V2), "Default encoding/reclen write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);
}

TEST (write, msr3_writemseed_nanosecond)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;
  msr->pubversion = 1;

  /* V3 Nanosecond time resolution with Int32 data and a timing exception and 512 max record length */
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr->formatversion = 3;
  msr->encoding = DE_INT32;
  msr->reclen = 512;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';
  msr->extra       = "{\"FDSN\":{"
                     "\"Time\":{"
                     "\"Exception\":[{"
                     "\"Time\":\"2012-05-12T00:00:26.987654321Z\","
                     "\"VCOCorrection\":50.7080078125,"
                     "\"ReceptionQuality\":100,"
                     "\"Count\":7654,"
                     "\"Type\":\"Valid\","
                     "\"ClockStatus\":\"Drift=-1973usec, Satellite SNR in dB=23, 0, 26, 25, 29, 28\""
                     "}]},"
                     "\"Clock\":{"
                     "\"Model\":\"Acme Corporation GPS3\""
                     "}}}";
  msr->extralength = strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_NSEC_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_NSEC_V3, "data/reference-" TESTFILE_NSEC_V3), "Nanosecond timing write mismatch");

  /* V2 Nanosecond time resolution with Int32 data and a timing exception and 512 record length */
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr->formatversion = 2;
  msr->encoding = DE_INT32;
  msr->reclen = 512;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';
  msr->extra       = "{\"FDSN\":{"
                     "\"Time\":{"
                     "\"Exception\":[{"
                     "\"Time\":\"2012-05-12T00:00:26.987654321Z\","
                     "\"VCOCorrection\":50.7080078125,"
                     "\"ReceptionQuality\":100,"
                     "\"Count\":7654,"
                     "\"Type\":\"Valid\","
                     "\"ClockStatus\":\"Drift=-1973usec, Satellite SNR in dB=23, 0, 26, 25, 29, 28\""
                     "}]},"
                     "\"Clock\":{"
                     "\"Model\":\"Acme Corporation GPS3\""
                     "}}}";
  msr->extralength = strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_NSEC_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_NSEC_V2, "data/reference-" TESTFILE_NSEC_V2), "Nanosecond timing write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);
}

TEST (write, msr3_writemseed_olden)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;
  msr->pubversion = 1;

  /* V3 Old, pre-epoch times with Int32 data and a timing exception and 4096 max record length */
  msr->starttime = ms_timestr2nstime ("1964-03-27T21:11:24.987654321Z");
  msr->formatversion = 3;
  msr->encoding = DE_INT32;
  msr->reclen = 4096;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';
  msr->extra       = "{\"FDSN\":{"
                     "\"Time\":{"
                     "\"Exception\":[{"
                     "\"Time\":\"1964-03-27T21:11:48.123456789Z\","
                     "\"Count\":1,"
                     "\"Type\":\"Unexpected\","
                     "\"ClockStatus\":\"Clock tower destroyed\""
                     "}]},"
                     "\"Clock\":{"
                     "\"Model\":\"Ye Olde Clock Tower Company\""
                     "}}}";
  msr->extralength = strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_OLDEN_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_OLDEN_V3, "data/reference-" TESTFILE_OLDEN_V3), "Old, pre-epoch times write mismatch");

  /* V2 Old, pre-epoch times with Int32 data and a timing exception and 4096 record length */
  msr->starttime = ms_timestr2nstime ("1964-03-27T21:11:24.987654321Z");
  msr->formatversion = 2;
  msr->encoding = DE_INT32;
  msr->reclen = 4096;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';
  msr->extra       = "{\"FDSN\":{"
                     "\"Time\":{"
                     "\"Exception\":[{"
                     "\"Time\":\"1964-03-27T21:11:48.123456789Z\","
                     "\"Count\":1,"
                     "\"Type\":\"Unexpected\","
                     "\"ClockStatus\":\"Clock tower destroyed\""
                     "}]},"
                     "\"Clock\":{"
                     "\"Model\":\"Ye Olde Clock Tower Company\""
                     "}}}";
  msr->extralength = strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_OLDEN_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_OLDEN_V2, "data/reference-" TESTFILE_OLDEN_V2), "Old, pre-epoch times write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);
}

TEST (write, tracelist)
{
  MS3Record *msr = NULL;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  int idx;
  int64_t rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Set up record parameters */
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate    = 40.0;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = sinedata;
  msr->sampletype  = 'i';

  seg = mstl3_addmsr (mstl, msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  rv = mstl3_writemseed (mstl, TESTFILE_STEIM2_V3 ".trace", 1, 512, DE_STEIM2, flags, 0);
  REQUIRE (rv == 4, "mstl3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V3 ".trace", "data/reference-" TESTFILE_STEIM2_V3), "Steim2 encoding trace write mismatch");

  mstl3_free (&mstl, 0);

  msr->datasamples = NULL;
  msr3_free (&msr);
}

/***************************************************************************
 *
 * Internal record handler.  The handler data should be a pointer to
 * an open file descriptor (FILE *) to which records will be written.
 *
 ***************************************************************************/
static void
record_handler_int (char *record, int reclen, void *ofp)
{
  if (fwrite (record, reclen, 1, (FILE *)ofp) != 1)
  {
    ms_log (2, "Error writing to output file\n");
  }
} /* End of ms_record_handler_int() */

TEST (pack_v2, tracelist)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t sinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = sinedata;
  msr.sampletype = 'i';

  /* Add first half of a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES / 2;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z channel */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add second half of a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_sampletime (msr.starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES - SINE_DATA_SAMPLES / 2;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_V2, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Pack miniSEED records, flushing data buffers (adding MSF_FLUSHDATA flag) */
  flags = MSF_FLUSHDATA | MSF_PACKVER2;
  int64_t packedsamples = 0;
  rv = mstl3_pack (mstl, record_handler_int, ofp, 512, DE_STEIM1, &packedsamples, flags, 0, NULL);
  REQUIRE (rv == 8, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_V2, "data/reference-" TESTFILE_MSTLPACK_V2), "Trace list packing v2 mismatch");

  mstl3_free (&mstl, 0);
}

TEST (pack_v3, tracelist)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t sinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = sinedata;
  msr.sampletype = 'i';

  /* Add first half of a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES / 2;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z channel */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add second half of a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_sampletime (msr.starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES - SINE_DATA_SAMPLES / 2;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_V3, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Pack miniSEED records, flushing data buffers (adding MSF_FLUSHDATA flag) */
  flags = MSF_FLUSHDATA;
  int64_t packedsamples = 0;
  rv = mstl3_pack (mstl, record_handler_int, ofp, 512, DE_STEIM1, &packedsamples, flags, 0, NULL);
  REQUIRE (rv == 8, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_V3, "data/reference-" TESTFILE_MSTLPACK_V3),
         "Trace list packing v3 mismatch");

  mstl3_free (&mstl, 0);
}