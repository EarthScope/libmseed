#include <tau/tau.h>
#include <libmseed.h>

#include "testdata.h"

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

  msr_dup = msr3_duplicate(msr, 1);
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
  ms3_readmsr(&msr, NULL, flags, 0);

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
  CHECK_EQ (msr3_endtime(msr_dup), endtime);

  CHECK_EQ (msr3_sampratehz(msr_dup), 1.0);

  CHECK_EQ (msr3_nsperiod(msr_dup), 1000000000);

  msr3_free(&msr_dup);
}

