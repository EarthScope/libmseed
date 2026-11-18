#include <libmseed.h>
#include <math.h>
#include <tau/tau.h>
#include <time.h>

TEST (msr3, utils)
{
  MS3Record *msr = NULL;
  MS3Record *msr_dup = NULL;
  nstime_t endtime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/testdata-3channel-signal.mseed3";

  /* General parsing */
  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, path, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");

  msr_dup = msr3_duplicate (msr, 1);
  REQUIRE (msr_dup != NULL, "msr3_duplicate() did not complete successfully");

  CHECK_EQ (msr->reclen, msr_dup->reclen);
  CHECK_EQ (msr->swapflag, msr_dup->swapflag);
  CHECK_STREQ (msr->sid, msr_dup->sid);
  CHECK_EQ (msr->formatversion, msr_dup->formatversion);
  CHECK_EQ (msr->flags, msr_dup->flags);
  CHECK_EQ (msr->starttime, msr_dup->starttime);
  CHECK_EQ (msr->samprate, msr_dup->samprate);
  CHECK_EQ (msr->encoding, msr_dup->encoding);
  CHECK_EQ (msr->pubversion, msr_dup->pubversion);
  CHECK_EQ (msr->samplecnt, msr_dup->samplecnt);
  CHECK_EQ (msr->crc, msr_dup->crc);
  CHECK_EQ (msr->extralength, msr_dup->extralength);
  CHECK_EQ (msr->datalength, msr_dup->datalength);
  CHECK_STREQ (msr->extra, msr_dup->extra);
  CHECK_EQ (msr->datasize, msr_dup->datasize);
  CHECK_EQ (msr->numsamples, msr_dup->numsamples);
  CHECK_EQ (msr->sampletype, msr_dup->sampletype);

  /* Clean up original MS3Record and file reading parameters */
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Test first and last 4 decoded sample values */
  REQUIRE (msr_dup->datasamples != NULL, "msr_dup->datasamples is unexpected NULL");
  int32_t *samples = (int32_t *)msr_dup->datasamples;
  CHECK (samples[0] == -502676, "Decoded sample value mismatch");
  CHECK (samples[1] == -504105, "Decoded sample value mismatch");
  CHECK (samples[2] == -507491, "Decoded sample value mismatch");
  CHECK (samples[3] == -506991, "Decoded sample value mismatch");

  CHECK (samples[131] == -505212, "Decoded sample value mismatch");
  CHECK (samples[132] == -499533, "Decoded sample value mismatch");
  CHECK (samples[133] == -495590, "Decoded sample value mismatch");
  CHECK (samples[134] == -496168, "Decoded sample value mismatch");

  endtime = ms_timestr2nstime ("2010-02-27T06:52:14.069539Z");
  CHECK_EQ (msr3_endtime (msr_dup), endtime);

  CHECK_EQ (msr3_sampratehz (msr_dup), 1.0);

  CHECK_EQ (msr3_nsperiod (msr_dup), 1000000000);

  msr3_free (&msr_dup);
}

TEST (msr3, duplicate_extra)
{
  MS3Record *msr = NULL;
  MS3Record *msr_dup = NULL;
  uint32_t flags = 0;
  int rv;

  char *path = "data/testdata-3channel-signal.mseed3";

  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, path, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->extra != NULL, "test data is expected to have extra headers");
  REQUIRE (msr->extralength > 0, "test data is expected to have extra headers");

  /* Duplicate with both data and extra headers */
  msr_dup = msr3_duplicate_extra (msr, 1, 1);
  REQUIRE (msr_dup != NULL, "msr3_duplicate_extra() did not complete successfully");
  CHECK_EQ (msr->extralength, msr_dup->extralength);
  CHECK_STREQ (msr->extra, msr_dup->extra);
  CHECK_EQ (msr->numsamples, msr_dup->numsamples);
  REQUIRE (msr_dup->datasamples != NULL, "msr_dup->datasamples is unexpected NULL");
  msr3_free (&msr_dup);

  /* Duplicate with data but without extra headers */
  msr_dup = msr3_duplicate_extra (msr, 1, 0);
  REQUIRE (msr_dup != NULL, "msr3_duplicate_extra() did not complete successfully");
  CHECK_EQ (msr_dup->extralength, 0);
  CHECK (msr_dup->extra == NULL, "msr_dup->extra is unexpected non-NULL");
  CHECK_EQ (msr->numsamples, msr_dup->numsamples);
  REQUIRE (msr_dup->datasamples != NULL, "msr_dup->datasamples is unexpected NULL");
  msr3_free (&msr_dup);

  /* Duplicate with extra headers but without data */
  msr_dup = msr3_duplicate_extra (msr, 0, 1);
  REQUIRE (msr_dup != NULL, "msr3_duplicate_extra() did not complete successfully");
  CHECK_EQ (msr->extralength, msr_dup->extralength);
  CHECK_STREQ (msr->extra, msr_dup->extra);
  CHECK_EQ (msr_dup->numsamples, 0);
  CHECK (msr_dup->datasamples == NULL, "msr_dup->datasamples is unexpected non-NULL");
  msr3_free (&msr_dup);

  /* Duplicate without data or extra headers */
  msr_dup = msr3_duplicate_extra (msr, 0, 0);
  REQUIRE (msr_dup != NULL, "msr3_duplicate_extra() did not complete successfully");
  CHECK_EQ (msr_dup->extralength, 0);
  CHECK (msr_dup->extra == NULL, "msr_dup->extra is unexpected non-NULL");
  CHECK_EQ (msr_dup->numsamples, 0);
  CHECK (msr_dup->datasamples == NULL, "msr_dup->datasamples is unexpected non-NULL");
  msr3_free (&msr_dup);

  ms3_readmsr (&msr, NULL, flags, 0);
}

/* Latency is measured to the last sample, for a sample rate given either
 * in samples per second or as a (negative) sample period in seconds */
TEST (msr3, host_latency)
{
  MS3Record msr = MS3Record_INITIALIZER;
  double latency_first;
  double latency_last;

  msr.starttime = MS_EPOCH2NSTIME ((int64_t)time (NULL) - 1000);

  /* Rate: 0.1 samples per second, a 10 second period */
  msr.samprate = 0.1;
  msr.samplecnt = 1;
  latency_first = msr3_host_latency (&msr);
  msr.samplecnt = 2;
  latency_last = msr3_host_latency (&msr);
  CHECK (fabs ((latency_first - latency_last) - 10.0) < 1.5,
         "Latency for positive sample rate does not account for the last sample");

  /* Period: 10 seconds per sample */
  msr.samprate = -10.0;
  msr.samplecnt = 1;
  latency_first = msr3_host_latency (&msr);
  msr.samplecnt = 2;
  latency_last = msr3_host_latency (&msr);
  CHECK (fabs ((latency_first - latency_last) - 10.0) < 1.5,
         "Latency for negative sample period does not account for the last sample");
}
