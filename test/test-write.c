#include <libmseed.h>
#include <tau/tau.h>

#include "testdata.h"

static char *soh_json_headers = "{"
                                "\"FDSN\":{"
                                "\"Time\":{"
                                "\"Exception\":["
                                "{"
                                "\"Time\":\"2012-01-13T00:00:00Z\","
                                "\"VCOCorrection\":51.51367,"
                                "\"ReceptionQuality\":0,"
                                "\"Count\":6829,"
                                "\"Type\":\"Daily Timemark\","
                                "\"ClockStatus\":\"SNR=48,49,48,47,51,48,51,50,47,48,46\""
                                "},"
                                "{"
                                "\"Time\":\"2012-01-13T00:03:16.000001Z\","
                                "\"VCOCorrection\":51.51367,"
                                "\"ReceptionQuality\":0,"
                                "\"Count\":196,"
                                "\"Type\":\"UnExp Timemark\","
                                "\"ClockStatus\":\"Jump of -0.999999 Seconds\""
                                "},"
                                "{"
                                "\"Time\":\"2012-01-13T00:03:36.000004Z\","
                                "\"VCOCorrection\":51.51367,"
                                "\"ReceptionQuality\":90,"
                                "\"Count\":21,"
                                "\"Type\":\"Valid Timemark\","
                                "\"ClockStatus\":\"SNR=47,50,47,49,50,48,52,49,48,46,46\""
                                "}"
                                "]"
                                "},"
                                "\"Clock\":{"
                                "\"Model\":\"P273T11N16\""
                                "}"
                                "},"
                                "\"Manufacturer123\":{"
                                "\"Metadata\":{"
                                "\"FilamentCurrent\":16.4,"
                                "\"HyperCoordinates\":\"1.1789:965402:73324@3.14159\""
                                "}"
                                "},"
                                "\"OperatorXYZ\":{"
                                "\"DSP\":{"
                                "\"PeakRMS\":2067,"
                                "\"RMSWindow\":10.5"
                                "}"
                                "}"
                                "}";

extern int cmpfiles (char *fileA, char *fileB);

/* Write test output files.  Reference files are at "data/reference-<name>" */
#define TESTFILE_TEXT_V2 "testdata-text.mseed2"
#define TESTFILE_FLOAT32_V2 "testdata-float32.mseed2"
#define TESTFILE_FLOAT64_V2 "testdata-float64.mseed2"
#define TESTFILE_INT16_V2 "testdata-int16.mseed2"
#define TESTFILE_INT32_V2 "testdata-int32.mseed2"
#define TESTFILE_STEIM1_V2 "testdata-steim1.mseed2"
#define TESTFILE_STEIM2_V2 "testdata-steim2.mseed2"
#define TESTFILE_DEFAULTS_V2 "testdata-defaults.mseed2"
#define TESTFILE_HEADERONLY_V2 "testdata-headeronly.mseed2"
#define TESTFILE_NSEC_V2 "testdata-nsec.mseed2"
#define TESTFILE_OLDEN_V2 "testdata-olden.mseed2"
#define TESTFILE_ODDRATE_V2 "testdata-oddrate.mseed2"
#define TESTFILE_MSTLPACK_V2 "testdata-mstlpack.mseed2"
#define TESTFILE_FLUSHIDLE_V2 "testdata-flushidle.mseed2"

#define TESTFILE_TEXT_V3 "testdata-text.mseed3"
#define TESTFILE_FLOAT32_V3 "testdata-float32.mseed3"
#define TESTFILE_FLOAT64_V3 "testdata-float64.mseed3"
#define TESTFILE_INT16_V3 "testdata-int16.mseed3"
#define TESTFILE_INT32_V3 "testdata-int32.mseed3"
#define TESTFILE_STEIM1_V3 "testdata-steim1.mseed3"
#define TESTFILE_STEIM2_V3 "testdata-steim2.mseed3"
#define TESTFILE_DEFAULTS_V3 "testdata-defaults.mseed3"
#define TESTFILE_HEADERONLY_V3 "testdata-headeronly.mseed3"
#define TESTFILE_NSEC_V3 "testdata-nsec.mseed3"
#define TESTFILE_OLDEN_V3 "testdata-olden.mseed3"
#define TESTFILE_ODDRATE_V3 "testdata-oddrate.mseed3"
#define TESTFILE_MSTLPACK_V3 "testdata-mstlpack.mseed3"
#define TESTFILE_FLUSHIDLE_V3 "testdata-flushidle.mseed3"

#define TESTFILE_MSTLPACK_ROLLINGBUFFER "testdata-mstlpack-rollingbuffer.mseed"
#define TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER "testdata-mstlpack-rollingbuffer-next.mseed"

/* Write test output files.  No reference files needed for these tests. */
#define TESTFILE_TIMECARRY_V2 "testdata-timecarry.mseed2"
#define TESTFILE_BTIMECARRY_V2 "testdata-btimecarry.mseed2"
#define TESTFILE_B500FIELDS_V2 "testdata-b500fields.mseed2"
#define TESTFILE_SAMPLECOUNT_V2 "testdata-samplecount.mseed2"
#define TESTFILE_MSTLPACK_EXTRA_V2 "testdata-mstlpack-extra.mseed2"

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
  int64_t rv;

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
  msr->samprate = 0;
  msr->encoding = DE_TEXT;
  msr->numsamples = strlen (textdata);
  msr->datasamples = textdata;
  msr->sampletype = 't';

  rv = msr3_writemseed (msr, TESTFILE_TEXT_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_TEXT_V3, "data/reference-" TESTFILE_TEXT_V3),
         "Text encoding write mismatch");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;

  /* Float32 encoding*/
  msr->encoding = DE_FLOAT32;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = fsinedata;
  msr->sampletype = 'f';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT32_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT32_V3, "data/reference-" TESTFILE_FLOAT32_V3),
         "Float32 encoding write mismatch");

  /* Float64 encoding */
  msr->encoding = DE_FLOAT64;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = dsinedata;
  msr->sampletype = 'd';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT64_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT64_V3, "data/reference-" TESTFILE_FLOAT64_V3),
         "Float64 encoding write mismatch");

  /* Int16 encoding */
  msr->encoding = DE_INT16;
  msr->numsamples = 220; /* Limit to first 220 samples, which can be represented in 16-bits */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V3, "data/reference-" TESTFILE_INT16_V3),
         "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V3, "data/reference-" TESTFILE_INT32_V3),
         "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V3, "data/reference-" TESTFILE_STEIM1_V3),
         "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples = SINE_DATA_SAMPLES -
                    1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V3, "data/reference-" TESTFILE_STEIM2_V3),
         "Steim2 encoding write mismatch");

  /* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples = SINE_DATA_SAMPLES -
                    1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_DEFAULTS_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_DEFAULTS_V3, "data/reference-" TESTFILE_DEFAULTS_V3),
         "Default encoding/reclen write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;

  /* Set miniSEED v2 flag */
  flags |= MSF_PACKVER2;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");
  msr->reclen = 512;

  /* Text encoding */
  strcpy (msr->sid, "FDSN:XX_TEST__L_O_G");
  msr->samprate = 0;
  msr->encoding = DE_TEXT;
  msr->numsamples = strlen (textdata);
  msr->datasamples = textdata;
  msr->sampletype = 't';

  rv = msr3_writemseed (msr, TESTFILE_TEXT_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_TEXT_V2, "data/reference-" TESTFILE_TEXT_V2),
         "Text encoding write mismatch");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->samprate = 40.0;

  /* Float32 encoding*/
  msr->encoding = DE_FLOAT32;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = fsinedata;
  msr->sampletype = 'f';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT32_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT32_V2, "data/reference-" TESTFILE_FLOAT32_V2),
         "Float32 encoding write mismatch");

  /* Float64 encoding */
  msr->encoding = DE_FLOAT64;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = dsinedata;
  msr->sampletype = 'd';

  rv = msr3_writemseed (msr, TESTFILE_FLOAT64_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_FLOAT64_V2, "data/reference-" TESTFILE_FLOAT64_V2),
         "Float64 encoding write mismatch");

  /* Int16 encoding */
  msr->encoding = DE_INT16;
  msr->numsamples = 220; /* Limit to first 220 samples, which can be represented in 16-bits */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT16_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT16_V2, "data/reference-" TESTFILE_INT16_V2),
         "Int16 encoding write mismatch");

  /* Int32 encoding */
  msr->encoding = DE_INT32;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_INT32_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_INT32_V2, "data/reference-" TESTFILE_INT32_V2),
         "Int32 encoding write mismatch");

  /* Steim1 encoding */
  msr->encoding = DE_STEIM1;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM1_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM1_V2, "data/reference-" TESTFILE_STEIM1_V2),
         "Steim1 encoding write mismatch");

  /* Steim2 encoding */
  msr->encoding = DE_STEIM2;
  msr->numsamples = SINE_DATA_SAMPLES -
                    1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_STEIM2_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V2, "data/reference-" TESTFILE_STEIM2_V2),
         "Steim2 encoding write mismatch");

  /* Default encoding (Steim2) and record length (4096) */
  msr->encoding = -1;
  msr->reclen = -1;
  msr->numsamples = SINE_DATA_SAMPLES -
                    1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  rv = msr3_writemseed (msr, TESTFILE_DEFAULTS_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_DEFAULTS_V2, "data/reference-" TESTFILE_DEFAULTS_V2),
         "Default encoding/reclen write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);
}

/* Test packing v2 miniSEED records with only the header and no data. */
TEST (write, msr3_writemseed_headeronly_v2)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_PACKVER2; /* write v2 format */
  int64_t rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  /* Set up record parameters */
  msr->reclen = 4096;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");

  strcpy (msr->sid, "FDSN:XX_TEST__S_O_H");
  msr->samprate = 0;
  msr->pubversion = 1;

  msr->extra = soh_json_headers;
  msr->extralength = (uint16_t)strlen (msr->extra);

  msr->samplecnt = 0;
  msr->numsamples = 0;
  msr->datasamples = NULL;

  rv = msr3_writemseed (msr, TESTFILE_HEADERONLY_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_HEADERONLY_V2, "data/reference-" TESTFILE_HEADERONLY_V2),
         "Header only write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
  msr3_free (&msr);
}

/* Test packing v3 miniSEED records with only the header and no data. */
TEST (write, msr3_writemseed_headeronly_v3)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int64_t rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  /* Set up record parameters */
  msr->reclen = 4096;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00");

  strcpy (msr->sid, "FDSN:XX_TEST__S_O_H");
  msr->samprate = 0;
  msr->pubversion = 1;

  msr->extra = soh_json_headers;
  msr->extralength = (uint16_t)strlen (msr->extra);

  msr->samplecnt = 0;
  msr->numsamples = 0;
  msr->datasamples = NULL;

  rv = msr3_writemseed (msr, TESTFILE_HEADERONLY_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_HEADERONLY_V3, "data/reference-" TESTFILE_HEADERONLY_V3),
         "Header only write mismatch");

  msr->extra = NULL;
  msr->extralength = 0;
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
  int64_t rv;

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

  /* V3 Nanosecond time resolution with Int32 data and a timing exception and 512 max record length
   */
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr->formatversion = 3;
  msr->encoding = DE_INT32;
  msr->reclen = 512;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';
  msr->extra = "{\"FDSN\":{"
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
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_NSEC_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_NSEC_V3, "data/reference-" TESTFILE_NSEC_V3),
         "Nanosecond timing write mismatch");

  /* V2 Nanosecond time resolution with Int32 data and a timing exception and 512 record length */
  msr->starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr->formatversion = 2;
  msr->encoding = DE_INT32;
  msr->reclen = 512;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';
  msr->extra = "{\"FDSN\":{"
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
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_NSEC_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_NSEC_V2, "data/reference-" TESTFILE_NSEC_V2),
         "Nanosecond timing write mismatch");

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
  int64_t rv;

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
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';
  msr->extra = "{\"FDSN\":{"
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
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_OLDEN_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_OLDEN_V3, "data/reference-" TESTFILE_OLDEN_V3),
         "Old, pre-epoch times write mismatch");

  /* V2 Old, pre-epoch times with Int32 data and a timing exception and 4096 record length */
  msr->starttime = ms_timestr2nstime ("1964-03-27T21:11:24.987654321Z");
  msr->formatversion = 2;
  msr->encoding = DE_INT32;
  msr->reclen = 4096;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';
  msr->extra = "{\"FDSN\":{"
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
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_OLDEN_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_OLDEN_V2, "data/reference-" TESTFILE_OLDEN_V2),
         "Old, pre-epoch times write mismatch");

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
  int64_t rv;

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
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  /* Odd rate (1080.0) with an repeating decimal period */
  msr->samprate = 1080.0;

  /* V3 */
  msr->formatversion = 3;

  rv = msr3_writemseed (msr, TESTFILE_ODDRATE_V3, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_ODDRATE_V3, "data/reference-" TESTFILE_ODDRATE_V3),
         "Odd rate write mismatch");

  /* V2 */
  msr->formatversion = 2;

  rv = msr3_writemseed (msr, TESTFILE_ODDRATE_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_ODDRATE_V2, "data/reference-" TESTFILE_ODDRATE_V2),
         "Odd rate write mismatch");

  msr->datasamples = NULL;
  msr3_free (&msr);
}

/* Test that miniSEED v2 continuation records correctly carry a 100 microsecond
 * rounding carry into the next second.
 *
 * A record's start time is encoded as tenths-of-milliseconds (fsec) plus a
 * microsecond offset in the range -50 to +49.  When the fractional second is
 * within 50 microseconds of the next second boundary (i.e. >= 0.99995s), the
 * rounded fsec/offset pair carries into the next second and the record's
 * Y/D/H/M/S fields must be derived from that carried time, not the raw,
 * uncarried start time.
 *
 * A small record length is used to force many continuation records at a 1 Hz
 * sample rate, so every continuation record's start fraction repeats the
 * initial, in-carry-band fraction.  If a continuation record's Y/D/H/M/S were
 * derived from the raw (uncarried) time it would be written exactly one second
 * early.
 */
TEST (write, msr3_writemseed_v2_continuation_timecarry)
{
  MS3Record *msr = NULL;
  MS3Record *rmsr = NULL;
  int32_t isinedata[SINE_DATA_SAMPLES];
  nstime_t starttime;
  nstime_t expected;
  int64_t cumulative = 0;
  int reccount = 0;
  uint32_t flags = MSF_FLUSHDATA | MSF_PACKVER2;
  int64_t rv;
  int rrv;
  int idx;

  /* Create integer sine data set */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->reclen = 128; /* Small reclen forces many continuation records */
  msr->pubversion = 1;
  msr->samprate = 1.0;
  msr->encoding = DE_INT32;
  msr->numsamples = SINE_DATA_SAMPLES;
  msr->samplecnt = SINE_DATA_SAMPLES;
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  /* Start time fraction (.999980s) is within the 50 microsecond carry band */
  starttime = ms_timestr2nstime ("2012-01-01T00:00:00.999980Z");
  msr->starttime = starttime;

  rv = msr3_writemseed (msr, TESTFILE_TIMECARRY_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");

  msr->datasamples = NULL;
  msr3_free (&msr);

  /* Read back every record and verify its start time against the time
   * calculated independently via ms_sampletime(), which would catch a
   * continuation record written exactly one second early. */
  while ((rrv = ms3_readmsr (&rmsr, TESTFILE_TIMECARRY_V2, 0, 0)) == MS_NOERROR)
  {
    expected = ms_sampletime (starttime, cumulative, 1.0);

    CHECK (rmsr->starttime == expected,
           "Record start time does not match expected time (continuation time carry)");

    cumulative += rmsr->samplecnt;
    reccount++;
  }

  CHECK (rrv == MS_ENDOFFILE, "ms3_readmsr() did not end with expected MS_ENDOFFILE");
  REQUIRE (reccount > 1, "Test did not generate multiple records, continuation path not exercised");

  ms3_readmsr (&rmsr, NULL, 0, 0);
}

/* Test that a v2 Blockette 500 (Timing Exception) time supplied via extra
 * headers round-trips correctly when its fractional second is within 50
 * microseconds of the next second boundary.
 *
 * The encoded BTIME's Y/D/H/M/S fields must be derived from the time after the
 * fsec/microsecond-offset rounding carry is applied; otherwise the blockette
 * time would be written exactly one second early whenever the fractional second
 * is >= 0.99995s.
 */
TEST (write, msr3_writemseed_v2_btime_timecarry)
{
  MS3Record *msr = NULL;
  MS3Record *rmsr = NULL;
  int32_t sampledata[4] = {1, 2, 3, 4};
  char gottime[64];
  nstime_t expected;
  nstime_t got;
  uint32_t flags = MSF_FLUSHDATA | MSF_PACKVER2;
  int64_t rv;
  int rrv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-06-01T00:00:00Z");
  msr->samprate = 1.0;
  msr->encoding = DE_INT32;
  msr->numsamples = 4;
  msr->samplecnt = 4;
  msr->datasamples = sampledata;
  msr->sampletype = 'i';

  /* Exception time fraction (.999980s) is within the 50 microsecond carry band */
  msr->extra = "{\"FDSN\":{\"Time\":{\"Exception\":["
               "{\"Time\":\"2012-06-01T12:00:00.999980Z\"}"
               "]}}}";
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_BTIMECARRY_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);

  /* Read back and confirm the decoded Blockette 500 time matches the
   * original exception time, which would catch an encoded BTIME that is
   * exactly one second early. */
  rrv = ms3_readmsr (&rmsr, TESTFILE_BTIMECARRY_V2, 0, 0);
  CHECK (rrv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (rmsr != NULL, "ms3_readmsr() did not populate 'rmsr'");

  rrv = mseh_get_string (rmsr, "/FDSN/Time/Exception/0/Time", gottime, sizeof (gottime));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded B500 time");

  expected = ms_timestr2nstime ("2012-06-01T12:00:00.999980Z");
  got = ms_timestr2nstime (gottime);

  CHECK (got == expected,
         "Decoded Blockette 500 time does not match encoded time (BTIME time carry)");

  ms3_readmsr (&rmsr, NULL, 0, 0);
}

/* Test that v2 Blockette 500 (Timing Exception) 'Type' and 'ClockStatus'
 * text fields round-trip correctly when they completely fill their 16- and
 * 128-byte SEED fields, alongside a second exception with short values and
 * a full-width 'Clock/Model' field.
 *
 * The Type and ClockStatus fields are exactly the size of their SEED
 * counterparts, so a fully populated field is not null terminated on the
 * wire; decoding it must neither overflow the destination nor read past
 * the field into adjacent data.
 */
/* Exactly 16 characters, matching the Blockette 500 Type field width */
#define B500TEST_TYPE_FULL "0123456789ABCDEF"
/* Exactly 128 characters, matching the Blockette 500 ClockStatus field width */
#define B500TEST_CLOCKSTATUS_FULL \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"              \
  "0123456789ABCDEF"
/* Exactly 32 characters, matching the Clock Model field width */
#define B500TEST_MODEL_FULL "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"

TEST (write, msr3_writemseed_v2_b500_full_fields)
{
  MS3Record *msr = NULL;
  MS3Record *rmsr = NULL;
  int32_t sampledata[4] = {1, 2, 3, 4};
  char gotstr[200];
  uint32_t flags = MSF_FLUSHDATA | MSF_PACKVER2;
  int64_t rv;
  int rrv;

  REQUIRE (strlen (B500TEST_TYPE_FULL) == 16,
           "Test fixture 'B500TEST_TYPE_FULL' is not 16 characters");
  REQUIRE (strlen (B500TEST_CLOCKSTATUS_FULL) == 128,
           "Test fixture 'B500TEST_CLOCKSTATUS_FULL' is not 128 characters");
  REQUIRE (strlen (B500TEST_MODEL_FULL) == 32,
           "Test fixture 'B500TEST_MODEL_FULL' is not 32 characters");

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__B_H_Z");
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-06-01T00:00:00Z");
  msr->samprate = 1.0;
  msr->encoding = DE_INT32;
  msr->numsamples = 4;
  msr->samplecnt = 4;
  msr->datasamples = sampledata;
  msr->sampletype = 'i';

  msr->extra =
      "{\"FDSN\":{\"Time\":{\"Exception\":["
      "{\"Time\":\"2012-06-01T00:00:01Z\",\"Type\":\"" B500TEST_TYPE_FULL
      "\",\"ClockStatus\":\"" B500TEST_CLOCKSTATUS_FULL "\"},"
      "{\"Time\":\"2012-06-01T00:00:02Z\",\"Type\":\"Short\",\"ClockStatus\":\"Brief status\"}"
      "]},\"Clock\":{\"Model\":\"" B500TEST_MODEL_FULL "\"}}}";
  msr->extralength = (uint16_t)strlen (msr->extra);

  rv = msr3_writemseed (msr, TESTFILE_B500FIELDS_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");

  msr->extra = NULL;
  msr->extralength = 0;
  msr->datasamples = NULL;
  msr3_free (&msr);

  rrv = ms3_readmsr (&rmsr, TESTFILE_B500FIELDS_V2, 0, 0);
  CHECK (rrv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (rmsr != NULL, "ms3_readmsr() did not populate 'rmsr'");

  /* Full-width Type must decode intact, not truncated and not run into ClockStatus */
  rrv = mseh_get_string (rmsr, "/FDSN/Time/Exception/0/Type", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded full-width Type");
  CHECK (strcmp (gotstr, B500TEST_TYPE_FULL) == 0,
         "Decoded full-width Type does not match encoded value");

  /* Full-width ClockStatus must decode intact and not carry a leaked Type prefix */
  rrv = mseh_get_string (rmsr, "/FDSN/Time/Exception/0/ClockStatus", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded full-width ClockStatus");
  CHECK (strcmp (gotstr, B500TEST_CLOCKSTATUS_FULL) == 0,
         "Decoded full-width ClockStatus does not match encoded value");

  /* Short values in the second exception must decode without trailing pad */
  rrv = mseh_get_string (rmsr, "/FDSN/Time/Exception/1/Type", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded short Type");
  CHECK (strcmp (gotstr, "Short") == 0, "Decoded short Type does not match encoded value");

  rrv = mseh_get_string (rmsr, "/FDSN/Time/Exception/1/ClockStatus", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded short ClockStatus");
  CHECK (strcmp (gotstr, "Brief status") == 0,
         "Decoded short ClockStatus does not match encoded value");

  rrv = mseh_get_string (rmsr, "/FDSN/Clock/Model", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find decoded Clock Model");
  CHECK (strcmp (gotstr, B500TEST_MODEL_FULL) == 0,
         "Decoded Clock Model does not match encoded value");

  ms3_readmsr (&rmsr, NULL, 0, 0);
}

/* Test that a miniSEED v2 record with more samples than fit in the 16-bit FSDH
 * sample-count field is split into multiple records rather than truncating the
 * count.
 *
 * The v2 sample-count field is 16 bits, so no single record can represent more
 * than UINT16_MAX samples regardless of record length.  A large text payload
 * (one sample per byte) in a 128 KiB record makes this deterministic: 70000
 * samples would fit in a single record by length but exceed the 16-bit count
 * field, so the packer must emit multiple records with a correct total count.
 */
TEST (write, msr3_writemseed_v2_samplecount_overflow)
{
  MS3Record *msr = NULL;
  MS3Record *rmsr = NULL;
  const int64_t numsamples = 70000; /* > UINT16_MAX, fits in one 128 KiB record */
  char *textdata_big = NULL;
  int64_t total = 0;
  int reccount = 0;
  uint32_t flags = MSF_FLUSHDATA | MSF_PACKVER2;
  int64_t rv;
  int rrv;

  textdata_big = (char *)malloc (numsamples);
  REQUIRE (textdata_big != NULL, "Failed to allocate test text buffer");
  memset (textdata_big, 'A', numsamples);

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__L_O_G");
  msr->reclen = 131072; /* < MAXRECLENv2, large enough to hold all samples at once */
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2012-01-01T00:00:00Z");
  msr->samprate = 0;
  msr->encoding = DE_TEXT;
  msr->numsamples = numsamples;
  msr->samplecnt = numsamples;
  msr->datasamples = textdata_big;
  msr->sampletype = 't';

  rv = msr3_writemseed (msr, TESTFILE_SAMPLECOUNT_V2, 1, flags, 0);
  REQUIRE (rv > 0, "msr3_writemseed() return unexpected value");

  msr->datasamples = NULL;
  msr3_free (&msr);
  free (textdata_big);

  /* Read back every record; the total sample count must be preserved and no
   * single record may exceed the 16-bit field. */
  while ((rrv = ms3_readmsr (&rmsr, TESTFILE_SAMPLECOUNT_V2, 0, 0)) == MS_NOERROR)
  {
    CHECK (rmsr->samplecnt <= UINT16_MAX, "Record sample count exceeds the 16-bit v2 FSDH field");

    total += rmsr->samplecnt;
    reccount++;
  }

  CHECK (rrv == MS_ENDOFFILE, "ms3_readmsr() did not end with expected MS_ENDOFFILE");
  CHECK (total == numsamples, "Total sample count across records does not match input");
  REQUIRE (reccount > 1, "Test did not split into multiple records, overflow path not exercised");

  ms3_readmsr (&rmsr, NULL, 0, 0);
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
  msr->samprate = 40.0;
  msr->numsamples = SINE_DATA_SAMPLES -
                    1; /* All but last sample for which the difference cannot be represented */
  msr->datasamples = isinedata;
  msr->sampletype = 'i';

  seg = mstl3_addmsr (mstl, msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  rv = mstl3_writemseed (mstl, TESTFILE_STEIM2_V3 ".trace", 1, 512, DE_STEIM2, flags, 0);
  REQUIRE (rv == 4, "mstl3_writemseed() return unexpected value");
  CHECK (!cmpfiles (TESTFILE_STEIM2_V3 ".trace", "data/reference-" TESTFILE_STEIM2_V3),
         "Steim2 encoding trace write mismatch");

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

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_V2, "data/reference-" TESTFILE_MSTLPACK_V2),
         "Trace list packing v2 mismatch");

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

/* Test that mstl3_pack_free() reports samples emitted by a segment packing
 * session that is still active (i.e. the caller stops after some records
 * without draining the segment to completion).
 */
TEST (pack, mstl3_pack_free_active_session)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3Record *rmsr = NULL;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int64_t packedsamples = 0;
  int64_t expectedsamples = 0;

  /* Create integer sine data set */
  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Small record length forces multiple records for this segment, so the
   * segment packing session is still active after the first record */
  packer = mstl3_pack_init (mstl, 128, DE_STEIM1, 0, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  result = mstl3_pack_next (packer, 0, &record, &reclen);
  REQUIRE (result == 1, "mstl3_pack_next() did not return a record");

  /* Determine the sample count of the emitted record independently, via a
   * header-only parse, to compare against the count reported on abort */
  REQUIRE (msr3_parse (record, reclen, &rmsr, 0, 0) == MS_NOERROR,
           "msr3_parse() failed on packed record");
  expectedsamples = rmsr->samplecnt;
  msr3_free (&rmsr);

  /* Abort with the segment packing session still active */
  mstl3_pack_free (&packer, &packedsamples);

  CHECK (packedsamples == expectedsamples,
         "Packed samples undercounted on abort with an active segment session");

  mstl3_free (&mstl, 1);
}

/* Test packing multiple segments with the generator-style interface while a
 * caller-owned (not packer-owned) extra headers buffer is supplied to
 * mstl3_pack_init().
 *
 * The extra headers buffer is documented as borrowed: it is added to every
 * generated record but ownership stays with the caller.  Internally, each
 * segment packed re-initializes an MS3Record template that temporarily
 * references this borrowed buffer; that reference must never be freed by the
 * packer.  A string literal is used here so any invalid free of it aborts
 * immediately rather than silently corrupting the heap.  Two trace segments
 * are packed so the template is reinitialized a second time, which is where
 * a borrowed pointer could incorrectly be freed.
 */
TEST (pack, mstl3_pack_next_extra_headers_borrowed)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  MS3Record *rmsr = NULL;
  FILE *ofp = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
  int recordcount = 0;
  int64_t packedsamples = 0;
  int rrv;
  int checked;
  uint64_t quality;

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

  /* Add a B_H_Z trace, a second segment whose packing reinitializes the
   * MS3Record template a second time */
  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.samprate = 40.0;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.123456789Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Open file for generated miniSEED records */
  ofp = fopen (TESTFILE_MSTLPACK_EXTRA_V2, "wb");
  REQUIRE (ofp != NULL, "Failed to open output file");

  /* Initialize the packing context with a borrowed (caller-owned) extra
   * headers buffer -- a string literal, not allocated by this library */
  flags = MSF_FLUSHDATA | MSF_PACKVER2;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0,
                            "{\"FDSN\":{\"Time\":{\"Quality\":100}}}", 0);
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

  CHECK (result == 0, "mstl3_pack_next() did not finish cleanly packing multiple segments");

  mstl3_pack_free (&packer, &packedsamples);
  CHECK (packedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES, "Packed samples mismatch");
  REQUIRE (recordcount > 1, "Test did not generate records for multiple segments");

  fclose (ofp);

  mstl3_free (&mstl, 1);

  /* Verify the extra headers survived intact in every record from both
   * segments; corruption from a premature free of the borrowed buffer would
   * surface here even where it did not abort outright. */
  checked = 0;
  while ((rrv = ms3_readmsr (&rmsr, TESTFILE_MSTLPACK_EXTRA_V2, 0, 0)) == MS_NOERROR)
  {
    CHECK (mseh_get_uint64 (rmsr, "/FDSN/Time/Quality", &quality) == 0,
           "Extra headers missing or corrupted in packed record");
    CHECK (quality == 100, "Extra header value corrupted in packed record");
    checked++;
  }

  CHECK (rrv == MS_ENDOFFILE, "ms3_readmsr() did not end with expected MS_ENDOFFILE");
  CHECK (checked == recordcount, "Did not verify extra headers for all packed records");

  ms3_readmsr (&rmsr, NULL, 0, 0);
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
  CHECK (mstl->traces.next[0]->first == bhz_seg,
         "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->last == bhz_seg,
         "MS3TraceList ID list does not have expected last segment");
  CHECK (mstl->traces.next[0]->next[0] == hhz_id, "MS3TraceList ID list is not expected H_H_Z ID");
  CHECK (mstl->traces.next[0]->next[0]->first == hhz_seg,
         "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->next[0]->last == hhz_seg,
         "MS3TraceList ID list does not have expected last segment");

  CHECK (mstl->traces.next[0]->first->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->last->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->first->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->last->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");

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
  CHECK (mstl->traces.next[0]->first == bhz_seg,
         "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->last == bhz_seg,
         "MS3TraceList ID list does not have expected last segment");
  CHECK (mstl->traces.next[0]->next[0] == hhz_id, "MS3TraceList ID list is not expected H_H_Z ID");
  CHECK (mstl->traces.next[0]->next[0]->first == hhz_seg,
         "MS3TraceList ID list does not have expected first segment");
  CHECK (mstl->traces.next[0]->next[0]->last == hhz_seg,
         "MS3TraceList ID list does not have expected last segment");

  CHECK (mstl->traces.next[0]->first->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->last->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->first->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");
  CHECK (mstl->traces.next[0]->next[0]->last->numsamples == SINE_DATA_SAMPLES,
         "MS3TraceList segment does not have expected number of samples");

  mstl3_free (&mstl, 0);
}

/* Test packing miniSEED records with the mstl3_pack_next() interface under
 * MSF_MAINTAINMSTL where a short trailing segment cannot fill a record
 * on its own.
 *
 * Regression test: prior versions either reset the packer's scan position to
 * the list head whenever a freshly-scanned segment returned 0 (re-packing
 * already-emitted samples as duplicates), or parked on the short segment
 * and never revisited earlier trace IDs (dropping their unflushed
 * remainder).  Since the trace list is never trimmed under MSF_MAINTAINMSTL,
 * each segment must be packed completely the first time it is visited.
 */
TEST (pack, mstl3_pack_next_maintainmstl_shortsegment)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int result = 0;
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
  msr.samprate = 100.0;

  /* Add a trace ID that sorts first and has enough samples to require
   * multiple records, leaving a sub-record remainder without a flush */
  strcpy (msr.sid, "FDSN:XX_TEST__A_A_A");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  /* Add a trace ID that sorts last and is far too short to fill a record */
  strcpy (msr.sid, "FDSN:XX_TEST__Z_Z_Z");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  msr.numsamples = 5;
  msr.samplecnt = msr.numsamples;

  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  /* Under MSF_MAINTAINMSTL every visited segment is packed completely, so
   * this first call (without MSF_FLUSHDATA) already packs both A_A_A and
   * the short Z_Z_Z segment */
  flags = MSF_MAINTAINMSTL;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
    ;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error before flush");

  /* A further flush call should find nothing left to pack; with either bug
   * this either re-packs A_A_A as duplicates or would have skipped it */
  while ((result = mstl3_pack_next (packer, MSF_FLUSHDATA, &record, &reclen)) == 1)
    ;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error during flush");

  mstl3_pack_free (&packer, &packedsamples);

  CHECK (packedsamples == SINE_DATA_SAMPLES + 5, "Packed samples do not match total input");

  mstl3_free (&mstl, 0);
}

/* Test packing miniSEED records with the mstl3_pack_next() interface under
 * MSF_MAINTAINMSTL where every segment is too short to fill a record on its
 * own.
 *
 * Regression test: a prior version parked the packer on the first short
 * segment it scanned and never revisited earlier trace IDs, so a later
 * flush call packed only the segment it was parked on.
 */
TEST (pack, mstl3_pack_next_maintainmstl_allshort)
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

  /* Common record parameters, 5 samples is far too short to fill a record */
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';
  msr.samprate = 100.0;
  msr.numsamples = 5;
  msr.samplecnt = msr.numsamples;

  strcpy (msr.sid, "FDSN:XX_TEST__A_A_A");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  strcpy (msr.sid, "FDSN:XX_TEST__Z_Z_Z");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  flags = MSF_MAINTAINMSTL;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
    recordcount++;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error before flush");

  while ((result = mstl3_pack_next (packer, MSF_FLUSHDATA, &record, &reclen)) == 1)
    recordcount++;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error during flush");

  mstl3_pack_free (&packer, &packedsamples);

  CHECK (recordcount == 2, "Unexpected total record count across both trace IDs");
  CHECK (packedsamples == 10, "Packed samples do not match total input");
  CHECK (mstl->numtraceids == 2, "MS3TraceList ID count is not 2");

  mstl3_free (&mstl, 0);
}

/* Test that mstl3_pack_next() under MSF_MAINTAINMSTL resumes scanning after
 * the last completed segment, so a trace ID added once packing has caught up
 * to the end of the list is picked up by a later call rather than requiring
 * the packer to be re-initialized.
 */
TEST (pack, mstl3_pack_next_maintainmstl_added_after)
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

  for (int idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
  }

  mstl = mstl3_init (mstl);
  REQUIRE (mstl != NULL, "mstl3_init() returned unexpected NULL");

  msr.reclen = 512;
  msr.pubversion = 1;
  msr.datasamples = isinedata;
  msr.sampletype = 'i';
  msr.samprate = 100.0;
  msr.numsamples = 5;
  msr.samplecnt = msr.numsamples;

  strcpy (msr.sid, "FDSN:XX_TEST__A_A_A");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  flags = MSF_MAINTAINMSTL;
  packer = mstl3_pack_init (mstl, 512, DE_STEIM1, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
    recordcount++;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error on first pass");
  REQUIRE (recordcount == 1, "First pass did not pack the initial trace ID");

  /* Add a trace ID that sorts after the one already packed, once the packer
   * has already scanned past it */
  strcpy (msr.sid, "FDSN:XX_TEST__Z_Z_Z");
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00.000000000Z");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");

  recordcount = 0;
  while ((result = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
    recordcount++;

  REQUIRE (result == 0, "mstl3_pack_next() returned an error packing the added trace ID");
  CHECK (recordcount == 1, "Trace ID added after the scan cursor was not packed");

  mstl3_pack_free (&packer, &packedsamples);

  CHECK (packedsamples == 10, "Packed samples mismatch");

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
  rv = mstl3_pack_ppupdate_flushidle (mstl, record_handler_int, ofp, 4096, DE_STEIM1,
                                      &packedsamples, flags, 0, NULL, flush_idle_seconds);
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
  rv = mstl3_pack_ppupdate_flushidle (mstl, record_handler_int, ofp, 4096, DE_STEIM1,
                                      &packedsamples, flags, 0, NULL, flush_idle_seconds);
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

  CHECK (totalpackedsamples == SINE_DATA_SAMPLES + SINE_DATA_SAMPLES,
         "Total packed samples mismatch");

  fclose (ofp);

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_ROLLINGBUFFER,
                    "data/reference-" TESTFILE_MSTLPACK_ROLLINGBUFFER),
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

  CHECK (!cmpfiles (TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER,
                    "data/reference-" TESTFILE_MSTLPACK_NEXT_ROLLINGBUFFER),
         "Trace list packing generator rollingbuffer reference file mismatch");

  /* Check that contents of the MS3TraceList have been removed */
  CHECK (mstl->numtraceids == 0, "MS3TraceList ID count is not 0");
  CHECK (mstl->traces.next[0] == NULL, "MS3TraceList ID list is not empty");

  mstl3_free (&mstl, 0);
}

/* Test that mstl3_pack_next() detects the segment it is actively packing
 * being merged away by an autohealing mstl3_addmsr() call.
 *
 * A gap is left between two segments of the same trace ID so that a record
 * added later, while the second (later) segment is being actively packed,
 * bridges the gap and autoheals: the segment before absorbs the bridging
 * record and the actively-packed segment, which is then freed.  Without a
 * liveness check, the next mstl3_pack_next() call would read the freed
 * segment; this test confirms it instead reports an error.
 */
TEST (pack, mstl3_pack_next_autoheal_merge)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  uint32_t flags = 0;
  int32_t isinedata[SINE_DATA_SAMPLES];
  nstime_t starttime = ms_timestr2nstime ("2012-05-12T00:00:00.0Z");

  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen = 0;
  int64_t packedsamples = 0;
  int result = 0;

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
  strcpy (msr.sid, "FDSN:XX_TEST__H_H_Z");
  msr.samprate = 1.0;

  /* Leading segment, short enough to never produce a full record on its own */
  msr.starttime = starttime;
  msr.numsamples = 2;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Trailing segment, separated by a gap and large enough to require
   * multiple records, so it remains the actively-packed segment across
   * mstl3_pack_next() calls */
  msr.starttime = ms_sampletime (starttime, 5, msr.samprate);
  msr.numsamples = SINE_DATA_SAMPLES;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  /* Initialize the packing context and pack a first record from the
   * trailing segment; the leading segment is too short to emit a record
   * without flushing and is skipped over within the same call */
  packer = mstl3_pack_init (mstl, 512, DE_INT32, flags, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() returned unexpected NULL");

  result = mstl3_pack_next (packer, 0, &record, &reclen);
  REQUIRE (result == 1, "mstl3_pack_next() did not produce an expected first record");

  /* Bridge the gap between the two segments; with autohealing this merges
   * the actively-packed (trailing) segment into the leading segment and
   * frees it */
  msr.starttime = ms_sampletime (starttime, 2, msr.samprate);
  msr.numsamples = 3;
  msr.samplecnt = msr.numsamples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, flags, NULL);
  REQUIRE (seg != NULL, "mstl3_addmsr() returned unexpected NULL");

  CHECK (mstl->traces.next[0]->numsegments == 1,
         "Expected autoheal to merge the trace ID down to a single segment");

  /* The packer's active segment was just freed by the merge above; this
   * must be detected and reported rather than dereferenced */
  result = mstl3_pack_next (packer, 0, &record, &reclen);
  CHECK (result == -1, "mstl3_pack_next() did not detect the merged/freed active segment");

  /* The samples emitted before the abort must still be reported */
  mstl3_pack_free (&packer, &packedsamples);
  CHECK (packedsamples > 0, "mstl3_pack_free() did not report samples packed before the abort");
  mstl3_free (&mstl, 0);

  /* Discard the expected error messages and restore printing */
  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);
}

/* Count records emitted, without writing them anywhere */
static void
record_counter (char *record, int reclen, void *count)
{
  (void)record;
  (void)reclen;
  if (count)
    (*(int *)count)++;
}

/* Pack a single v2 record at the specified sample rate, returning the
 * msr3_pack() result and setting 'records' to the number emitted. */
static int64_t
pack_v2_at_rate (double samprate, int *records)
{
  MS3Record msr = MS3Record_INITIALIZER;
  int32_t data[200];

  for (int idx = 0; idx < 200; idx++)
    data[idx] = idx * 3 - 100;

  *records = 0;

  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.formatversion = 2;
  msr.starttime = ms_timestr2nstime ("2012-05-12T00:00:00Z");
  msr.samprate = samprate;
  msr.encoding = DE_STEIM1;
  msr.datasamples = data;
  msr.sampletype = 'i';
  msr.numsamples = 200;
  msr.samplecnt = 200;

  return msr3_pack (&msr, record_counter, records, NULL, MSF_FLUSHDATA, 0);
}

/* Verify that sample rates which cannot be represented as a miniSEED 2
 * factor/multiplier pair are rejected rather than converted out of range,
 * and that rates at the limits of the representable range still pack.
 *
 * The maximum representable nominal rate is 32767 * 32767 = 1073676289. */
TEST (pack, msr3_pack_v2_samprate_range)
{
  int records = 0;

  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* Rates beyond the representable range, and NaN, must be rejected */
  CHECK (pack_v2_at_rate (1.0e10, &records) < 0, "Sample rate 1e10 was not rejected for v2");
  CHECK (pack_v2_at_rate (1.0e30, &records) < 0, "Sample rate 1e30 was not rejected for v2");
  CHECK (pack_v2_at_rate (3.0e9, &records) < 0, "Sample rate 3e9 was not rejected for v2");
  CHECK (pack_v2_at_rate (1073676290.0, &records) < 0,
         "Sample rate above the maximum nominal rate was not rejected for v2");
  CHECK (pack_v2_at_rate (NAN, &records) < 0, "A NaN sample rate was not rejected for v2");
  CHECK (pack_v2_at_rate (INFINITY, &records) < 0,
         "An infinite sample rate was not rejected for v2");

  /* Rates at and within the limit must still pack, confirming the range test
   * is not overly restrictive */
  CHECK (pack_v2_at_rate (1073676289.0, &records) > 0 && records > 0,
         "The maximum nominal sample rate was rejected for v2");
  CHECK (pack_v2_at_rate (32767.0, &records) > 0 && records > 0,
         "Sample rate 32767 was rejected for v2");
  CHECK (pack_v2_at_rate (100.0, &records) > 0 && records > 0,
         "Sample rate 100 was rejected for v2");
  CHECK (pack_v2_at_rate (1.0 / 3.0, &records) > 0 && records > 0,
         "Sample rate 1/3 was rejected for v2");
  CHECK (pack_v2_at_rate (-10.0, &records) > 0 && records > 0,
         "Sample period -10 (0.1 Hz) was rejected for v2");

  /* Discard the expected error messages and restore printing */
  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);
}

/* Retain the last record emitted, for parsing back */
static char lastrecord[512];
static int lastreclen = 0;

static void
record_keeper (char *record, int reclen, void *ptr)
{
  (void)ptr;
  if (reclen <= (int)sizeof (lastrecord))
  {
    memcpy (lastrecord, record, reclen);
    lastreclen = reclen;
  }
}

/* Pack a single v2 record with the specified extra headers, returning the
 * msr3_pack() result and retaining the record in 'lastrecord'. */
static int64_t
pack_v2_with_extra (const char *extra)
{
  MS3Record msr = MS3Record_INITIALIZER;
  int32_t data[4] = {1, 2, 3, 4};

  lastreclen = 0;

  strcpy (msr.sid, "FDSN:XX_TEST__B_H_Z");
  msr.reclen = 512;
  msr.pubversion = 1;
  msr.formatversion = 2;
  msr.starttime = ms_timestr2nstime ("2024-01-02T03:04:05Z");
  msr.samprate = 1.0;
  msr.encoding = DE_INT32;
  msr.datasamples = data;
  msr.sampletype = 'i';
  msr.numsamples = 4;
  msr.samplecnt = 4;
  msr.extra = (char *)extra;
  msr.extralength = (uint16_t)strlen (extra);

  return msr3_pack (&msr, record_keeper, NULL, NULL, MSF_FLUSHDATA, 0);
}

/* Verify that a calibration abort round trips through miniSEED v2 as a
 * Blockette 395, and that a sequence which cannot be represented is rejected.
 *
 * Reading a Blockette 395 produces a sequence with a Type of ABORT and an
 * EndTime, which must be accepted when packing so the blockette can be
 * written back out. */
TEST (pack, msr3_pack_v2_calibration_abort)
{
  MS3Record *rmsr = NULL;
  char gotstr[64];
  int rrv;

  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"ABORT\","
                             "\"EndTime\":\"2024-01-02T03:04:09Z\"}]}}}") > 0,
         "A calibration abort sequence was rejected for v2");
  REQUIRE (lastreclen > 0, "No record was retained");

  rrv = msr3_parse (lastrecord, lastreclen, &rmsr, 0, 0);
  REQUIRE (rrv == MS_NOERROR, "msr3_parse() did not return expected MS_NOERROR");

  rrv = mseh_get_string (rmsr, "/FDSN/Calibration/Sequence/0/Type", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find the decoded calibration Type");
  CHECK (strcmp (gotstr, "ABORT") == 0, "Decoded calibration Type is not ABORT");

  rrv = mseh_get_string (rmsr, "/FDSN/Calibration/Sequence/0/EndTime", gotstr, sizeof (gotstr));
  CHECK (rrv == 0, "mseh_get_string() did not find the decoded calibration EndTime");
  CHECK (strcmp (gotstr, "2024-01-02T03:04:09Z") == 0,
         "Decoded calibration EndTime does not match encoded value");

  msr3_free (&rmsr);

  /* A sequence with only an end time is a Blockette 395 alone */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{"
                             "\"EndTime\":\"2024-01-02T03:04:09Z\"}]}}}") > 0,
         "A calibration sequence with only an end time was rejected for v2");

  /* An abort without an end time has no content to write */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"ABORT\"}]}}}") <
             0,
         "A calibration abort without an end time was not rejected");

  /* An end time must be a string to be converted */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"EndTime\":12345}]}}}") <
             0,
         "A non-string calibration end time was not rejected");

  /* An unrecognized type cannot be mapped to a blockette */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"BOGUS\","
                             "\"EndTime\":\"2024-01-02T03:04:09Z\"}]}}}") < 0,
         "An unrecognized calibration Type was not rejected");

  /* Discard the expected error messages and restore printing */
  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);
}

/* Extra header numbers that cannot be represented in their miniSEED v2
 * blockette fields are rejected rather than converted */
TEST (pack, msr3_pack_v2_blockette_value_range)
{
  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* Values within range are accepted */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"STEP\","
                             "\"Duration\":1.5,\"StepBetween\":2,\"Amplitude\":-3.25,"
                             "\"ReferenceAmplitude\":7}]}}}") > 0,
         "In-range calibration values were rejected for v2");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Event\":{\"Detection\":[{\"Type\":\"MURDOCK\","
                             "\"SignalAmplitude\":-80.5,\"MEDSNR\":[1,2,3,4,5,255]}]}}}") > 0,
         "In-range detection values were rejected for v2");

  /* Negative and too large unsigned values */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"STEP\","
                             "\"Duration\":-1}]}}}") < 0,
         "A negative calibration Duration was not rejected");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"SINE\","
                             "\"Duration\":1e6}]}}}") < 0,
         "A calibration Duration beyond 32-bit 0.0001 second units was not rejected");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Calibration\":{\"Sequence\":[{\"Type\":\"PSEUDORANDOM\","
                             "\"ReferenceAmplitude\":5e9}]}}}") < 0,
         "A calibration ReferenceAmplitude beyond 32 bits was not rejected");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Event\":{\"Detection\":[{\"Type\":\"MURDOCK\","
                             "\"MEDSNR\":[1,2,300]}]}}}") < 0,
         "A MEDSNR value beyond 8 bits was not rejected");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Event\":{\"Detection\":[{\"Type\":\"MURDOCK\","
                             "\"MEDSNR\":[-1]}]}}}") < 0,
         "A negative MEDSNR value was not rejected");

  /* Real values beyond single precision */
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Event\":{\"Detection\":[{"
                             "\"SignalAmplitude\":1e300}]}}}") < 0,
         "A detection SignalAmplitude beyond single precision was not rejected");
  CHECK (pack_v2_with_extra ("{\"FDSN\":{\"Time\":{\"Exception\":[{"
                             "\"VCOCorrection\":-1e40}]}}}") < 0,
         "A timing exception VCOCorrection beyond single precision was not rejected");

  /* Discard the expected error messages and restore printing */
  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);
}

/* Headers packed to an unaligned buffer are identical to those packed to
 * an aligned buffer */
TEST (pack, msr3_pack_header_unaligned)
{
  MS3Record msr = MS3Record_INITIALIZER;
  static const char extra[] = "{\"FDSN\":{\"Time\":{\"Quality\":80,\"Correction\":-0.5,"
                              "\"Exception\":[{\"Time\":\"2024-01-02T03:04:05.123456Z\","
                              "\"VCOCorrection\":12.5,\"Count\":3}]},"
                              "\"Event\":{\"Detection\":[{\"Type\":\"MURDOCK\","
                              "\"SignalAmplitude\":80.5,\"OnsetTime\":\"2024-01-02T03:04:06Z\"}]},"
                              "\"Calibration\":{\"Sequence\":[{\"Type\":\"STEP\","
                              "\"BeginTime\":\"2024-01-02T03:04:07Z\",\"Duration\":1.5,"
                              "\"EndTime\":\"2024-01-02T03:04:09Z\"}]}}}";
  double aligned_storage[1024];
  double unaligned_storage[1024];
  char *aligned = (char *)aligned_storage;
  char *unaligned = (char *)unaligned_storage + 1;
  int alignedlen;
  int unalignedlen;

  strcpy (msr.sid, "FDSN:XX_TEST_00_B_H_Z");
  msr.reclen = 4096;
  msr.encoding = DE_STEIM2;
  msr.starttime = ms_timestr2nstime ("2024-01-02T03:04:05.123456789Z");
  msr.samprate = 40.5;
  msr.extra = (char *)extra;
  msr.extralength = (uint16_t)strlen (extra);

  memset (aligned_storage, 0, sizeof (aligned_storage));
  memset (unaligned_storage, 0, sizeof (unaligned_storage));
  alignedlen = msr3_pack_header3 (&msr, aligned, sizeof (aligned_storage) - 1, 0);
  unalignedlen = msr3_pack_header3 (&msr, unaligned, sizeof (unaligned_storage) - 1, 0);
  REQUIRE (alignedlen > 0, "msr3_pack_header3() failed");
  CHECK (unalignedlen == alignedlen && memcmp (aligned, unaligned, alignedlen) == 0,
         "Unaligned v3 header differs from aligned v3 header");

  memset (aligned_storage, 0, sizeof (aligned_storage));
  memset (unaligned_storage, 0, sizeof (unaligned_storage));
  alignedlen = msr3_pack_header2 (&msr, aligned, sizeof (aligned_storage) - 1, 0);
  unalignedlen = msr3_pack_header2 (&msr, unaligned, sizeof (unaligned_storage) - 1, 0);
  REQUIRE (alignedlen > 0, "msr3_pack_header2() failed");
  CHECK (unalignedlen == alignedlen && memcmp (aligned, unaligned, alignedlen) == 0,
         "Unaligned v2 header differs from aligned v2 header");
}
