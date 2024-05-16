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
  REQUIRE (msr != NULL, "msr3_duplicate() did not complete sucessfully");

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

