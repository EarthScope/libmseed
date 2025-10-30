#include <libmseed.h>
#include <tau/tau.h>

extern int cmpfiles (char *fileA, char *fileB);

/* Write test output files.  Reference files are at "data/reference-<name>" */
#define TESTFILE_REPACK_V3 "testdata-repack.mseed3"
#define TESTFILE_REPACK_V2 "testdata-repack.mseed2"

#define V2INPUT_RECORD "data/reference-testdata-defaults.mseed2"

TEST (repack, v3)
{
  MS3Record *msr = NULL;
  char buffer[8192];
  uint32_t flags;
  int packedlength;
  int rv;

  /* Read v2 input data */
  flags = MSF_UNPACKDATA;
  rv    = ms3_readmsr (&msr, V2INPUT_RECORD, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");

  /* Change some header fields */
  strcpy (msr->sid, "FDSN:XX_REPAK__H_H_Z");
  msr->starttime = ms_timestr2nstime ("2008-05-12T13:44:55.123456789Z");
  msr->samprate = 100.0;
  msr->pubversion = 2;

  /* Repack to v3 record */
  packedlength = msr3_repack_mseed3 (msr, buffer, sizeof (buffer), 0);

  CHECK (packedlength > 0, "msr3_repack_mseed3() returned an error");

  /* Write output */
  FILE *fd = fopen (TESTFILE_REPACK_V3, "wb");

  CHECK (fd != NULL, "Failed to open output file");

  rv = fwrite (buffer, 1, packedlength, fd);

  CHECK (rv == packedlength, "Failed to write output file");
  CHECK (fclose (fd) == 0, "Failed to close output file");

  /* Compare to reference */
  rv = cmpfiles (TESTFILE_REPACK_V3, "data/reference-" TESTFILE_REPACK_V3);

  CHECK (rv == 0, "Repacked v3 record does not match reference");

  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (repack, v2)
{
  MS3Record *msr = NULL;
  char buffer[8192];
  uint32_t flags;
  int packedlength;
  int rv;

  /* Read v2 input data */
  flags = MSF_UNPACKDATA;
  rv    = ms3_readmsr (&msr, V2INPUT_RECORD, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");

  /* Change some header fields */
  strcpy (msr->sid, "FDSN:XX_REPAK__H_H_Z");
  msr->starttime = ms_timestr2nstime ("2008-05-12T13:44:55.123456789Z");
  msr->samprate = 100.0;
  msr->pubversion = 2;

  /* Repack to v2 record */
  packedlength = msr3_repack_mseed2 (msr, buffer, sizeof (buffer), 0);

  CHECK (packedlength > 0, "msr3_repack_mseed2() returned an error");

  /* Write output */
  FILE *fd = fopen (TESTFILE_REPACK_V2, "wb");

  CHECK (fd != NULL, "Failed to open output file");

  rv = fwrite (buffer, 1, packedlength, fd);

  CHECK (rv == packedlength, "Failed to write output file");
  CHECK (fclose (fd) == 0, "Failed to close output file");

  /* Compare to reference */
  rv = cmpfiles (TESTFILE_REPACK_V2, "data/reference-" TESTFILE_REPACK_V2);

  CHECK (rv == 0, "Repacked v2 record does not match reference");

  ms3_readmsr(&msr, NULL, flags, 0);
}
