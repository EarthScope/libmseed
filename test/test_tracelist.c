#include "munit.h"
#include "../libmseed.h"

static MunitResult
read_tests(const MunitParameter params[], void* data) {
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  nstime_t starttime;
  nstime_t endtime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/test-oneseries-mixedlengths-mixedorder.mseed2";

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  starttime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");
  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  flags = MSF_UNPACKDATA;
  rv = ms3_readtracelist (&mstl, path, NULL, 0, flags, 0);

  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (mstl);
  munit_assert_uint32 (mstl->numtraceids, ==, 1);

  id = mstl->traces.next[0];

  munit_assert_string_equal (id->sid, "FDSN:XX_TEST_00_L_H_Z");
  munit_assert_int64 (id->earliest, ==, starttime);
  munit_assert_int64 (id->latest, ==, id->latest);
  munit_assert_uint32 (id->pubversion, ==, 1);
  munit_assert_uint32 (id->numsegments, ==, 1);
  munit_assert_int64 (id->first->starttime, ==, starttime);
  munit_assert_int64 (id->first->endtime, ==, endtime);
  munit_assert_int64 (id->first->samplecnt, ==, 3952);
  munit_assert_char (id->first->sampletype, ==, 'i');
  munit_assert_int64 (id->first->numsamples, ==, 3952);
  munit_assert_null (id->next[0]);
  munit_assert_null (id->first->next);

  mstl3_free (&mstl, 0);

  return MUNIT_OK;
}

static MunitResult
recptr_tests (const MunitParameter params[], void *data)
{
  MS3Record *msr = NULL;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  MS3RecordPtr *recptr = NULL;
  nstime_t starttime;
  nstime_t endtime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/test-encoding-steim2-alldifferences-LE.mseed2";

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  /* General parsing */
  while ((rv = ms3_readmsr (&msr, path, NULL, NULL, flags, 0)) == MS_NOERROR)
  {

  }

  //if ()



  return MUNIT_OK;
}

/* Create test array */
MunitTest tracelist_tests[] = {
  { (char*) "/read", read_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/recptr", recptr_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};