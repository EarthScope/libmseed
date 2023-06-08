#include <tau/tau.h>
#include <libmseed.h>

TEST (trace, read)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id     = NULL;
  nstime_t starttime;
  nstime_t endtime;
  uint32_t flags = 0;
  int rv;

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed2";

  starttime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");
  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  flags = MSF_UNPACKDATA;
  rv = ms3_readtracelist (&mstl, path, NULL, 0, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readtracelist() did not return expected MS_NOERROR");
  REQUIRE (mstl != NULL, "ms3_readtracelist() did not populate 'mstl'");
  CHECK (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");

  id = mstl->traces.next[0];

  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");
  REQUIRE (id->first != NULL, "id->first is not populated");
  CHECK_STREQ (id->sid, "FDSN:XX_TEST_00_L_H_Z");
  CHECK (id->earliest == starttime, "Earliest time is not expected '2010-02-27T06:50:00.069539Z'");
  CHECK (id->latest == endtime, "Latest time is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (id->pubversion == 1, "id->pubversion is not expected 1");
  CHECK (id->numsegments == 1, "id->numsegments is not expected 1");
  CHECK (id->first->starttime == starttime, "Segment start is not expected '2010-02-27T06:50:00.069539Z'");
  CHECK (id->first->endtime == endtime, "Segment start is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (id->first->samplecnt == 3952, "id->first->samplecnt is not expected 3952");
  CHECK (id->first->sampletype == 'i', "id->first->sampletype is not expected 'i'");
  CHECK (id->first->numsamples == 3952, "id->first->numsamples is not expected 3952");
  CHECK (id->next[0] == NULL, "id->next[0] is not expected NULL");
  CHECK (id->first->next == NULL, "id->first->next is not expected NULL");
  CHECK (id->first == id->last, "id->first is not equal to id->last as expected");

  mstl3_free (&mstl, 1);
}

TEST (read, recptr_file)
{
  MS3TraceList *mstl   = NULL;
  MS3TraceID *id       = NULL;
  MS3RecordPtr *recptr = NULL;
  nstime_t endtime;
  int64_t unpacked;
  uint32_t flags = 0;
  int32_t *int32s;
  int rv;

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed2";

  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  /* Set bit flag to build a record list */
  flags = MSF_RECORDLIST;

  rv = ms3_readtracelist (&mstl, path, NULL, 0, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readtracelist() did not return expected MS_NOERROR");
  REQUIRE (mstl != NULL, "ms3_readtracelist() did not populate 'mstl'");
  CHECK (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");

  id = mstl->traces.next[0];

  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");
  REQUIRE (id->first != NULL, "id->first is not populated");
  REQUIRE (id->first->recordlist != NULL, "id->first->recordlist is not populated");
  CHECK (id->first->samplecnt == 3952, "id->first->samplecnt is not expected 3952");

  /* No data has been decoded */
  CHECK (id->first->sampletype == 0, "id->first->sampletype is not expected 0");
  CHECK (id->first->datasamples == NULL, "id->first->datasamples is not expected NULL");
  CHECK (id->first->numsamples == 0, "id->first->numsamples is not expected 0");

  recptr = id->first->recordlist->last;
  CHECK (recptr->filename != NULL, "recptr->filename is unexpected NULL");     /* Record is in a file */
  CHECK (recptr->bufferptr == NULL, "recptr->bufferptr is not expected NULL"); /* Record is not in a buffer */
  CHECK (recptr->fileptr == NULL, "recptr->fileptr is not expected NULL");     /* File is not currently open, closed by read routine */
  CHECK (recptr->fileoffset == 1152, "recptr->fileoffset is not expected 1152");
  CHECK (recptr->msr != NULL, "recptr->msr is not expected NULL");
  CHECK (recptr->endtime == endtime, "recptr->endtime is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (recptr->dataoffset == 64, "recptr->dataoffset is not expected 64");
  CHECK (recptr->next == NULL, "recptr->next is not exected NULL");

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  CHECK (unpacked == id->first->samplecnt, "Return from mstl3_unpack_recordlist is not expected id->first->samplecnt");
  CHECK (id->first->sampletype == 'i', "id->first->sampletype is not expected 'i'");
  CHECK (id->first->datasamples != NULL, "id->first->datasamples is unexpected NULL");
  CHECK (id->first->numsamples == 3952, "id->first->numsamples is not expected 3952");

  int32s = (int32_t *)id->first->datasamples;
  CHECK (int32s[3948] == 28067, "Decoded sample value mismatch");
  CHECK (int32s[3949] == -9565, "Decoded sample value mismatch");
  CHECK (int32s[3950] == -71961, "Decoded sample value mismatch");
  CHECK (int32s[3951] == -146622, "Decoded sample value mismatch");

  mstl3_free (&mstl, 1);
}

TEST (read, recptr_buffer)
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

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed2";

  /* Read test data into buffer */
  fp = fopen (path, "rb");
  REQUIRE (fp != NULL, "File pointer is unexpected NULL");

  rv = fread (buffer, sizeof(buffer), 1, fp);
  REQUIRE (rv == 1, "fread() did not read entire file");

  fclose (fp);

  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  /* Set bit flag to build a record list */
  flags = MSF_RECORDLIST;

  rv = mstl3_readbuffer (&mstl, buffer, sizeof(buffer), 0, flags, 0, 0);

  CHECK (rv == 7, "mstl3_readbuffer did not return expected 7");
  CHECK (mstl != NULL, "mstl3_readbuffer did not populate 'mstl'");
  CHECK (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");

  id = mstl->traces.next[0];

  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");
  REQUIRE (id->first != NULL, "id->first is unexpected NULL");
  REQUIRE (id->first->recordlist, "id->first->recordlist is unexpected NULL");
  CHECK (id->first->samplecnt == 3952, "id->first->samplecnt is not expected 3952");

  /* No data has been decoded */
  CHECK (id->first->sampletype == 0, "id->first->sampletype is not expected 0");
  CHECK (id->first->datasamples == NULL, "id->first->datasamples is unexpected NULL");
  CHECK (id->first->numsamples == 0, "id->first->numsamples is not expected 0");

  recptr = id->first->recordlist->last;
  CHECK (recptr != NULL, "id->first->recordlist->last is unexpected NULL");
  CHECK (recptr->filename == NULL, "recptr->filename is not expected NULL"); /* Record is not in a file */
  CHECK (recptr->bufferptr != NULL, "recptr->bufferptr is unexpected NULL"); /* Record is in a buffer */
  CHECK (recptr->fileptr == NULL, "recptr->fileptr is not expected NULL");   /* File is not currently open, closed by read routine */

  CHECK (recptr->fileoffset == 0, "recptr->fileoffset is not expected 0");
  CHECK (recptr->msr != NULL, "recptr->msr is not expected NULL");
  CHECK (recptr->endtime == endtime, "recptr->endtime is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (recptr->dataoffset == 64, "recptr->dataoffset is not expected 64");
  CHECK (recptr->next == NULL, "recptr->next is not expected NULL");

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  CHECK (unpacked == id->first->samplecnt, "Return from mstl3_unpack_recordlist is not expected id->first->samplecnt");
  CHECK (id->first->sampletype == 'i', "id->first->sampletype is not expected 'i'");
  CHECK (id->first->datasamples != NULL, "id->first->datasamples is unexpected NULL");
  CHECK (id->first->numsamples == 3952, "id->first->numsamples is not expected 3952");

  int32s = (int32_t *)id->first->datasamples;
  CHECK (int32s[3948] == 28067, "Decoded sample value mismatch");
  CHECK (int32s[3949] == -9565, "Decoded sample value mismatch");
  CHECK (int32s[3950] == -71961, "Decoded sample value mismatch");
  CHECK (int32s[3951] == -146622, "Decoded sample value mismatch");

  mstl3_free (&mstl, 1);
}