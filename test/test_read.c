#include "munit.h"
#include "../libmseed.h"

static char *textdata =
    "I've seen things you people wouldn't believe. Attack ships on fire off the shoulder of Orion. "
    "I watched C-beams glitter in the dark near the Tannhäuser Gate. All those moments will be lost "
    "in time, like tears...in...rain. Time to die.";

static MunitResult
parse_v2_tests (const MunitParameter params[], void *data)
{
  MS3Record *msr = NULL;
  nstime_t nstime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/test-encoding-steim2-alldifferences-LE.mseed2";

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  nstime = ms_timestr2nstime ("2016-03-02T12:36:06.069538Z");

  /* General parsing */
  flags = MSF_UNPACKDATA;
  rv = ms3_readmsr (&msr, path, NULL, NULL, flags, 0);

  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_int32 (msr->reclen, ==, 4096);
  munit_assert_string_equal (msr->sid, "FDSN:XX_TEST__L_H_Z");
  munit_assert_uint8 (msr->formatversion, ==, 2);
  munit_assert_uint8 (msr->flags, ==, 0);
  munit_assert_int64 (msr->starttime, ==, nstime);
  munit_assert_double_equal (msr->samprate, 1.0, 9);
  munit_assert_int8 (msr->encoding, ==, 11);
  munit_assert_uint8 (msr->pubversion, ==, 1);
  munit_assert_int64 (msr->samplecnt, ==, 3096);
  munit_assert_uint32 (msr->crc, ==, 0);
  munit_assert_uint16 (msr->extralength, ==, 31);
  munit_assert_uint16 (msr->datalength, ==, 4032);
  munit_assert_string_equal (msr->extra, "{\"FDSN\":{\"Time\":{\"Quality\":0}}}");
  munit_assert_size (msr->datasize, ==, 12384);
  munit_assert_int64 (msr->numsamples, ==, 3096);
  munit_assert_char (msr->sampletype, ==, 'i');

  /* Test first and last 4 decoded sample values */
  munit_assert_not_null (msr->datasamples);
  int32_t *samples = (int32_t *)msr->datasamples;
  munit_assert_int32 (samples[0], ==, -10780);
  munit_assert_int32 (samples[1], ==, -10779);
  munit_assert_int32 (samples[2], ==, -10782);
  munit_assert_int32 (samples[3], ==, -10783);

  munit_assert_int32 (samples[3092], ==, -9585);
  munit_assert_int32 (samples[3093], ==, -9743);
  munit_assert_int32 (samples[3094], ==, -9807);
  munit_assert_int32 (samples[3095], ==, -9742);

  return MUNIT_OK;
}

static MunitResult
decoding_v2_tests (const MunitParameter params[], void *data)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int32_t *int32s;
  float *float32s;
  double *float64s;
  int rv;

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  flags = MSF_UNPACKDATA;

  /* Text */
  rv = ms3_readmsr (&msr, "data/test-encoding-text.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  munit_assert_string_equal ((char *)msr->datasamples, textdata);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* CDSN */
  rv = ms3_readmsr (&msr, "data/test-encoding-CDSN.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, -96);
  munit_assert_int32 (int32s[1], ==, -87);
  munit_assert_int32 (int32s[2], ==, -100);
  munit_assert_int32 (int32s[3], ==, -128);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* DWWSSN */
  rv = ms3_readmsr (&msr, "data/test-encoding-DWWSSN.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, 6);
  munit_assert_int32 (int32s[1], ==, 5);
  munit_assert_int32 (int32s[2], ==, 1);
  munit_assert_int32 (int32s[3], ==, -9);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* SRO */
  rv = ms3_readmsr (&msr, "data/test-encoding-SRO.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, 39);
  munit_assert_int32 (int32s[1], ==, 42);
  munit_assert_int32 (int32s[2], ==, 32);
  munit_assert_int32 (int32s[3], ==, 1);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* GEOSCOPE */
  rv = ms3_readmsr (&msr, "data/test-encoding-GEOSCOPE-16bit-3exp-encoded.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  float32s = (float *)msr->datasamples;
  munit_assert_double_equal (float32s[0], -1.0625, 9);
  munit_assert_double_equal (float32s[1], -1.078125, 9);
  munit_assert_double_equal (float32s[2], -1.078125, 9);
  munit_assert_double_equal (float32s[3], -1.078125, 9);

  rv = ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);

  /* Float32 */
  rv = ms3_readmsr (&msr, "data/test-encoding-float32.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  float32s = (float *)msr->datasamples;
  munit_assert_double_equal (float32s[0], -1.0625, 9);
  munit_assert_double_equal (float32s[1], -1.078125, 9);
  munit_assert_double_equal (float32s[2], -1.078125, 9);
  munit_assert_double_equal (float32s[3], -1.078125, 9);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Float64/double */
  rv = ms3_readmsr (&msr, "data/test-encoding-float64.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  float64s = (double *)msr->datasamples;
  munit_assert_double_equal (float64s[0], -1.0625, 9);
  munit_assert_double_equal (float64s[1], -1.078125, 9);
  munit_assert_double_equal (float64s[2], -1.078125, 9);
  munit_assert_double_equal (float64s[3], -1.078125, 9);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Int16 */
  rv = ms3_readmsr (&msr, "data/test-encoding-int16.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, 6);
  munit_assert_int32 (int32s[1], ==, 5);
  munit_assert_int32 (int32s[2], ==, 1);
  munit_assert_int32 (int32s[3], ==, -9);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Int32 */
  rv = ms3_readmsr (&msr, "data/test-encoding-int32.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, -29830);
  munit_assert_int32 (int32s[1], ==, -19121);
  munit_assert_int32 (int32s[2], ==, -11992);
  munit_assert_int32 (int32s[3], ==, -34742);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Steim-1 big endian */
  rv = ms3_readmsr (&msr, "data/test-encoding-steim1-alldifferences-BE.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, 2757);
  munit_assert_int32 (int32s[1], ==, 3299);
  munit_assert_int32 (int32s[2], ==, 3030);
  munit_assert_int32 (int32s[3], ==, 2326);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Steim-1 little endian */
  rv = ms3_readmsr (&msr, "data/test-encoding-steim1-alldifferences-LE.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, 2757);
  munit_assert_int32 (int32s[1], ==, 3299);
  munit_assert_int32 (int32s[2], ==, 3030);
  munit_assert_int32 (int32s[3], ==, 2326);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Steim-2 big endian */
  rv = ms3_readmsr (&msr, "data/test-encoding-steim2-alldifferences-BE.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, -10780);
  munit_assert_int32 (int32s[1], ==, -10779);
  munit_assert_int32 (int32s[2], ==, -10782);
  munit_assert_int32 (int32s[3], ==, -10783);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  /* Steim-2 little endian */
  rv = ms3_readmsr (&msr, "data/test-encoding-steim2-alldifferences-LE.mseed2", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (msr);
  munit_assert_not_null (msr->datasamples);

  /* Test first 4 decoded sample values */
  int32s = (int32_t *)msr->datasamples;
  munit_assert_int32 (int32s[0], ==, -10780);
  munit_assert_int32 (int32s[1], ==, -10779);
  munit_assert_int32 (int32s[2], ==, -10782);
  munit_assert_int32 (int32s[3], ==, -10783);

  ms3_readmsr(&msr, NULL, NULL, NULL, flags, 0);

  return MUNIT_OK;
}

static MunitResult
error_tests (const MunitParameter params[], void *data)
{
  MS3Record *msr = NULL;
  uint32_t flags = 0;
  int rv;

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  /* No MS3Record */
  rv = ms3_readmsr (NULL, NULL, NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_GENERROR);

  /* Non-existent file */
  rv = ms3_readmsr (&msr, "no/such/file.data", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_GENERROR);
  ms3_readmsr (&msr, NULL, NULL, NULL, flags, 0);

  /* Not miniSEED */
  rv = ms3_readmsr (&msr, "Makefile", NULL, NULL, flags, 0);
  munit_assert_int (rv, ==, MS_NOTSEED);
  ms3_readmsr (&msr, NULL, NULL, NULL, flags, 0);

  return MUNIT_OK;
}

/* Create test array */
MunitTest read_tests[] = {
  { (char*) "/parse_v2", parse_v2_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/decoding_v2", decoding_v2_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/error", error_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};