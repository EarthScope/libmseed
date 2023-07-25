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

#define TESTFILE_TEXT_V3    "testdata-text.mseed3"
#define TESTFILE_FLOAT32_V3 "testdata-float32.mseed3"
#define TESTFILE_FLOAT64_V3 "testdata-float64.mseed3"
#define TESTFILE_INT16_V3   "testdata-int16.mseed3"
#define TESTFILE_INT32_V3   "testdata-int32.mseed3"
#define TESTFILE_STEIM1_V3  "testdata-steim1.mseed3"
#define TESTFILE_STEIM2_V3  "testdata-steim2.mseed3"
#define TESTFILE_DEFAULTS_V3  "testdata-defaults.mseed3"

TEST (write, msr)
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

  /* Set miniSEED v2 flag */
  flags |= MSF_PACKVER2;
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

  msr->datasamples = NULL;
  msr3_free (&msr);
}

TEST (write, trace)
{
  MS3Record *msr = NULL;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  uint32_t flags = MSF_FLUSHDATA; /* Set data flush flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  int idx;
  int64_t rv;

  /* Create integer sine data sets */
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
