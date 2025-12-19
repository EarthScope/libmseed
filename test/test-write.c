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
#define TESTFILE_ODDRATE_V2 "testdata-oddrate.mseed2"
#define TESTFILE_MSTLPACK_V2 "testdata-mstlpack.mseed2"
#define TESTFILE_FLUSHIDLE_V2 "testdata-flushidle.mseed2"

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
#define TESTFILE_ODDRATE_V3 "testdata-oddrate.mseed3"
#define TESTFILE_MSTLPACK_V3  "testdata-mstlpack.mseed3"
#define TESTFILE_FLUSHIDLE_V3 "testdata-flushidle.mseed3"

#define TESTFILE_MSTLPACK_ROLLINGBUFFER "testdata-mstlpack-rollingbuffer.mseed"
#define TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER "testdata-mstlpack-rollingbuffer-next.mseed"

/* Test writing miniSEED records to a file for each supported encoding and
 * verifies the output against reference files.
 */
TEST (write, msr3_writemseed_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  float fsinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
    fsinedata[idx] = (float)(dsinedata[idx]);
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
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V3, "data/reference-" TESTFILE_INT16_V3), "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V3, "data/reference-" TESTFILE_INT32_V3), "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V3, "data/reference-" TESTFILE_STEIM1_V3), "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V3, "data/reference-" TESTFILE_STEIM2_V3), "Steim2 encoding write mismatch");

  /* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
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
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V2, "data/reference-" TESTFILE_INT16_V2), "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V2, "data/reference-" TESTFILE_INT32_V2), "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V2, "data/reference-" TESTFILE_STEIM1_V2), "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V2, "data/reference-" TESTFILE_STEIM2_V2), "Steim2 encoding write mismatch");

/* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples  = SINE_DATA_SAMPLES - 1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  rv = msr3_writemseed (msr, TESTFILE_DEFAULTS_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_DEFAULTS_V2, "data/reference-" TESTFILE_DEFAULTS_V2), "Default encoding/reclen write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);
}

/* Test writing miniSEED records to a file with nanosecond time resolution for
 * both the data sample payload and a timing exception and verifies the output
 * against reference files for both v2 and v3 miniSEED formats.
 */
TEST (write, msr3_writemseed_nanosecond)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
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
  msr->datasamples = isinedata;
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
  msr->datasamples = isinedata;
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

/* Test writing miniSEED records to a file with old, pre-epoch data samples and
 * a timing exception and verifies the output against reference files for both
 * v2 and v3 miniSEED formats.
 */
TEST (write, msr3_writemseed_olden)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
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
  msr->datasamples = isinedata;
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
  msr->datasamples = isinedata;
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

/* Test writing miniSEED records to a file with an odd sample rate and verifies
 * the output against reference files for both v2 and v3 miniSEED formats.
 *
 * The target odd sample rate is 1080.0 samples/second, which is a sample period
 * with repeating decimal representation, which exercises the rounding and
 * truncation of the sample time calculation.
 */
TEST (write, msr3_writemseed_oddrate)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  int idx;
  int rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2025-05-12T21:11:24.987654321Z");
  msr->encoding = DE_INT32;
  msr->reclen = 512;
  msr->numsamples  = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype  = 'i';

  /* Odd rate (1080.0) with an repeating decimal period */
  msr->samprate = 1080.0;

  /* V3 */
  msr->formatversion = 3;

  rv = msr3_writemseed (msr, TESTFILE_ODDRATE_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_ODDRATE_V3, "data/reference-" TESTFILE_ODDRATE_V3), "Odd rate write mismatch");

  /* V2 */
  msr->formatversion = 2;

  rv = msr3_writemseed (msr, TESTFILE_ODDRATE_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_ODDRATE_V2, "data/reference-" TESTFILE_ODDRATE_V2), "Odd rate write mismatch");

  msr->datasamples = NULL;
  msr3_free (&msr);
}

/* Test writing miniSEED records to a file from a MS3TraceList and verify output
 * against a reference file for v3 miniSEED.
 */
TEST (write, mstl3_writemseed)
{
  MS3Record *msr = NULL;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  int idx;
  int64_t rv;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
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
  msr->datasamples = isinedata;
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
  if (ofp)
  {
    if (fwrite (record, reclen, 1, (FILE *)ofp) != 1)
    {
      ms_log (2, "Error writing to output file\n");
    }
  }
} /* End of ms_record_handler_int() */

/* Test packing miniSEED records from a MS3TraceList and verify output against a
 * reference file for v2 miniSEED.
 *
 * After packing, the MS3TraceList should be empty.  Test for this by checking
 * the numtraceids and start of list pointer.
 */
TEST (pack, mstl3_pack_v2)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

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

  /* Check that contents of the MS3TraceList have been removed */
  CHECK (mstl->numtraceids == 0, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] == NULL, "MS3TraceList ID list is not empty");

  mstl3_free (&mstl, 0);
}

/* Test packing v3 miniSEED records from a MS3TraceList and verify output
 * against a reference file.
 *
 * After packing, the MS3TraceList should be empty.  Test for this by checking
 * the numtraceids and start of list pointer.
 */
TEST (pack, mstl3_pack_v3)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

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

  /* Check that contents of the MS3TraceList have been removed */
  CHECK (mstl->numtraceids == 0, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] == NULL, "MS3TraceList ID list is not empty");

  mstl3_free (&mstl, 0);
}

/* Test packing v2 miniSEED records from a MS3TraceList with the generator-sytle
 * interface.
 *
 * This test should reproduce the results of the mstl3_pack_v2 test with the
 * same parameters and data (slightly different input phasing), and verify
 * output against the same reference data.
 */
TEST (pack, mstl3_pack_next_v2)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int recordcount = 0;
  int64_t packedsamples = 0;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_V2 ".next", "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Initialize the packing context */
  flags = MSF_FLUSHDATA | MSF_PACKVER2;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  /* Pack the records */
  recordcount = 0;
  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
  {
    if (fwrite (record, reclen, 1, ofp) != 1)
    {
      ms_log (2, "Error writing to output file\n");
      break;
    }

    recordcount++;
  }

  if (result != 0)
  {
    ms_log (2, "mstl3_pack_next() returned an error: %d\n", result);
  }

  mstl3_pack_free (&packer, &packedsamples);
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_V2 ".next", "data/reference-" TESTFILE_MSTLPACK_V2),
         "Trace list packing v2 next mismatch");

  mstl3_free (&mstl, 1);
}

/* Test packing v3 miniSEED records from a MS3TraceList with the generator-sytle
 * interface.
 *
 * This test should reproduce the results of the mstl3_pack_v3 test with the
 * same parameters and data (slightly different input phasing), and verify output
 * against the same reference data.
 */
TEST (pack, mstl3_pack_next_v3)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int recordcount = 0;
  int64_t packedsamples = 0;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_V3 ".next", "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Initialize the packing context */
  flags = MSF_FLUSHDATA;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  /* Pack the records */
  recordcount = 0;
  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
  {
    if (fwrite (record, reclen, 1, ofp) != 1)
    {
      ms_log (2, "Error writing to output file\n");
      break;
    }

    recordcount++;
  }

  if (result != 0)
  {
    ms_log (2, "mstl3_pack_next() returned an error: %d\n", result);
  }

  mstl3_pack_free (&packer, &packedsamples);
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_V3 ".next", "data/reference-" TESTFILE_MSTLPACK_V3),
         "Trace list packing v3 next mismatch");

  mstl3_free (&mstl, 1);
}

/* Test packing miniSEED records from a MS3TraceList with the callback interface
 * and set the MSF_MAINTAINMSTL flag to maintain the trace list after packing.
 *
 * Verify that the trace list has not been modified after packing.
 */
TEST (pack, mstl3_pack_maintainmstl)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  MS3TraceSeg *hhz_seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (hhz_seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  MS3TraceSeg *bhz_seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (bhz_seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  MS3TraceID *hhz_id = mstl3_findID (mstl, "FDSN:XX_TEST__H_H_Z", 0, NULL);
  MS3TraceID *bhz_id = mstl3_findID (mstl, "FDSN:XX_TEST__B_H_Z", 0, NULL);
  REQUIRE (hhz_id != NULL, "H_H_Z trace ID not found");
  REQUIRE (bhz_id != NULL, "B_H_Z trace ID not found");

  /* Pack miniSEED records, while maintaining the trace list (with MSF_MAINTAINMSTL flag) */
  int64_t packedsamples = 0;
  flags |= MSF_FLUSHDATA;
  flags |= MSF_MAINTAINMSTL;
  rv = mstl3_pack (mstl, record_handler_int, NULL, 512, DE_STEIM1, &packedsamples, flags, 0, NULL);
  REQUIRE (rv == 8, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  /* Check that contents of the MS3TraceList have NOT been removed */
  CHECK (mstl->numtraceids == 2, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] != NULL, "MS3TraceList ID list is NULL");
  CHECK (mstl->traces.next[0] == bhz_id, "MS3TraceList ID list is not expected B_H_Z ID");
  CHECK (mstl->traces.next[0]->first == bhz_seg, "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->last == bhz_seg, "MS3TraceList ID list does not have expected last segment");
  CHECK (mstl->traces.next[0]->next[0] == hhz_id, "MS3TraceList ID list is not expected H_H_Z ID");
  CHECK (mstl->traces.next[0]->next[0]->first == hhz_seg, "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->next[0]->last == hhz_seg, "MS3TraceList ID list does not have expected last segment");

  CHECK (mstl->traces.next[0]->first->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->last->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->first->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->last->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");

  mstl3_free (&mstl, 0);
}

/* Test packing miniSEED records from a MS3TraceList with the generator
 * interface and set the MSF_MAINTAINMSTL flag to maintain the trace list after
 * packing.
 *
 * Verify that the trace list has not been modified after packing.
 */
TEST (pack, mstl3_pack_next_maintainmstl)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int recordcount = 0;
  int64_t packedsamples = 0;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  MS3TraceSeg *hhz_seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (hhz_seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  MS3TraceSeg *bhz_seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (bhz_seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  MS3TraceID *hhz_id = mstl3_findID (mstl, "FDSN:XX_TEST__H_H_Z", 0, NULL);
  MS3TraceID *bhz_id = mstl3_findID (mstl, "FDSN:XX_TEST__B_H_Z", 0, NULL);
  REQUIRE (hhz_id != NULL, "H_H_Z trace ID not found");
  REQUIRE (bhz_id != NULL, "B_H_Z trace ID not found");

  /* Pack miniSEED records, while maintaining the trace list (with MSF_MAINTAINMSTL flag) */
  flags |= MSF_FLUSHDATA;
  flags |= MSF_MAINTAINMSTL;

  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
  {
    recordcount++;
  }

  if (result != 0)
  {
    ms_log (2, "mstl3_pack_next() returned an error: %d\n", result);
  }

  mstl3_pack_free (&packer, &packedsamples);

  REQUIRE (recordcount == 8, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");

  /* Check that contents of the MS3TraceList have NOT been removed */
  CHECK (mstl->numtraceids == 2, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] != NULL, "MS3TraceList ID list is NULL");
  CHECK (mstl->traces.next[0] == bhz_id, "MS3TraceList ID list is not expected B_H_Z ID");
  CHECK (mstl->traces.next[0]->first == bhz_seg, "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->last == bhz_seg, "MS3TraceList ID list does not have expected last segment");
  CHECK (mstl->traces.next[0]->next[0] == hhz_id, "MS3TraceList ID list is not expected H_H_Z ID");
  CHECK (mstl->traces.next[0]->next[0]->first == hhz_seg, "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->next[0]->last == hhz_seg, "MS3TraceList ID list does not have expected last segment");

  CHECK (mstl->traces.next[0]->first->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->last->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->first->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->last->numsamples == SINE_DATA_SAMPLES, "MS3TraceList segment does not have expected number of samples");

  mstl3_free (&mstl, 0);
}

/* Test packing v2 miniSEED records with PPUPDATE and flushidle functionality.
 * Two traces H_H_Z and B_H_Z are added to a MS3TraceList using the
 * MSF_PPUPDATETIME flag to track update times.
 *
 * The update of the B_H_Z trace is updated to be 60 seconds in the past, which
 * should cause the trace to be flushed when packing the records with a flush
 * idle threshold of 30 seconds.
 *
 * The H_H_Z trace should have the current time as the update time, so it should
 * not be flushed.
 */
TEST (pack, mstl3_pack_ppupdate_flushidle_v2)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Set the PPUPDATE flag to track update times in mstl3_addmsr() */
  flags |= MSF_PPUPDATETIME;

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_FLUSHIDLE_V2, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Manipulate the update time of the B_H_Z trace to be 60 seconds in the past */
  if (seg->prvtptr)
  {
    nstime_t *update_time = (nstime_t *)seg->prvtptr;
    *update_time = lmp_systemtime () - (nstime_t)60 * NSTMODULUS;
  }

  /* Set the flush idle threshold to 30 seconds */
  uint32_t flush_idle_seconds = 30;

  /* Pack v2 miniSEED records flushing only idle segments */
  int64_t packedsamples = 0;
  flags |= MSF_PACKVER2;
  rv = mstl3_pack_ppupdate_flushidle (mstl, record_handler_int, ofp, 4096, DE_STEIM1, &packedsamples,
                                      flags, 0, NULL, flush_idle_seconds);
  REQUIRE (rv == 1, "mstl3_pack_ppupdate_flushidle() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_FLUSHIDLE_V2, "data/reference-" TESTFILE_FLUSHIDLE_V2),
         "Trace list packing v2 flushidle mismatch");

  mstl3_free (&mstl, 1);
}

/* Test packing v3 miniSEED records with PPUPDATE and flushidle functionality.
 * Two traces B_H_Z and H_H_Z are added to a MS3TraceList using the
 * MSF_PPUPDATETIME flag to track update times.
 *
 * The update of the H_H_Z trace is updated to be 60 seconds in the past, which
 * should cause the trace to be flushed when packing the records with a flush
 * idle threshold of 30 seconds.
 *
 * The B_H_Z trace should have the current time as the update time, so it should
 * not be flushed.
 */
TEST (pack, mstl3_pack_ppupdate_flushidle_v3)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Set the PPUPDATE flag to track update times in mstl3_addmsr() */
  flags |= MSF_PPUPDATETIME;

  /* Add a B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add a H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_FLUSHIDLE_V3, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Manipulate the update time of the B_H_Z trace to be 60 seconds in the past */
  if (seg->prvtptr)
  {
    nstime_t *update_time = (nstime_t *)seg->prvtptr;
    *update_time = lmp_systemtime () - (nstime_t)60 * NSTMODULUS;
  }

  /* Set the flush idle threshold to 30 seconds */
  uint32_t flush_idle_seconds = 30;

  /* Pack v3 miniSEED records flushing only idle segments */
  int64_t packedsamples = 0;
  rv = mstl3_pack_ppupdate_flushidle (mstl, record_handler_int, ofp, 4096, DE_STEIM1, &packedsamples,
                                      flags, 0, NULL, flush_idle_seconds);
  REQUIRE (rv == 1, "mstl3_pack_ppupdate_flushidle() return unexpected value");
  CHECK (packedsamples == SINE_DATA_SAMPLES, "Packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_FLUSHIDLE_V3, "data/reference-" TESTFILE_FLUSHIDLE_V3),
         "Trace list packing v3 flushidle mismatch");

  mstl3_free (&mstl, 1);
}

/* Test packing records with the callback interfacefrom a MS3TraceList used as a
 * rolling buffer with, where packed data is removed from the trace list after
 * each pack, data is then added and packed in later calls.
 */
TEST (pack, mstl3_pack_rollingbuffer)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  int64_t packedsamples = 0;
  int64_t totalpackedsamples = 0;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  int64_t rv;
  nstime_t starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Common record parameters */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add first half of H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = starttime;
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add first half of B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = starttime;
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_ROLLINGBUFFER, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Pack miniSEED records, WITHOUT flushing data buffers */
  rv = mstl3_pack (mstl, record_handler_int, ofp, 512, DE_INT32, &packedsamples, flags, 0, NULL);
  REQUIRE (rv == 4, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == 452, "No samples packed");

  totalpackedsamples += packedsamples;

  /* Add second half of H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_sampletime (starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add second half of B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_sampletime (starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Pack miniSEED records, flushing data buffers */
  flags |= MSF_FLUSHDATA;
  rv = mstl3_pack (mstl, record_handler_int, ofp, 512, DE_INT32, &packedsamples, flags, 0, NULL);
  REQUIRE (rv == 6, "mstl3_pack() return unexpected value");
  CHECK (packedsamples == 548, "No samples packed");

  totalpackedsamples += packedsamples;

  CHECK (totalpackedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Total packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_ROLLINGBUFFER, "data/reference-" TESTFILE_MSTLPACK_ROLLINGBUFFER),
         "Trace list packing callback rollingbuffer reference file mismatch");

  /* Check that contents of the MS3TraceList have been removed */
  CHECK (mstl->numtraceids == 0, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] == NULL, "MS3TraceList ID list is not empty");

  mstl3_free (&mstl, 0);
}

/* Test packing records with the generator-style interface from a MS3TraceList
 * used as a rolling buffer with, where packed data is removed from the trace
 * list after each pack, data is then added and packed in later calls.
 */
TEST (pack, mstl3_pack_next_rollingbuffer)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  FILE *ofp = NULL;
  int64_t packedsamples = 0;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  nstime_t starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int recordcount = 0;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  /* Initialize the packing context */
  packer = mstl3_pack_init (mstl, 512, DE_INT32, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  /* Common record parameters */
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';

  /* Add first half of H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = starttime;
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add first half of B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = starttime;
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Pack miniSEED records, WITHOUT flushing data buffers */
  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
  {
    record_handler_int (record, reclen, ofp);
    recordcount++;
  }

  REQUIRE (result == 0, "mstl3_pack_next() return unexpected value");
  CHECK (recordcount == 4, "mstl3_pack_next()Expected 4 records");

  /* Add second half of H_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_sampletime (starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Add second half of B_H_Z trace */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_sampletime (starttime, SINE_DATA_SAMPLES / 2, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES / 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Pack miniSEED records, flushing data buffers */
  flags |= MSF_FLUSHDATA;
  recordcount = 0;
  while ((result = mstl3_pack_next (packer, flags, &record, &reclen)) == 1)
  {
    record_handler_int (record, reclen, ofp);
    recordcount++;
  }

  REQUIRE (result == 0, "mstl3_pack_next() return unexpected value");
  CHECK (recordcount == 6, "mstl3_pack_next() Expected 6 records");

  mstl3_pack_free (&packer, &packedsamples);

  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Total packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER, "data/reference-" TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER),
         "Trace list packing generator rollingbuffer reference file mismatch");

  /* Check that contents of the MS3TraceList have been removed */
  CHECK (mstl->numtraceids == 0, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] == NULL, "MS3TraceList ID list is not empty");

  mstl3_free (&mstl, 0);
}
