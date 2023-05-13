#include "munit.h"
#include "../libmseed.h"

static MunitResult
read_tests(const MunitParameter params[], void* data) {
  MS3TraceList *mstl = NULL;
  MS3TraceID *id     = NULL;
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
  munit_assert_ptr (id->first, ==, id->last);

  mstl3_free (&mstl, 0);

  return MUNIT_OK;
}

static MunitResult
recptr_file_tests (const MunitParameter params[], void *data)
{
  MS3TraceList *mstl   = NULL;
  MS3TraceID *id       = NULL;
  MS3RecordPtr *recptr = NULL;
  nstime_t endtime;
  int64_t unpacked;
  uint32_t flags = 0;
  int32_t *int32s;
  int rv;

  char *path = "data/test-oneseries-mixedlengths-mixedorder.mseed2";

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  /* Set bit flag to build a record list */
  flags |= MSF_RECORDLIST;

  rv = ms3_readtracelist (&mstl, path, NULL, 0, flags, 0);

  munit_assert_int (rv, ==, MS_NOERROR);
  munit_assert_not_null (mstl);
  munit_assert_uint32 (mstl->numtraceids, ==, 1);

  id = mstl->traces.next[0];

  munit_assert_not_null (id->first);
  munit_assert_not_null (id->first->recordlist);
  munit_assert_int64 (id->first->samplecnt, ==, 3952);

  /* No data has been decoded */
  munit_assert_char (id->first->sampletype, ==, 0);
  munit_assert_null (id->first->datasamples);
  munit_assert_int64 (id->first->numsamples, ==, 0);

  recptr = id->first->recordlist->last;
  munit_assert_not_null (recptr->filename); /* Record is in a file */
  munit_assert_null (recptr->bufferptr);    /* Record is not in a buffer */
  munit_assert_null (recptr->fileptr);      /* File is not currently open, closed by read routine */
  munit_assert_int64 (recptr->fileoffset, ==, 1152);
  munit_assert_not_null (recptr->msr);
  munit_assert_int64 (recptr->endtime, ==, endtime);
  munit_assert_uint32 (recptr->dataoffset, ==, 64);
  munit_assert_null (recptr->next);

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  munit_assert_int64 (unpacked, ==, id->first->samplecnt);
  munit_assert_char (id->first->sampletype, ==, 'i');
  munit_assert_not_null (id->first->datasamples);
  munit_assert_int64 (id->first->numsamples, ==, 3952);

  int32s = (int32_t *)id->first->datasamples;
  munit_assert_int32 (int32s[3948], ==, 28067);
  munit_assert_int32 (int32s[3949], ==, -9565);
  munit_assert_int32 (int32s[3950], ==, -71961);
  munit_assert_int32 (int32s[3951], ==, -146622);

  mstl3_free (&mstl, 1);

  return MUNIT_OK;
}

static MunitResult
recptr_buffer_tests (const MunitParameter params[], void *data)
{
  char buffer[16256];
  FILE *fp = NULL;

  MS3TraceList *mstl   = NULL;
  MS3TraceID *id       = NULL;
  MS3RecordPtr *recptr = NULL;
  nstime_t endtime;
  int64_t unpacked;
  uint32_t flags = 0;
  int32_t *int32s;
  size_t rv;

  char *path = "data/test-oneseries-mixedlengths-mixedorder.mseed2";

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  /* Read test data into buffer */
  fp = fopen (path, "rb");
  munit_assert_not_null (fp);

  rv = fread (buffer, sizeof(buffer), 1, fp);
  munit_assert_size (rv, ==, 1);

  fclose (fp);

  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  /* Set bit flag to build a record list */
  flags |= MSF_RECORDLIST;

  rv = mstl3_readbuffer (&mstl, buffer, sizeof(buffer), 0, flags, 0, 0);

  munit_assert_int (rv, ==, 7);
  munit_assert_not_null (mstl);
  munit_assert_uint32 (mstl->numtraceids, ==, 1);

  id = mstl->traces.next[0];

  munit_assert_not_null (id->first);
  munit_assert_not_null (id->first->recordlist);
  munit_assert_int64 (id->first->samplecnt, ==, 3952);

  /* No data has been decoded */
  munit_assert_char (id->first->sampletype, ==, 0);
  munit_assert_null (id->first->datasamples);
  munit_assert_int64 (id->first->numsamples, ==, 0);

  recptr = id->first->recordlist->last;
  munit_assert_null (recptr->filename);      /* Record is not in a file */
  munit_assert_not_null (recptr->bufferptr); /* Record is in a buffer */
  munit_assert_null (recptr->fileptr);       /* File is not currently open, closed by read routine */
  munit_assert_int64 (recptr->fileoffset, ==, 0);
  munit_assert_not_null (recptr->msr);
  munit_assert_int64 (recptr->endtime, ==, endtime);
  munit_assert_uint32 (recptr->dataoffset, ==, 64);
  munit_assert_null (recptr->next);

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  munit_assert_int64 (unpacked, ==, id->first->samplecnt);
  munit_assert_char (id->first->sampletype, ==, 'i');
  munit_assert_not_null (id->first->datasamples);
  munit_assert_int64 (id->first->numsamples, ==, 3952);

  int32s = (int32_t *)id->first->datasamples;
  munit_assert_int32 (int32s[3948], ==, 28067);
  munit_assert_int32 (int32s[3949], ==, -9565);
  munit_assert_int32 (int32s[3950], ==, -71961);
  munit_assert_int32 (int32s[3951], ==, -146622);

  mstl3_free (&mstl, 1);

  return MUNIT_OK;
}

/* Create test array */
MunitTest tracelist_tests[] = {
  { (char*) "/read", read_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/recptr/file", recptr_file_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/recptr/buffer", recptr_buffer_tests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};