#include <libmseed.h>
#include <math.h>
#include <tau/tau.h>

#include "mseedformat.h"
#include "testdata.h"

/* Handle binary mode for Windows specifically */
#if defined(LMP_WIN)
#include <fcntl.h>
#include <io.h>
#define SET_BINARY_MODE(fd) _setmode (fd, _O_BINARY)
#else
#define SET_BINARY_MODE(fd) ((void)0)
#endif

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
  flags |= MSF_VALIDATECRC;
  rv = ms3_readmsr (&msr, path, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  CHECK (msr->reclen == 414, "msr->reclen is not expected 478");
  CHECK_STREQ (msr->sid, "FDSN:IU_COLA_00_L_H_1");
  CHECK (msr->formatversion == 3, "msr->formatversion is not expected 3");
  CHECK (msr->flags == 4, "msr->flags is not expected 4");
  CHECK (msr->starttime == nstime, "msr->starttime is not expected 2010-02-27T06:50:00.069539Z");
  CHECK (msr->samprate == 1.0, "msr->samprate is not expected 1.0");
  CHECK (msr->encoding == 11, "msr->encoding is not expected 11");
  CHECK (msr->pubversion == 4, "msr->pubversion is not expected 4");
  CHECK (msr->samplecnt == 135, "msr->samplecnt is not expected 135");
  CHECK (msr->crc == 0xCE52C9F7, "msr->crc is not expected 0xCE52C9F7");
  CHECK (msr->extralength == 33, "msr->extralength is not expected 33");
  CHECK (msr->datalength == 320, "msr->datalength is not expected 384");
  CHECK_STREQ (msr->extra, "{\"FDSN\":{\"Time\":{\"Quality\":100}}}");
  CHECK (msr->datasize == 540 || msr->datasize == libmseed_prealloc_block_size,
         "msr->datasize is not 540 or prealloc block size");
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

  ms3_readmsr (&msr, NULL, flags, 0);
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
  CHECK (msr->datasize == 540 || msr->datasize == libmseed_prealloc_block_size,
         "msr->datasize is not 540 or prealloc block size");
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

  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, headeronly_v2)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int rv;

  rv = ms3_readmsr (&msr, "data/reference-testdata-headeronly.mseed2", flags, 0);

  // DEBUG
  msr3_print (msr, 1);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples == NULL, "ms3_readmsr() did not populate 'msr->datasamples'");
  CHECK (msr->numsamples == 0, "ms3_readmsr() returned unexpected value for 'msr->numsamples'");
  CHECK (msr->sampletype == 0, "ms3_readmsr() returned unexpected value for 'msr->sampletype'");
  CHECK (msr->datasize == 0, "ms3_readmsr() returned unexpected value for 'msr->datasize'");
  CHECK (msr->datalength == 0, "ms3_readmsr() returned unexpected value for 'msr->datalength'");
  CHECK (msr->extralength == 569, "ms3_readmsr() returned unexpected value for 'msr->extralength'");
  CHECK (msr->crc == 0, "ms3_readmsr() returned unexpected value for 'msr->crc'");

  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, headeronly_v3)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int rv;

  rv = ms3_readmsr (&msr, "data/reference-testdata-headeronly.mseed3", flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples == NULL, "ms3_readmsr() did not populate 'msr->datasamples'");
  CHECK (msr->numsamples == 0, "ms3_readmsr() returned unexpected value for 'msr->numsamples'");
  CHECK (msr->sampletype == 0, "ms3_readmsr() returned unexpected value for 'msr->sampletype'");
  CHECK (msr->datasize == 0, "ms3_readmsr() returned unexpected value for 'msr->datasize'");
  CHECK (msr->datalength == 0, "ms3_readmsr() returned unexpected value for 'msr->datalength'");
  CHECK (msr->extralength == 730, "ms3_readmsr() returned unexpected value for 'msr->extralength'");
  CHECK (msr->crc == 0xC22273A9, "ms3_readmsr() returned unexpected value for 'msr->crc'");

  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, v3_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_UNPACKDATA; /* Set data decode/unpack flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  float fsinedata[SINE_DATA_SAMPLES];
  int rv;
  int idx;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
    fsinedata[idx] = (float)(dsinedata[idx]);
  }

  /* Text */
  rv = ms3_readmsr (&msr, "data/reference-testdata-text.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK_SUBSTREQ ((char *)msr->datasamples, textdata, strlen (textdata));
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Float32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float32.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpfloats ((float *)msr->datasamples, fsinedata, msr->numsamples),
         "Decoded sample mismatch, float32");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Float64/double */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float64.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpdoubles ((double *)msr->datasamples, dsinedata, msr->numsamples),
         "Decoded sample mismatch, float64");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Int16 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int16.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, int16");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Int32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int32.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, int32");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-1 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-1");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-2 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2.mseed3", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-2");
  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, v2_encodings)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_UNPACKDATA; /* Set data decode/unpack flag */
  int32_t isinedata[SINE_DATA_SAMPLES];
  float fsinedata[SINE_DATA_SAMPLES];
  float *float32s;
  int32_t *int32s;
  int rv;
  int idx;

  /* Create integer and double sine data sets */
  for (idx = 0; idx < SINE_DATA_SAMPLES; idx++)
  {
    isinedata[idx] = (int32_t)(dsinedata[idx]);
    fsinedata[idx] = (float)(dsinedata[idx]);
  }

  /* Text */
  rv = ms3_readmsr (&msr, "data/reference-testdata-text.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK_SUBSTREQ ((char *)msr->datasamples, textdata, strlen (textdata));
  ms3_readmsr (&msr, NULL, flags, 0);

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

  ms3_readmsr (&msr, NULL, flags, 0);

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

  ms3_readmsr (&msr, NULL, flags, 0);

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

  ms3_readmsr (&msr, NULL, flags, 0);

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

  ms3_readmsr (&msr, NULL, flags, 0);

  /* Float32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float32.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpfloats ((float *)msr->datasamples, fsinedata, msr->numsamples),
         "Decoded sample mismatch, float32");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Float64/double */
  rv = ms3_readmsr (&msr, "data/reference-testdata-float64.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpdoubles ((double *)msr->datasamples, dsinedata, msr->numsamples),
         "Decoded sample mismatch, float64");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Int16 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int16.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, int16");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Int32 */
  rv = ms3_readmsr (&msr, "data/reference-testdata-int32.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, int32");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-1 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-1");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-1 little endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim1-LE.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-1 LE");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-2 big endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-2");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Steim-2 little endian */
  rv = ms3_readmsr (&msr, "data/reference-testdata-steim2-LE.mseed2", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  REQUIRE (msr->datasamples != NULL, "ms3_readmsr() did not populate 'msr->datasamples'");

  CHECK (!cmpint32s ((int32_t *)msr->datasamples, isinedata, msr->numsamples),
         "Decoded sample mismatch, Steim-2 LE");
  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, byterange)
{
  MS3FileParam *msfp = NULL;
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;

  nstime = ms_timestr2nstime ("2010-02-27T06:51:04.069539Z");

  /* Set flag to parse byte ranges from path names */
  flags |= MSF_PNAMERANGE;

  /* Read byte range 9428-9967 from V3 format file */
  rv = ms3_readmsr (&msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@9428-9967", flags,
                    0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Read byte range 9428-9967 from V2 format file */
  rv = ms3_readmsr (&msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed2@9344-9855", flags,
                    0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Suppress error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* A start or end value beyond INT64_MAX must not wrap to a negative
   * offset; the suffix is left unrecognized as a range and the open fails */
  rv = ms3_readmsr (&msr,
                    "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@-9999999999999999999",
                    flags, 0);
  CHECK (rv == MS_GENERROR,
         "ms3_readmsr() did not return expected MS_GENERROR for oversized end offset");
  ms3_readmsr (&msr, NULL, flags, 0);

  rv = ms3_readmsr (&msr,
                    "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@9999999999999999999-",
                    flags, 0);
  CHECK (rv == MS_GENERROR,
         "ms3_readmsr() did not return expected MS_GENERROR for oversized start offset");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* INT64_MAX itself is still a valid start offset, so the range is accepted
   * instead of being rejected outright as a malformed pattern.  Reading then
   * fails, either at the seek or on finding no data beyond the offset,
   * depending on how far the platform allows a seek, so check the parsed
   * offset rather than the return code. */
  rv = ms3_readmsr_r (&msfp, &msr,
                      "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@9223372036854775807-",
                      flags, 0);
  CHECK (rv != MS_NOERROR, "ms3_readmsr_r() did not return an error for boundary start offset");
  REQUIRE (msfp != NULL, "ms3_readmsr_r() did not populate 'msfp'");
  CHECK (msfp->startoffset == INT64_MAX, "Boundary start offset was not parsed as a byte range");
  ms3_readmsr_r (&msfp, &msr, NULL, flags, 0);

  /* A valid start offset past the end of the file is accepted and seeked to,
   * reading then finds no data */
  rv = ms3_readmsr (&msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@1000000-", flags,
                    0);
  CHECK (rv == MS_NOTSEED,
         "ms3_readmsr() did not return expected MS_NOTSEED for start offset past end of file");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* An end offset of INT64_MAX must not overflow the end-of-range check */
  rv = ms3_readmsr (&msr,
                    "data/testdata-oneseries-mixedlengths-mixedorder.mseed3@0-9223372036854775807",
                    flags, 0);
  CHECK (rv == MS_NOERROR,
         "ms3_readmsr() did not return expected MS_NOERROR for maximum end offset");
  CHECK (msr != NULL && msr->numsamples > 0, "Maximum end offset read no samples");
  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, byterange_init)
{
  MS3FileParam *msfp = NULL;
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;

  nstime = ms_timestr2nstime ("2010-02-27T06:51:04.069539Z");

  /* Read byte range 9428-9967 from V3 format file */
  msfp = ms3_msfp_init (9428, 9967, -1);
  REQUIRE (msfp != NULL, "ms3_msfp_init() did not return expected MS3FileParam");
  rv = ms3_readmsr_r (&msfp, &msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed3", flags,
                      0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr_r() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr (&msr, NULL, flags, 0);

  // /* Read byte range 9344-9855 from V2 format file */
  msfp = ms3_msfp_init (9344, 9855, -1);
  REQUIRE (msfp != NULL, "ms3_msfp_init() did not return expected MS3FileParam");
  rv = ms3_readmsr_r (&msfp, &msr, "data/testdata-oneseries-mixedlengths-mixedorder.mseed2", flags,
                      0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr_r() did not return expected MS_NOERROR");
  CHECK (msr->numsamples == 112, "Byte range read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Byte range read, unexpected record start time");
  ms3_readmsr (&msr, NULL, flags, 0);
}

TEST (read, stdin_no_close)
{
  MS3Record *msr = NULL;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;
  int stdin_fd = fileno (stdin);
  int orig_stdin_copy;
  FILE *test_data_fp;

  /* Save the original stdin descriptor to restore later */
  orig_stdin_copy = dup (stdin_fd);
  REQUIRE (orig_stdin_copy >= 0, "Failed to duplicate stdin");

  /* Redirect stdin to our test data file */
  test_data_fp = fopen ("data/testdata-3channel-signal.mseed3", "rb");
  REQUIRE (test_data_fp != NULL, "Cannot open test data file");

  REQUIRE (dup2 (fileno (test_data_fp), stdin_fd) >= 0, "Failed to redirect stdin");
  fclose (test_data_fp); /* Close FILE* wrapper; fd is now duplicated onto stdin */
  SET_BINARY_MODE (stdin_fd);

  /* Read a record from stdin via "-" */
  rv = ms3_readmsr (&msr, "-", flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readmsr() failed to read from stdin");
  REQUIRE (msr != NULL, "ms3_readmsr() did not populate 'msr'");
  CHECK (msr->numsamples == 135, "stdin read, unexpected number of decoded samples");
  CHECK_STREQ (msr->sid, "FDSN:IU_COLA_00_L_H_1");

  /* Trigger cleanup, where the descriptor was previously erroneously closed */
  ms3_readmsr (&msr, NULL, flags, 0);
  CHECK (msr == NULL, "ms3_readmsr() cleanup failed to nullify pointer");

  /* Verify stdin was NOT closed by libmseed cleanup */
  int check_fd = dup (stdin_fd);
  CHECK (check_fd >= 0, "stdin was closed by libmseed cleanup!");
  if (check_fd >= 0)
    close (check_fd);

  /* Restore original stdin */
  dup2 (orig_stdin_copy, stdin_fd);
  close (orig_stdin_copy);
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

  rv = ms3_readmsr_selection (&msfp, &msr, "data/testdata-3channel-signal.mseed3", flags,
                              selections, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr_selection() did not return expected MS_NOERROR");

  CHECK (msr->numsamples == 112, "Selection read, unexpected number of decoded samples");
  CHECK (msr->starttime == nstime, "Selection read, unexpected record start time");

  /* Drain the remainder of the stream; the matching channel is the only
   * one selected but other channels' records are skipped along the way,
   * so end of stream should still be reported as MS_ENDOFFILE */
  while ((rv = ms3_readmsr_selection (&msfp, &msr, "data/testdata-3channel-signal.mseed3", flags,
                                      selections, 0)) == MS_NOERROR)
    ;
  CHECK (rv == MS_ENDOFFILE,
         "ms3_readmsr_selection() did not return expected MS_ENDOFFILE at end of stream");

  ms3_readmsr_selection (&msfp, &msr, NULL, flags, NULL, 0);
  ms3_freeselections (selections);
}

TEST (read, selection_nomatch)
{
  MS3Record *msr = NULL;
  MS3FileParam *msfp = NULL;
  MS3TraceList *mstl = NULL;
  MS3Selections *selections = NULL;
  uint32_t flags = MSF_UNPACKDATA;
  int rv;

  /* A selection matching nothing in the file; the records are parsed and
   * skipped, which is not the same as the input not being SEED at all */
  rv = ms3_addselect (&selections, "FDSN:XX_NOSUCH_*_B_H_Z", NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == 0, "ms3_addselect() returned an unexpected error");

  while ((rv = ms3_readmsr_selection (&msfp, &msr, "data/testdata-3channel-signal.mseed3", flags,
                                      selections, 0)) == MS_NOERROR)
    ;
  CHECK (
      rv == MS_ENDOFFILE,
      "ms3_readmsr_selection() did not return expected MS_ENDOFFILE when selections match nothing");

  ms3_readmsr_selection (&msfp, &msr, NULL, flags, NULL, 0);

  rv = ms3_readtracelist_selection (&mstl, "data/testdata-3channel-signal.mseed3", NULL, selections,
                                    0, flags, 0);
  CHECK (rv == MS_NOERROR, "ms3_readtracelist_selection() did not return expected MS_NOERROR when "
                           "selections match nothing");
  REQUIRE (mstl != NULL, "ms3_readtracelist_selection() did not populate 'mstl'");
  CHECK (mstl->numtraceids == 0, "Trace list unexpectedly populated by a non-matching selection");

  mstl3_free (&mstl, 0);
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

  CHECK (mseh_exists (msr, "/FDSN/Event/Detection/0"),
         "Expected /FDSN/Event/Detection does not exist");
  mseh_get_string (msr, "/FDSN/Event/Detection/0/OnsetTime", timestr, sizeof (timestr));
  CHECK_STREQ (timestr, "2004-07-28T20:28:06.185000Z");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Unapplied time correction (format version 2) */
  rv = ms3_readmsr (&msr, "data/testdata-unapplied-timecorrection.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");

  nstime = ms_timestr2nstime ("2003-05-29T02:13:23.043400Z");

  CHECK (msr->starttime == nstime, "Record start time is not expected, corrected value");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* No Blockette 1000 with Steim-1 assumption needed (format version 2) */
  rv = ms3_readmsr (&msr, "data/testdata-no-blockette1000-steim1.mseed2", flags, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readmsr() did not return expected MS_NOERROR");

  CHECK (msr->samplecnt == 3632, "Bare SEED data record (no B1000) incorrect sample count");
  CHECK (msr->numsamples == 3632,
         "Bare SEED data record (no B1000) incorrect decoded sample count");
  int32s = (int32_t *)msr->datasamples;
  CHECK (int32s[3628] == 309, "Decoded sample value mismatch");
  CHECK (int32s[3629] == 211, "Decoded sample value mismatch");
  CHECK (int32s[3630] == 117, "Decoded sample value mismatch");
  CHECK (int32s[3631] == 26, "Decoded sample value mismatch");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Invalid blockette chain (format version 2): FSDH blockette offset (40)
   * falls within the 48-byte fixed header, not a valid blockette start. */
  rv = ms3_readmsr (&msr, "data/testdata-invalid-blockette-offsets.mseed2", flags, 0);
  REQUIRE (rv == MS_GENERROR, "ms3_readmsr() did not return expected MS_GENERROR");
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
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Non-existent file */
  rv = ms3_readmsr (&msr, "no/such/file.data", flags, 0);
  CHECK (rv == MS_GENERROR, "ms3_readmsr() did not return expected MS_GENERROR for file not found");
  ms3_readmsr (&msr, NULL, flags, 0);

  /* Not miniSEED */
  rv = ms3_readmsr (&msr, "Makefile", flags, 0);
  CHECK (rv == MS_NOTSEED, "ms3_readmsr() did not return expected MS_NOTSEED for non-SEED file");
  ms3_readmsr (&msr, NULL, flags, 0);
}

static char packbuf[1024];
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

/* Verify a version 3 record with a non-finite fixed-header sample rate is
 * rejected instead of being used to compute a garbage end time. */
TEST (read, v3_invalid_samplerate)
{
  MS3Record *msr = NULL;
  MS3Record *parsed = NULL;
  int8_t swapflag = (ms_bigendianhost ()) ? 1 : 0; /* miniSEED 3 is little endian */
  int64_t packedsamples = 0;
  int rv;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__L_H_Z");
  msr->reclen = 512;
  msr->formatversion = 3;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2020-01-01T00:00:00Z");
  msr->samprate = 1.0;

  packbuflen = 0;
  rv = msr3_pack (msr, record_handler_buf, NULL, &packedsamples, MSF_FLUSHDATA, 0);
  REQUIRE (rv == 1, "msr3_pack() returned unexpected value");
  REQUIRE (packbuflen > 0, "msr3_pack() did not produce any output");

  /* Patch the fixed-header sample rate to NaN */
  *pMS3FSDH_SAMPLERATE (packbuf) = HO8f (NAN, swapflag);

  rv = msr3_parse (packbuf, (uint64_t)packbuflen, &parsed, 0, 0);
  CHECK (rv != MS_NOERROR, "msr3_parse() did not reject a NaN sample rate");

  msr3_free (&msr);
  msr3_free (&parsed);
}

/* Verify a Blockette 100 with an invalid (negative) sample rate is ignored,
 * leaving the nominal fixed-header rate in place, instead of being applied. */
TEST (read, v2_b100_invalid_samplerate)
{
  MS3Record *msr = NULL;
  MS3Record *parsed = NULL;
  int8_t swapflag = (ms_bigendianhost ()) ? 0 : 1; /* miniSEED 2 is big endian */
  int64_t packedsamples = 0;
  int b100offset = MS2FSDH_LENGTH + 8; /* Fixed header + Blockette 1000 */
  int rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__L_H_Z");
  msr->reclen = 128;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2020-01-01T00:00:00Z");
  msr->samprate = 1.0;

  packbuflen = 0;
  rv = msr3_pack (msr, record_handler_buf, NULL, &packedsamples, MSF_FLUSHDATA | MSF_PACKVER2, 0);
  REQUIRE (rv == 1, "msr3_pack() returned unexpected value");
  REQUIRE (packbuflen >= b100offset + 12,
           "msr3_pack() did not produce enough output for a Blockette 100");

  /* Splice in a Blockette 100 with a negative sample rate after the mandatory Blockette 1000 */
  *pMS2B1000_NEXT (packbuf + MS2FSDH_LENGTH) = HO2u ((uint16_t)b100offset, swapflag);
  *pMS2FSDH_NUMBLOCKETTES (packbuf) += 1;

  *pMS2B100_TYPE (packbuf + b100offset) = HO2u (100, swapflag);
  *pMS2B100_NEXT (packbuf + b100offset) = 0;
  *pMS2B100_SAMPRATE (packbuf + b100offset) = HO4f (-5.0f, swapflag);
  *pMS2B100_FLAGS (packbuf + b100offset) = 0;
  memset (pMS2B100_RESERVED (packbuf + b100offset), 0, 3);

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  rv = msr3_parse (packbuf, (uint64_t)packbuflen, &parsed, 0, 0);
  REQUIRE (rv == MS_NOERROR, "msr3_parse() did not return expected MS_NOERROR");
  REQUIRE (parsed != NULL, "msr3_parse() did not populate 'parsed'");
  CHECK (parsed->samprate == 1.0, "Invalid Blockette 100 sample rate was not ignored");

  msr3_free (&msr);
  msr3_free (&parsed);
}

/* Verify a miniSEED 2 buffer shorter than the fixed header is parsed as
 * incomplete, with exactly-sized buffers so out-of-bounds reads are detectable. */
TEST (read, v2_truncated_header)
{
  MS3Record *msr = NULL;
  MS3Record *parsed = NULL;
  int64_t packedsamples = 0;
  char *exact;
  int length;
  int rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  strcpy (msr->sid, "FDSN:XX_TEST__L_H_Z");
  msr->reclen = 128;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2020-01-01T00:00:00Z");
  msr->samprate = 1.0;

  packbuflen = 0;
  rv = msr3_pack (msr, record_handler_buf, NULL, &packedsamples, MSF_FLUSHDATA | MSF_PACKVER2, 0);
  REQUIRE (rv == 1, "msr3_pack() returned unexpected value");
  REQUIRE (packbuflen >= MS2FSDH_LENGTH, "msr3_pack() did not produce a full fixed header");

  for (length = MINRECLEN; length < MS2FSDH_LENGTH; length++)
  {
    exact = (char *)malloc ((size_t)length);
    REQUIRE (exact != NULL, "Cannot allocate memory");
    memcpy (exact, packbuf, (size_t)length);

    rv = msr3_parse (exact, (uint64_t)length, &parsed, 0, 0);
    CHECK (rv == MINRECLEN, "msr3_parse() did not request more data for a truncated header");
    CHECK (parsed == NULL, "msr3_parse() populated 'parsed' for a truncated header");

    free (exact);
  }

  msr3_free (&msr);
  msr3_free (&parsed);
}

/* Build a 2-frame Steim buffer: X0=100, Xn=104, differences 7,1,1,1,1 as 8-bit
 * words, in frame 0 when firstframe is nonzero, otherwise in frame 1.  A nonzero
 * trailnibble places trailword with that nibble in the word after the differences. */
static void
build_steim_frames (uint32_t *frames, int firstframe, int trailnibble, uint32_t trailword)
{
  uint32_t *dframe = firstframe ? frames : frames + 16;
  int dstart = firstframe ? 3 : 1;

  memset (frames, 0, 32 * sizeof (uint32_t));

  /* Two words of 4 x 8-bit differences, nibble 01 (valid for Steim1 and Steim2) */
  dframe[dstart]     = 0x07010101u;
  dframe[dstart + 1] = 0x01000000u;
  dframe[0] |= (0x1u << (30 - 2 * dstart)) | (0x1u << (30 - 2 * (dstart + 1)));

  if (trailnibble)
  {
    dframe[dstart + 2] = trailword;
    dframe[0] |= ((uint32_t)trailnibble << (30 - 2 * (dstart + 2)));
  }

  frames[1] = 100;
  frames[2] = 104;

  if (!ms_bigendianhost ())
    for (int idx = 0; idx < 32; idx++)
      frames[idx] = ((frames[idx] & 0xFFu) << 24) | ((frames[idx] & 0xFF00u) << 8) |
                    ((frames[idx] >> 8) & 0xFF00u) | (frames[idx] >> 24);
}

TEST (read, steim_first_difference)
{
  uint32_t frames[32];
  int32_t output[5];
  int32_t expected[5] = {100, 101, 102, 103, 104};
  uint8_t encodings[2] = {DE_STEIM1, DE_STEIM2};
  char sampletype;
  int64_t rv;

  for (int enc = 0; enc < 2; enc++)
  {
    for (int firstframe = 1; firstframe >= 0; firstframe--)
    {
      build_steim_frames (frames, firstframe, 0, 0);
      memset (output, 0, sizeof (output));

      rv = ms_decode_data (frames, sizeof (frames), encodings[enc], 5, output, sizeof (output),
                           &sampletype, 1 - ms_bigendianhost (), NULL, 0);
      REQUIRE (rv == 5, "ms_decode_data() did not decode 5 samples");
      CHECK (cmpint32s (output, expected, 5) == 0,
             "Steim decode is wrong when the first difference is not in frame 0");
    }
  }
}

TEST (read, steim2_trailing_undefined)
{
  uint32_t frames[32];
  int32_t output[9];
  int32_t expected[5] = {100, 101, 102, 103, 104};
  /* Undefined codes: nibble 10 with dnib 00, nibble 11 with dnib 11 */
  int nibbles[2]      = {2, 3};
  uint32_t words[2]   = {0x00000000u, 0xC0000000u};
  char sampletype;
  int64_t rv;

  for (int idx = 0; idx < 2; idx++)
  {
    /* Undefined word follows the 8 differences (5 used) in frame 0 */
    build_steim_frames (frames, 1, nibbles[idx], words[idx]);
    memset (output, 0, sizeof (output));

    rv = ms_decode_data (frames, sizeof (frames), DE_STEIM2, 5, output, sizeof (output),
                         &sampletype, 1 - ms_bigendianhost (), NULL, 0);
    REQUIRE (rv == 5, "ms_decode_data() rejected an undefined code after the last needed difference");
    CHECK (cmpint32s (output, expected, 5) == 0, "Steim2 decode is wrong with a trailing undefined code");

    /* Reaching the undefined word is an error */
    rv = ms_decode_data (frames, sizeof (frames), DE_STEIM2, 9, output, sizeof (output),
                         &sampletype, 1 - ms_bigendianhost (), NULL, 0);
    CHECK (rv < 0, "ms_decode_data() accepted an undefined Steim2 code that was needed");
  }
}
