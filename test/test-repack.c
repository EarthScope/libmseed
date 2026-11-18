#include <libmseed.h>
#include <math.h>
#include <tau/tau.h>

extern int cmpfiles (char *fileA, char *fileB);

/* Write test output files.  Reference files are at "data/reference-<name>" */
#define TESTFILE_REPACK_V3 "testdata-repack.mseed3"
#define TESTFILE_REPACK_V2 "testdata-repack.mseed2"

#define V2INPUT_RECORD "data/reference-testdata-defaults.mseed2"

static char packbuf[4096];
static int packbuflen = 0;

static void
record_handler_buf (char *record, int reclen, void *handlerdata)
{
  (void)handlerdata;

  if (packbuflen + reclen <= (int)sizeof (packbuf))
  {
    memcpy (packbuf + packbuflen, record, reclen);
    packbuflen += reclen;
  }
}

TEST (repack, v3)
{
  MS3Record *msr = NULL;
  char buffer[8192];
  uint32_t flags;
  int packedlength;
  int rv;

  /* Read v2 input data */
  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, V2INPUT_RECORD, flags, 0);

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

  rv = (int)fwrite (buffer, 1, packedlength, fd);

  CHECK (rv == packedlength, "Failed to write output file");
  CHECK (fclose (fd) == 0, "Failed to close output file");

  /* Compare to reference */
  rv = cmpfiles (TESTFILE_REPACK_V3, "data/reference-" TESTFILE_REPACK_V3);

  CHECK (rv == 0, "Repacked v3 record does not match reference");

  ms3_readmsr (&msr, NULL, flags, 0);
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
  rv = ms3_readmsr (&msr, V2INPUT_RECORD, flags, 0);

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

  rv = (int)fwrite (buffer, 1, packedlength, fd);

  CHECK (rv == packedlength, "Failed to write output file");
  CHECK (fclose (fd) == 0, "Failed to close output file");

  /* Compare to reference */
  rv = cmpfiles (TESTFILE_REPACK_V2, "data/reference-" TESTFILE_REPACK_V2);

  CHECK (rv == 0, "Repacked v2 record does not match reference");

  ms3_readmsr (&msr, NULL, flags, 0);
}

/* Test that negative /FDSN/Time/Correction values round-trip through a v2
 * record without losing precision to truncation-toward-zero rounding.
 */
TEST (repack, v2_negative_time_correction)
{
  MS3Record *msr = NULL;
  MS3Record *parsed = NULL;
  double correction;
  int64_t packedsamples = 0;
  int rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__L_H_Z");
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2020-01-01T00:00:00Z");
  msr->samprate = 0;

  msr->extra = "{\"FDSN\":{\"Time\":{\"Correction\":-1.5}}}";
  msr->extralength = (uint16_t)strlen (msr->extra);

  packbuflen = 0;
  rv = msr3_pack (msr, record_handler_buf, NULL, &packedsamples, MSF_FLUSHDATA | MSF_PACKVER2, 0);
  REQUIRE (rv == 1, "msr3_pack() returned unexpected value");
  REQUIRE (packbuflen > 0, "msr3_pack() did not produce any output");

  rv = msr3_parse (packbuf, (uint64_t)packbuflen, &parsed, 0, 0);
  REQUIRE (rv == MS_NOERROR, "msr3_parse() did not return expected MS_NOERROR");
  REQUIRE (parsed != NULL, "msr3_parse() did not populate 'parsed'");

  rv = mseh_get_number (parsed, "/FDSN/Time/Correction", &correction);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (fabs (correction - (-1.5)) < 0.00001, "/FDSN/Time/Correction did not round-trip -1.5s");

  /* Sub-100-usec correction, rounds to the nearest 100 usec unit */
  msr->extra = "{\"FDSN\":{\"Time\":{\"Correction\":-0.00012}}}";
  msr->extralength = (uint16_t)strlen (msr->extra);

  packbuflen = 0;
  rv = msr3_pack (msr, record_handler_buf, NULL, &packedsamples, MSF_FLUSHDATA | MSF_PACKVER2, 0);
  REQUIRE (rv == 1, "msr3_pack() returned unexpected value");

  msr3_parse (packbuf, (uint64_t)packbuflen, &parsed, 0, 0);
  rv = mseh_get_number (parsed, "/FDSN/Time/Correction", &correction);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (fabs (correction - (-0.0001)) < 0.00001,
         "/FDSN/Time/Correction did not round-trip -0.00012s");

  msr->extra = NULL;
  msr->extralength = 0;
  msr3_free (&msr);
  msr3_free (&parsed);
}

/* Repack the first record of a file to the specified version, parse the
 * result and confirm the decoded samples match the original.
 */
static void
repack_roundtrip (const char *path, int version)
{
  MS3Record *msr = NULL;
  MS3Record *parsed = NULL;
  char buffer[8192];
  uint8_t samplesize;
  int packedlength;
  int rv;

  rv = ms3_readmsr (&msr, path, MSF_UNPACKDATA, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  CHECK (msr != NULL && msr->numsamples > 0, "ms3_readmsr() did not unpack samples");

  if (rv != MS_NOERROR || msr == NULL || msr->numsamples <= 0)
    goto cleanup;

  if (version == 3)
  {
    packedlength = msr3_repack_mseed3 (msr, buffer, sizeof (buffer), 0);
  }
  else
  {
    /* v2 record length must be a power of 2 */
    if (msr->formatversion == 3)
      msr->reclen = 1024;

    packedlength = msr3_repack_mseed2 (msr, buffer, sizeof (buffer), 0);
  }

  CHECK (packedlength > 0, "Repack returned an error");

  if (packedlength <= 0)
    goto cleanup;

  rv = msr3_parse (buffer, packedlength, &parsed, MSF_UNPACKDATA, 0);
  CHECK (rv == MS_NOERROR && parsed != NULL, "msr3_parse() of repacked record failed");

  if (rv != MS_NOERROR || parsed == NULL)
    goto cleanup;

  CHECK (parsed->formatversion == version, "Repacked format version is not expected");
  CHECK (parsed->encoding == msr->encoding, "Repacked encoding does not match");
  CHECK (parsed->sampletype == msr->sampletype, "Repacked sample type does not match");
  CHECK (parsed->numsamples == msr->numsamples, "Repacked sample count does not match");

  samplesize = ms_samplesize (msr->sampletype);
  if (parsed->numsamples == msr->numsamples && parsed->sampletype == msr->sampletype)
    CHECK (memcmp (parsed->datasamples, msr->datasamples,
                   (size_t)msr->numsamples * samplesize) == 0,
           "Repacked samples do not match original");

cleanup:
  msr3_free (&parsed);
  ms3_readmsr (&msr, NULL, 0, 0);
}

TEST (repack, v3_payload_byteorder)
{
  repack_roundtrip ("data/reference-testdata-int16.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-int32.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-float32.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-float64.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-steim1-LE.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-steim2-LE.mseed2", 3);
  repack_roundtrip ("data/reference-testdata-text.mseed2", 3);
}

TEST (repack, v2_payload_byteorder)
{
  repack_roundtrip ("data/reference-testdata-int16.mseed3", 2);
  repack_roundtrip ("data/reference-testdata-int32.mseed3", 2);
  repack_roundtrip ("data/reference-testdata-float32.mseed3", 2);
  repack_roundtrip ("data/reference-testdata-float64.mseed3", 2);
  repack_roundtrip ("data/reference-testdata-steim2.mseed3", 2);
  repack_roundtrip ("data/reference-testdata-steim1-LE.mseed2", 2);
  repack_roundtrip ("data/reference-testdata-steim2-LE.mseed2", 2);
}

/* A miniSEED 2 header-only record has a data offset of 0, meaning no payload,
 * regardless of the encoding declared in Blockette 1000.  Repacking must not
 * carry any part of the record as encoded data. */
TEST (repack, v2_headeronly_steim)
{
  MS3Record *msr = NULL;
  char record[4096];
  char repacked[4096];
  uint32_t dataoffset = 1;
  uint32_t datasize = 1;
  uint32_t datalength;
  FILE *fp = NULL;
  size_t readcount;
  int reclen;
  int rv;

  fp = fopen ("data/reference-testdata-headeronly.mseed2", "rb");
  REQUIRE (fp != NULL, "File pointer is unexpected NULL");
  readcount = fread (record, sizeof (record), 1, fp);
  fclose (fp);
  REQUIRE (readcount == 1, "fread() did not read a complete record");

  REQUIRE ((uint8_t)record[48] == 0x03 && (uint8_t)record[49] == 0xE8,
           "Blockette 1000 is not at expected offset 48");
  REQUIRE (record[44] == 0 && record[45] == 0, "Data offset is not expected 0");

  record[52] = DE_STEIM2; /* Blockette 1000 encoding */

  rv = msr3_parse (record, sizeof (record), &msr, 0, 0);
  REQUIRE (rv == MS_NOERROR, "msr3_parse() did not return expected MS_NOERROR");
  CHECK (msr->samplecnt == 0, "msr->samplecnt is not expected 0");

  rv = msr3_data_bounds (msr, &dataoffset, &datasize);
  CHECK (rv == 0, "msr3_data_bounds() did not return expected 0");
  CHECK (datasize == 0, "msr3_data_bounds() datasize is not expected 0");

  reclen = msr3_repack_mseed3 (msr, repacked, sizeof (repacked), 0);
  REQUIRE (reclen > 0, "msr3_repack_mseed3() did not return a record length");
  memcpy (&datalength, repacked + 36, sizeof (datalength));
  if (ms_bigendianhost ())
    ms_gswap4 (&datalength);
  CHECK (datalength == 0, "Repacked v3 data length is not expected 0");

  reclen = msr3_repack_mseed2 (msr, repacked, sizeof (repacked), 0);
  CHECK (reclen == 4096, "msr3_repack_mseed2() did not return expected 4096");

  msr3_free (&msr);
}
