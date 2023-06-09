#include <tau/tau.h>
#include <libmseed.h>

#include "testdata.h"

extern int cmpint32s (int32_t *arrayA, int32_t *arrayB, size_t length);
extern int cmpfloats (float *arrayA, float *arrayB, size_t length);
extern int cmpdoubles (double *arrayA, double *arrayB, size_t length);

TEST (read, v3_parse)
{
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/testdata-3channel-signal.mseed3";

  nstime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");

  /* General parsing */
  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, path, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  CHECK (msr->reclen == 478, "msr->reclen is not expected 478");
  CHECK_STREQ (msr->sid, "FDSN:IU_COLA_00_L_H_1");
  CHECK (msr->formatversion == 3, "msr->formatversion is not expected 3");
  CHECK (msr->flags == 4, "msr->flags is not expected 4");
  CHECK (msr->starttime == nstime, "msr->starttime is not expected 2010-02-27T06:50:00.069539Z");
  CHECK (msr->samprate == 1.0, "msr->samprate is not expected 1.0");
  CHECK (msr->encoding == 11, "msr->encoding is not expected 11");
  CHECK (msr->pubversion == 4, "msr->pubversion is not expected 4");
  CHECK (msr->samplecnt == 135, "msr->samplecnt is not expected 135");
  CHECK (msr->crc == 0x4F3EAB65, "msr->crc is not expected 0x4F3EAB65");
  CHECK (msr->extralength == 33, "msr->extralength is not expected 33");
  CHECK (msr->datalength == 384, "msr->datalength is not expected 384");
  CHECK_STREQ (msr->extra, "{\"FDSN\":{\"Time\":{\"Quality\":100}}}");
  CHECK (msr->datasize == 540, "msr->datasize is not expected 540");
  CHECK (msr->numsamples == 135, "msr->numsamples is not expected 135");
  CHECK (msr->sampletype == 'i', "msr->sampletype is not expected 'i'");

  /* Test first and last 4 decoded sample values */
  REQUIRE (msr->datasamples != NULL, "msr->datasamples is unexpected NULL");
  int32_t *samples = (int32_t *)msr->datasamples;
  CHECK (samples[0] == -502676, "Decoded sample value mismatch");
  CHECK (samples[1] == -504105, "Decoded sample value mismatch");
  CHECK (samples[2] == -507491, "Decoded sample value mismatch");
  CHECK (samples[3] == -506991, "Decoded sample value mismatch");

  CHECK (samples[131] == -505212, "Decoded sample value mismatch");
  CHECK (samples[132] == -499533, "Decoded sample value mismatch");
  CHECK (samples[133] == -495590, "Decoded sample value mismatch");
  CHECK (samples[134] == -496168, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (read, v2_parse)
{
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/testdata-3channel-signal.mseed2";

  nstime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");

  /* General parsing */
  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, path, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  CHECK (msr->reclen == 512, "msr->reclen is not expected 512");
  CHECK_STREQ (msr->sid, "FDSN:IU_COLA_00_L_H_1");
  CHECK (msr->formatversion == 2, "msr->formatversion is not expected 2");
  CHECK (msr->flags == 4, "msr->flags is not expected 4");
  CHECK (msr->starttime == nstime, "msr->starttime is not expected 2010-02-27T06:50:00.069539Z");
  CHECK (msr->samprate == 1.0, "msr->samprate is not expected 1.0");
  CHECK (msr->encoding == 11, "msr->encoding is not expected 11");
  CHECK (msr->pubversion == 4, "msr->pubversion is not expected 4");
  CHECK (msr->samplecnt == 135, "msr->samplecnt is not expected 135");
  CHECK (msr->crc == 0, "msr->crc is not expected 0");
  CHECK (msr->extralength == 33, "msr->extralength is not expected 33");
  CHECK (msr->datalength == 448, "msr->datalength is not expected 448");
  CHECK_STREQ (msr->extra, "{\"FDSN\":{\"Time\":{\"Quality\":100}}}");
  CHECK (msr->datasize == 540, "msr->datasize is not expected 540");
  CHECK (msr->numsamples == 135, "msr->numsamples is not expected 135");
  CHECK (msr->sampletype == 'i', "msr->sampletype is not expected 'i'");

  /* Test first and last 4 decoded sample values */
  REQUIRE (msr->datasamples != NULL, "msr->datasamples is unexpected NULL");
  int32_t *samples = (int32_t *)msr->datasamples;
  CHECK (samples[0] == -502676, "Decoded sample value mismatch");
  CHECK (samples[1] == -504105, "Decoded sample value mismatch");
  CHECK (samples[2] == -507491, "Decoded sample value mismatch");
  CHECK (samples[3] == -506991, "Decoded sample value mismatch");

  CHECK (samples[131] == -505212, "Decoded sample value mismatch");
  CHECK (samples[132] == -499533, "Decoded sample value mismatch");
  CHECK (samples[133] == -495590, "Decoded sample value mismatch");
  CHECK (samples[134] == -496168, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (read, v3_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_UNPACKDATA; /* Set data decode/unpack flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  double dsinedata[SINE_DATA_SAMPLES];
  int rv;
  int idx;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
    dsinedata[idx] = (double)(fsinedata[idx]);
  }

  /* Text */
  rv = ms3_readmsr (&msr, "data/reference-testdata-text.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK_SUBSTREQ ((char *)msr->datasamples, textdata, strlen(textdata));
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Float32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float32.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpfloats ((float *)msr->datasamples, fsinedata, msr->numsamples), "Decoded sample mismatch, float32");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Float64/double */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float64.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpdoubles ((double *)msr->datasamples, dsinedata, msr->numsamples), "Decoded sample mismatch, float64");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Int16 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int16.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, int16");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Int32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int32.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, int32");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-1 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-1");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-2 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-2");
  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (read, v2_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_UNPACKDATA; /* Set data decode/unpack flag */
  int32_t sinedata[SINE_DATA_SAMPLES];
  double dsinedata[SINE_DATA_SAMPLES];
  float *float32s;
  int32_t *int32s;
  int rv;
  int idx;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    sinedata[idx] = (int32_t)(fsinedata[idx]);
    dsinedata[idx] = (double)(fsinedata[idx]);
  }

  /* Text */
  rv = ms3_readmsr (&msr, "data/reference-testdata-text.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK_SUBSTREQ ((char *)msr->datasamples, textdata, strlen(textdata));
  ms3_readmsr(&msr, NULL, flags, 0);

  /* CDSN */
  rv = ms3_readmsr (&msr, "data/testdata-encoding-CDSN.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  CHECK (int32s[0] == -96, "Decoded sample value mismatch");
  CHECK (int32s[1] == -87, "Decoded sample value mismatch");
  CHECK (int32s[2] == -100, "Decoded sample value mismatch");
  CHECK (int32s[3] == -128, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);

  /* DWWSSN */
  rv = ms3_readmsr (&msr, "data/testdata-encoding-DWWSSN.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  CHECK (int32s[0] == 6, "Decoded sample value mismatch");
  CHECK (int32s[1] == 5, "Decoded sample value mismatch");
  CHECK (int32s[2] == 1, "Decoded sample value mismatch");
  CHECK (int32s[3] == -9, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);

  /* SRO */
  rv = ms3_readmsr (&msr, "data/testdata-encoding-SRO.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  CHECK (int32s[0] == 39, "Decoded sample value mismatch");
  CHECK (int32s[1] == 42, "Decoded sample value mismatch");
  CHECK (int32s[2] == 32, "Decoded sample value mismatch");
  CHECK (int32s[3] == 1, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);

  /* GEOSCOPE */
  rv = ms3_readmsr (&msr, "data/testdata-encoding-GEOSCOPE-16bit-3exp-encoded.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  /* Test first 4 decoded sample values */
  float32s = (float *)msr->datasamples;
  CHECK (float32s[0] == -1.0625, "Decoded sample value mismatch");
  CHECK (float32s[1] == -1.078125, "Decoded sample value mismatch");
  CHECK (float32s[2] == -1.078125, "Decoded sample value mismatch");
  CHECK (float32s[3] == -1.078125, "Decoded sample value mismatch");

  ms3_readmsr(&msr, NULL, flags, 0);

  /* Float32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float32.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpfloats ((float *)msr->datasamples, fsinedata, msr->numsamples), "Decoded sample mismatch, float32");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Float64/double */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float64.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpdoubles ((double *)msr->datasamples, dsinedata, msr->numsamples), "Decoded sample mismatch, float64");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Int16 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int16.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, int16");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Int32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int32.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, int32");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-1 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-1");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-1 little endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1-LE.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-1 LE");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-2 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-2");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Steim-2 little endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2-LE.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, sinedata, msr->numsamples), "Decoded sample mismatch, Steim-2 LE");
  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (read, byterange)
{
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;

  nstime = ms_timestr2nstime ("2010-02-27T06:51:04.069539Z");

  /* Set flag to parse byte ranges from path names */
  flags |= MSF_PNAMERANGE;

  /* Read byte range 9428-9967 from V3 format file */
  rv = ms3_readmsr (&msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@9428-9967", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Read byte range 9428-9967 from V2 format file */
  rv = ms3_readmsr (&msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed2@9344-9855", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr(&msr, NULL, flags, 0);
}

TEST (read, selection)
{
  MS3Record *msr = NULL;
  MS3FileParam *msfp = NULL;
  MS3Selections *selections = NULL;
  nstime_t nstime;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;

  nstime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");

  rv = ms3_addselect (&selections, "FDSN:IU_COLA_*_L_H_Z", NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == 0, "ms3_addselect() returned an unexpected error");

  rv = ms3_readmsr_selection (&msfp, &msr, "data/testdata-3channel-signal.mseed3", flags, selections, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr_selection() did not return expected MS_NOERROR");

  CHECK (msr->numsamples == 112, "Selection read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Selection read, unexpected record start time");

  ms3_readmsr_selection(&msfp, &msr, NULL, flags, NULL, 0);
  ms3_freeselections (selections);
}

TEST (read, oddball)
{
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = MSF_UNPACKDATA;
  int32_t *int32s = NULL;
  char timestr[50] = {0};
  int rv;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* Detection record: includes an event detection and no other data */
  rv = ms3_readmsr (&msr, "data/testdata-detection.record.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");

  CHECK (mseh_exists (msr, "/FDSN/Event/Detection/0"), "Expected /FDSN/Event/Detection does not exist");
  mseh_get_string (msr, "/FDSN/Event/Detection/0/OnsetTime", timestr, sizeof(timestr));
  CHECK_STREQ (timestr, "2004-07-28T20:28:06.185000Z");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Unapplied time correction (format version 2) */
  rv = ms3_readmsr (&msr, "data/testdata-unapplied-timecorrection.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");

  nstime = ms_timestr2nstime ("2003-05-29T02:13:23.043400Z");

  CHECK (msr->starttime == nstime, "Record start time is not expected, corrected value");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* No Blockette 1000 with Steim-1 assumption needed (format version 2) */
  rv = ms3_readmsr (&msr, "data/testdata-no-blockette1000-steim1.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");

  CHECK (msr->samplecnt == 3632, "Bare SEED data record (no B1000) incorrect sample count");
  CHECK (msr->numsamples == 3632, "Bare SEED data record (no B1000) incorrect decoded sample count");
  int32s = (int32_t *)msr->datasamples;
  CHECK (int32s[3628] == 309, "Decoded sample value mismatch");
  CHECK (int32s[3629] == 211, "Decoded sample value mismatch");
  CHECK (int32s[3630] == 117, "Decoded sample value mismatch");
  CHECK (int32s[3631] == 26, "Decoded sample value mismatch");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Invalid blockette chain (format version 2).  One could argue these should not be readable. */
  rv = ms3_readmsr (&msr, "data/testdata-invalid-blockette-offsets.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, error)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int rv;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* No MS3Record */
  rv = ms3_readmsr (NULL, NULL, flags, 0);
  CHECK (rv == MS_GENERROR, "ms3_readmsr() did not return expected MS_GENERROR with msr==NULL");
  ms3_readmsr(&msr, NULL, flags, 0);

  /* Non-existent file */
  rv = ms3_readmsr (&msr, "no/such/file.data", flags, 0);
  CHECK (rv == MS_GENERROR, "ms3_readmsr() did not return expected MS_GENERROR for file not found");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Not miniSEED */
  rv = ms3_readmsr (&msr, "Makefile", flags, 0);
  CHECK (rv == MS_NOTSEED, "ms3_readmsr() did not return expected MS_NOTSEED for non-SEED file");
  ms3_readmsr (&msr, NULL, flags, 0);
}
