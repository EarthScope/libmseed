#include <libmseed.h>
#include <tau/tau.h>
#include <time.h>

/* This test reads a miniSEED file directly into a MS3TraceList and verifies the
 * contents of the trace list against expected values.
 *
 * The test data is a miniSEED file with one series of data with mixed lengths
 * and mixed time order.
 *
 * The test verifies basic functionality of reading data from files of miniSEED
 * into a trace list and that data added to the trace list are reconstructed as a
 * continuous time series, regardless of the order in which the data are added.
 */
TEST (tracelist, ms3_readtracelist_mixedlengths_mixedorder)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
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
  CHECK (id->first->starttime == starttime,
         "Segment start is not expected '2010-02-27T06:50:00.069539Z'");
  CHECK (id->first->endtime == endtime,
         "Segment start is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (id->first->samplecnt == 3952, "id->first->samplecnt is not expected 3952");
  CHECK (id->first->sampletype == 'i', "id->first->sampletype is not expected 'i'");
  CHECK (id->first->numsamples == 3952, "id->first->numsamples is not expected 3952");
  CHECK (id->next[0] == NULL, "id->next[0] is not expected NULL");
  CHECK (id->first->next == NULL, "id->first->next is not expected NULL");
  CHECK (id->first == id->last, "id->first is not equal to id->last as expected");

  mstl3_free (&mstl, 1);
}

/* This test reads a miniSEED file directly into a MS3TraceList while using the
 * MSF_RECORDLIST flag to build a record list for each trace segment.  The
 * expected contents of the record list are verified.
 */
TEST (tracelist, ms3_readtracelist_recptr)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
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
  CHECK (recptr->filename != NULL, "recptr->filename is unexpected NULL"); /* Record is in a file */
  CHECK (recptr->bufferptr == NULL,
         "recptr->bufferptr is not expected NULL"); /* Record is not in a buffer */
  CHECK (recptr->fileptr == NULL,
         "recptr->fileptr is not expected NULL"); /* File is not currently open, closed by read
                                                     routine */
  CHECK (recptr->fileoffset == 1152, "recptr->fileoffset is not expected 1152");
  CHECK (recptr->msr != NULL, "recptr->msr is not expected NULL");
  CHECK (recptr->msr->record == NULL,
         "recptr->msr->record is not expected NULL"); /* Record is not in a buffer */
  CHECK (recptr->endtime == endtime,
         "recptr->endtime is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (recptr->dataoffset == 64, "recptr->dataoffset is not expected 64");
  CHECK (recptr->next == NULL, "recptr->next is not exected NULL");

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  CHECK (unpacked == id->first->samplecnt,
         "Return from mstl3_unpack_recordlist is not expected id->first->samplecnt");
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

/* This test reads miniSEED from a buffer into a MS3TraceList while using the
 * MSF_RECORDLIST flag to build a record list for each trace segment.  The
 * expected contents of the record list are verified.
 */
TEST (tracelist, mstl3_readbuffer_recptr)
{
  char buffer[16256];
  FILE *fp = NULL;

  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
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

  rv = fread (buffer, sizeof (buffer), 1, fp);
  REQUIRE (rv == 1, "fread() did not read entire file");

  fclose (fp);

  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  /* Set bit flag to build a record list */
  flags = MSF_RECORDLIST;

  rv = mstl3_readbuffer (&mstl, buffer, sizeof (buffer), 0, flags, 0, 0);

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
  CHECK (recptr->filename == NULL,
         "recptr->filename is not expected NULL"); /* Record is not in a file */
  CHECK (recptr->bufferptr != NULL,
         "recptr->bufferptr is unexpected NULL"); /* Record is in a buffer */
  CHECK (recptr->fileptr == NULL,
         "recptr->fileptr is not expected NULL"); /* File is not currently open, closed by read
                                                     routine */

  CHECK (recptr->fileoffset == 0, "recptr->fileoffset is not expected 0");
  CHECK (recptr->msr != NULL, "recptr->msr is not expected NULL");
  CHECK (recptr->msr->record == recptr->bufferptr,
         "recptr->msr->record is not expected recptr->bufferptr");
  CHECK (recptr->endtime == endtime,
         "recptr->endtime is not expected '2010-02-27T07:55:51.069539Z'");
  CHECK (recptr->dataoffset == 64, "recptr->dataoffset is not expected 64");
  CHECK (recptr->next == NULL, "recptr->next is not expected NULL");

  /* Decode data */
  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  CHECK (unpacked == id->first->samplecnt,
         "Return from mstl3_unpack_recordlist is not expected id->first->samplecnt");
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

/* This test decodes a record list of miniSEED 2 records with a big endian
 * header and little endian INT32 payload, as declared in Blockette 1000.
 * On a little endian host only the header needs swapping, and the samples
 * decoded from the record list must match those unpacked while parsing.
 */
TEST (tracelist, mstl3_unpack_recordlist_mixedbyteorder)
{
  char buffer[2560];
  FILE *fp = NULL;

  MS3TraceList *mstl_unpacked = NULL;
  MS3TraceList *mstl_reclist = NULL;
  MS3TraceSeg *seg_unpacked = NULL;
  MS3TraceSeg *seg_reclist = NULL;
  uint16_t dataoffset;
  int64_t unpacked;
  int64_t rv;
  int offset;
  int idx;

  char *path = "data/reference-testdata-int32.mseed2";

  fp = fopen (path, "rb");
  REQUIRE (fp != NULL, "File pointer is unexpected NULL");
  rv = fread (buffer, sizeof (buffer), 1, fp);
  fclose (fp);
  REQUIRE (rv == 1, "fread() did not read entire file");

  /* Reference samples from the unmodified, big endian records */
  rv = mstl3_readbuffer (&mstl_unpacked, buffer, sizeof (buffer), 0, MSF_UNPACKDATA, NULL, 0);
  REQUIRE (rv == 5, "mstl3_readbuffer() did not return expected 5 records");
  seg_unpacked = mstl_unpacked->traces.next[0]->first;
  REQUIRE (seg_unpacked->numsamples > 0, "Reference segment has no samples");

  /* Convert each 512-byte record payload to little endian, declared in Blockette 1000 */
  for (offset = 0; offset < (int)sizeof (buffer); offset += 512)
  {
    dataoffset = (uint16_t)(((uint8_t)buffer[offset + 44] << 8) | (uint8_t)buffer[offset + 45]);
    REQUIRE (dataoffset == 56, "Data offset is not expected 56");
    REQUIRE ((uint8_t)buffer[offset + 48] == 0x03 && (uint8_t)buffer[offset + 49] == 0xE8,
             "Blockette 1000 is not at expected offset 48");

    buffer[offset + 53] = 0; /* Blockette 1000 byte order: little endian */

    for (idx = offset + dataoffset; idx + 4 <= offset + 512; idx += 4)
      ms_gswap4 (buffer + idx);
  }

  rv = mstl3_readbuffer (&mstl_reclist, buffer, sizeof (buffer), 0, MSF_RECORDLIST, NULL, 0);
  REQUIRE (rv == 5, "mstl3_readbuffer() did not return expected 5 records");
  seg_reclist = mstl_reclist->traces.next[0]->first;
  REQUIRE (seg_reclist->recordlist != NULL, "Record list is unexpected NULL");

  unpacked = mstl3_unpack_recordlist (mstl_reclist->traces.next[0], seg_reclist, NULL, 0, 0);

  CHECK (unpacked == seg_unpacked->numsamples, "Unpacked sample count does not match reference");
  REQUIRE (seg_reclist->datasamples != NULL, "Record list samples are unexpected NULL");
  CHECK (memcmp (seg_reclist->datasamples, seg_unpacked->datasamples,
                 (size_t)seg_unpacked->numsamples * sizeof (int32_t)) == 0,
         "Record list samples do not match samples unpacked while parsing");

  mstl3_free (&mstl_unpacked, 1);
  mstl3_free (&mstl_reclist, 1);
}

/* This test builds a record list from a miniSEED 2 record with samples but a
 * data offset of 0, i.e. no data payload.  Unpacking the record list must fail
 * instead of decoding the record header as data.
 */
TEST (tracelist, mstl3_unpack_recordlist_dataoffset0)
{
  char buffer[512];
  FILE *fp = NULL;

  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  int64_t unpacked;
  int64_t rv;

  char *path = "data/reference-testdata-steim2.mseed2";

  fp = fopen (path, "rb");
  REQUIRE (fp != NULL, "File pointer is unexpected NULL");
  rv = fread (buffer, sizeof (buffer), 1, fp);
  fclose (fp);
  REQUIRE (rv == 1, "fread() did not read first record");

  REQUIRE ((uint8_t)buffer[48] == 0x03 && (uint8_t)buffer[49] == 0xE8,
           "Blockette 1000 is not at expected offset 48");
  REQUIRE ((uint8_t)buffer[54] == 9, "Record length is not expected 512");

  /* Set the fixed header data offset to 0 */
  buffer[44] = 0;
  buffer[45] = 0;

  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  rv = mstl3_readbuffer (&mstl, buffer, sizeof (buffer), 0, MSF_RECORDLIST, NULL, 0);
  REQUIRE (rv == 1, "mstl3_readbuffer() did not return expected 1 record");

  id = mstl->traces.next[0];
  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");
  REQUIRE (id->first != NULL, "id->first is not populated");
  REQUIRE (id->first->recordlist != NULL, "Record list is unexpected NULL");
  REQUIRE (id->first->samplecnt > 0, "id->first->samplecnt is unexpected 0");
  REQUIRE (id->first->recordlist->first->dataoffset == 0,
           "Record pointer data offset is not expected 0");

  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);
  CHECK (unpacked == -1, "mstl3_unpack_recordlist() did not return expected -1");

  mstl3_free (&mstl, 1);

  /* Discard the expected error messages and restore printing */
  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);
}

/* This test adds a header-only record with extra headers to a MS3TraceList
 * via mstl3_addmsr_recordptr(), once with default flags and once with the
 * MSF_RECORDLIST_NOEXTRAS flag set, verifying that the extra headers are
 * copied to the resulting MS3RecordPtr by default and omitted when the flag
 * is set.
 */
TEST (tracelist, mstl3_addmsr_recordptr_noextras)
{
  MS3TraceList *mstl = NULL;
  MS3RecordPtr *recptr = NULL;
  MS3Record msr = MS3Record_INITIALIZER;
  char *extra = "{\"FDSN\":{\"Time\":{\"Quality\":100}}}";

  strcpy (msr.sid, "FDSN:XX_TEST__X_H_Z");
  msr.formatversion = 3;
  msr.pubversion = 1;
  msr.starttime = ms_timestr2nstime ("2024-01-01T00:00:00.0Z");
  msr.samprate = 1.0;
  msr.sampletype = 'i';
  msr.samplecnt = 100;
  msr.numsamples = 0;
  msr.datasamples = NULL;
  msr.extra = extra;
  msr.extralength = (uint16_t)strlen (extra);

  /* Default flags: extra headers are copied to the record list entry */
  REQUIRE (mstl = mstl3_init (NULL), "mstl3_init() returned unexpected NULL");
  REQUIRE (mstl3_addmsr_recordptr (mstl, &msr, &recptr, 0, 1, 0, NULL) != NULL,
           "mstl3_addmsr_recordptr() returned unexpected NULL");

  REQUIRE (recptr != NULL, "recptr is unexpected NULL");
  REQUIRE (recptr->msr != NULL, "recptr->msr is unexpected NULL");
  CHECK (recptr->msr->extralength == msr.extralength,
         "recptr->msr->extralength does not match source extralength");
  REQUIRE (recptr->msr->extra != NULL, "recptr->msr->extra is unexpected NULL");
  CHECK_STREQ (recptr->msr->extra, extra);

  mstl3_free (&mstl, 0);

  /* MSF_RECORDLIST_NOEXTRAS: extra headers are not copied */
  recptr = NULL;
  REQUIRE (mstl = mstl3_init (NULL), "mstl3_init() returned unexpected NULL");
  REQUIRE (mstl3_addmsr_recordptr (mstl, &msr, &recptr, 0, 1, MSF_RECORDLIST_NOEXTRAS, NULL) !=
               NULL,
           "mstl3_addmsr_recordptr() returned unexpected NULL");

  REQUIRE (recptr != NULL, "recptr is unexpected NULL");
  REQUIRE (recptr->msr != NULL, "recptr->msr is unexpected NULL");
  CHECK (recptr->msr->extralength == 0, "recptr->msr->extralength is not expected 0");
  CHECK (recptr->msr->extra == NULL, "recptr->msr->extra is not expected NULL");

  mstl3_free (&mstl, 0);
}

/* This test reads miniSEED from a file into a MS3TraceList while using the
 * MSF_PPUPDATETIME flag to set the segment prvtptr to the update time of the
 * record.  The expected value of the segment prvtptr is verified to be within
 * 10 seconds of the system time.
 */
TEST (tracelist, ms3_readtracelist_ppupdatetime)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  uint32_t flags;
  nstime_t difference;
  time_t timeval;
  int rv;

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed2";

  timeval = time (NULL);

  /* Set bit flag to set segment prvtptr to nstime_t value of update time */
  flags = MSF_PPUPDATETIME;

  rv = ms3_readtracelist (&mstl, path, NULL, 0, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readtracelist() did not return expected MS_NOERROR");
  REQUIRE (mstl != NULL, "ms3_readtracelist() did not populate 'mstl'");
  CHECK (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");

  id = mstl->traces.next[0];

  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");
  REQUIRE (id->first != NULL, "id->first is not populated");

  CHECK (id->first->prvtptr != NULL, "id->first->prvtptr is not populated");

  /* Check that update time is within 10 seconds of system time */
  difference = *(nstime_t *)id->first->prvtptr - (nstime_t)timeval * NSTMODULUS;

  CHECK (difference < (nstime_t)10 * (nstime_t)NSTMODULUS,
         "update time at id->first->prvtptr is not within 10 seconds of system time");

  mstl3_free (&mstl, 1);
}

/* This test reads miniSEED from a file into a MS3TraceList while using the
 * MSF_SPLITISVERSION flag to use the value of splitversion as the version
 * instead of the record publication version.  The expected value of the trace
 * ID's version is verified.
 */
TEST (tracelist, ms3_readtracelist_splitisversion)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  uint32_t flags;
  int rv;

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed3";

  /* Set bit flag to use the value of splitversion as the version
   * instead of the record publication version. */
  flags = MSF_SPLITISVERSION;

  rv = ms3_readtracelist (&mstl, path, NULL, 99, flags, 0);

  CHECK (rv == MS_NOERROR, "ms3_readtracelist() did not return expected MS_NOERROR");
  REQUIRE (mstl != NULL, "ms3_readtracelist() did not populate 'mstl'");
  CHECK (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");

  id = mstl->traces.next[0];

  REQUIRE (id != NULL, "mstl->traces.next[0] is not populated");

  CHECK (id->pubversion == 99, "id->pubversion is not expected 99");

  mstl3_free (&mstl, 1);
}

/* Build a trace list from two time-contiguous, header-only records for the same
 * source whose sample rates are specified as parameters and return the resulting number
 * of segments. */
static int
addmsr_two_rates (const MS3Tolerance *tolerance, double samprate1, double samprate2)
{
  MS3TraceList *mstl = NULL;
  MS3Record msr = MS3Record_INITIALIZER;
  nstime_t endtime1;
  int numsegments;

  if (!(mstl = mstl3_init (NULL)))
    return -1;

  strcpy (msr.sid, "FDSN:XX_TEST__X_H_Z");
  msr.formatversion = 3;
  msr.pubversion = 1;
  msr.sampletype = 'i';
  msr.samplecnt = 100;
  msr.numsamples = 0; /* Header-only, no decoded samples needed for merge logic */
  msr.datasamples = NULL;

  /* Record 1 at samprate1 */
  msr.starttime = ms_timestr2nstime ("2024-01-01T00:00:00.0Z");
  msr.samprate = samprate1;
  endtime1 = msr3_endtime (&msr);

  if (!mstl3_addmsr (mstl, &msr, 0, 1, 0, tolerance))
  {
    mstl3_free (&mstl, 0);
    return -1;
  }

  /* Record 2 at samprate2, starting exactly one (record 2) sample period after
   * record 1 ends, so the records are time-contiguous regardless of rate. */
  msr.samprate = samprate2;
  msr.starttime = endtime1 + msr3_nsperiod (&msr);

  if (!mstl3_addmsr (mstl, &msr, 0, 1, 0, tolerance))
  {
    mstl3_free (&mstl, 0);
    return -1;
  }

  numsegments = (mstl->traces.next[0]) ? (int)mstl->traces.next[0]->numsegments : -1;

  mstl3_free (&mstl, 0);
  return numsegments;
}

/* Verify sample rate tolerance handling in mstl3_addmsr() for default tolerance.  Two
 * time-contiguous records whose sample rates differ beyond the default
 * tolerance must remain separate segments when the default tolerance is used. */
TEST (tracelist, mstl3_addmsr_sampratetol_default)
{
  CHECK (addmsr_two_rates (NULL, 100.0, 99.5) == 2,
         "Differing sample rates with default tolerance did not yield 2 segments");
}

/* A sample rate tolerance callback for mstl3_addmsr() that considers any two
 * sample rates within 1.0 Hz of each other to be the same. */
static double
samprate_tol_generous (const MS3Record *msr)
{
  (void)msr;
  return 1.0;
}

/* Verify that supplying a custom (generous) sample rate tolerance causes the same
 * two records to be considered similar and merged into a single segment. */
TEST (tracelist, mstl3_addmsr_sampratetol_custom)
{
  MS3Tolerance tolerance = MS3Tolerance_INITIALIZER;
  tolerance.samprate = samprate_tol_generous;

  CHECK (addmsr_two_rates (&tolerance, 100.0, 99.5) == 1,
         "Differing sample rates with generous custom tolerance did not merge into 1 segment");
}

/* A sample rate tolerance callback for mstl3_addmsr() that requires an exact match. */
static double
samprate_tol_exact (const MS3Record *msr)
{
  (void)msr;
  return 0.0;
}

/* Verify that a sample rate tolerance callback returning 0.0 requires an exact
 * match, rather than falling back to the default relative tolerance.  Rates
 * that differ within the default tolerance (100.0 vs 100.005 Hz) must remain
 * separate segments when an exact match is requested. */
TEST (tracelist, mstl3_addmsr_sampratetol_exact)
{
  MS3Tolerance tolerance = MS3Tolerance_INITIALIZER;
  tolerance.samprate = samprate_tol_exact;

  CHECK (addmsr_two_rates (&tolerance, 100.0, 100.005) == 2,
         "Sample rates within default tolerance did not remain separate with an exact tolerance");
}

/* A sample rate tolerance callback for mstl3_addmsr() that returns an invalid
 * (negative) tolerance. */
static double
samprate_tol_negative (const MS3Record *msr)
{
  (void)msr;
  return -1.0;
}

/* Verify that a negative sample rate tolerance is ignored in favor of the
 * default tolerance, matching the behavior with no callback at all. */
TEST (tracelist, mstl3_addmsr_sampratetol_negative)
{
  MS3Tolerance tolerance = MS3Tolerance_INITIALIZER;
  tolerance.samprate = samprate_tol_negative;

  CHECK (addmsr_two_rates (&tolerance, 100.0, 100.005) == 1,
         "Negative sample rate tolerance did not fall back to the default tolerance");
  CHECK (addmsr_two_rates (&tolerance, 100.0, 99.5) == 2,
         "Negative sample rate tolerance did not fall back to the default tolerance");
}

/* Build a single-segment trace list containing the specified float samples and
 * convert it to 32-bit integers.  The conversion return value is stored at
 * 'result', the resulting sample type at 'sampletype', and the first converted
 * sample at 'firstsample'.
 *
 * Returns 0 on success, -1 if the trace list could not be constructed. */
static int
convert_float_samples (const float *samples, int64_t count, int8_t truncate, int *result,
                       char *sampletype, int32_t *firstsample)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;

  if (!(mstl = mstl3_init (NULL)))
    return -1;

  strcpy (msr.sid, "FDSN:XX_TEST__X_H_Z");
  msr.reclen = 512;
  msr.formatversion = 3;
  msr.pubversion = 1;
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2024-01-01T00:00:00.0Z");
  msr.sampletype = 'f';
  msr.datasamples = (void *)samples;
  msr.numsamples = count;
  msr.samplecnt = count;

  if (!mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) || !mstl->traces.next[0])
  {
    mstl3_free (&mstl, 0);
    return -1;
  }

  seg = mstl->traces.next[0]->first;

  *result = mstl3_convertsamples (seg, 'i', truncate);
  *sampletype = seg->sampletype;
  *firstsample = (seg->sampletype == 'i') ? ((int32_t *)seg->datasamples)[0] : 0;

  mstl3_free (&mstl, 0);
  return 0;
}

/* Verify that NaN samples are rejected by mstl3_convertsamples() rather than
 * being converted to a platform-dependent garbage integer.  Every comparison
 * against NaN is false, so the loss-of-precision test alone cannot detect it. */
TEST (tracelist, mstl3_convertsamples_nan)
{
  float samples[4] = {1.0f, 2.0f, NAN, 4.0f};
  char sampletype = 0;
  int32_t firstsample = 0;
  int result = 0;

  REQUIRE (convert_float_samples (samples, 4, 0, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for NaN conversion test");
  CHECK (result == -1, "mstl3_convertsamples() did not reject NaN samples with truncate unset");
  CHECK (sampletype == 'f', "Sample type changed after a rejected conversion");

  REQUIRE (convert_float_samples (samples, 4, 1, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for NaN conversion test");
  CHECK (result == -1, "mstl3_convertsamples() did not reject NaN samples with truncate set");
  CHECK (sampletype == 'f', "Sample type changed after a rejected conversion");
}

/* Verify that sample values outside the range of a 32-bit integer are rejected,
 * including when truncation of sub-integer precision is allowed. */
TEST (tracelist, mstl3_convertsamples_outofrange)
{
  float values[4] = {1.0e30f, -1.0e30f, INFINITY, -INFINITY};
  char sampletype = 0;
  int32_t firstsample = 0;
  int result = 0;

  for (int idx = 0; idx < 4; idx++)
  {
    float samples[2] = {1.0f, values[idx]};

    for (int8_t truncate = 0; truncate <= 1; truncate++)
    {
      REQUIRE (convert_float_samples (samples, 2, truncate, &result, &sampletype, &firstsample) ==
                   0,
               "Could not construct trace list for out-of-range conversion test");
      CHECK (result == -1, "mstl3_convertsamples() did not reject an out-of-range sample");
      CHECK (sampletype == 'f', "Sample type changed after a rejected conversion");
    }
  }
}

/* Verify that the range check is not off by one, the extreme representable
 * 32-bit integer values must still convert successfully. */
TEST (tracelist, mstl3_convertsamples_boundary)
{
  char sampletype = 0;
  int32_t firstsample = 0;
  int result = 0;

  /* INT32_MAX is not exactly representable as a float, use the nearest value
   * that is and rounds within range */
  float maxsample[1] = {2147483520.0f};
  float minsample[1] = {-2147483648.0f};

  REQUIRE (convert_float_samples (maxsample, 1, 0, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for boundary conversion test");
  CHECK (result == 0, "mstl3_convertsamples() rejected a representable maximum sample");
  CHECK (sampletype == 'i', "Sample type was not converted to integer");
  CHECK (firstsample == 2147483520, "Maximum sample did not convert to the expected value");

  REQUIRE (convert_float_samples (minsample, 1, 0, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for boundary conversion test");
  CHECK (result == 0, "mstl3_convertsamples() rejected a representable minimum sample");
  CHECK (sampletype == 'i', "Sample type was not converted to integer");
  CHECK (firstsample == INT32_MIN, "Minimum sample did not convert to the expected value");
}

/* Verify that ordinary samples still convert, that sub-integer precision is
 * rejected unless truncation is allowed, and that rounding is unchanged. */
TEST (tracelist, mstl3_convertsamples_valid)
{
  float integral[3] = {-2.0f, 0.0f, 3.0f};
  float fractional[1] = {1.4f};
  float negfractional[1] = {-1.4f};
  char sampletype = 0;
  int32_t firstsample = 0;
  int result = 0;

  REQUIRE (convert_float_samples (integral, 3, 0, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for valid conversion test");
  CHECK (result == 0, "mstl3_convertsamples() rejected integral float samples");
  CHECK (sampletype == 'i', "Sample type was not converted to integer");
  CHECK (firstsample == -2, "Integral sample did not convert to the expected value");

  REQUIRE (convert_float_samples (fractional, 1, 0, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for valid conversion test");
  CHECK (result == -1, "mstl3_convertsamples() did not detect loss of precision");

  REQUIRE (convert_float_samples (fractional, 1, 1, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for valid conversion test");
  CHECK (result == 0, "mstl3_convertsamples() rejected a truncated conversion");
  CHECK (firstsample == 1, "Fractional sample did not round as expected");

  REQUIRE (convert_float_samples (negfractional, 1, 1, &result, &sampletype, &firstsample) == 0,
           "Could not construct trace list for valid conversion test");
  CHECK (result == 0, "mstl3_convertsamples() rejected a truncated conversion");
  CHECK (firstsample == -1, "Negative fractional sample did not round as expected");
}

/* Verify that the double sample branch of mstl3_convertsamples() rejects NaN and
 * out-of-range values, and still converts representable values. */
TEST (tracelist, mstl3_convertsamples_double)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;
  double samples[3] = {1.0, 2.0, 3.0};

  strcpy (msr.sid, "FDSN:XX_TEST__X_H_Z");
  msr.reclen = 512;
  msr.formatversion = 3;
  msr.pubversion = 1;
  msr.samprate = 100.0;
  msr.starttime = ms_timestr2nstime ("2024-01-01T00:00:00.0Z");
  msr.sampletype = 'd';
  msr.datasamples = samples;
  msr.numsamples = 3;
  msr.samplecnt = 3;

  /* NaN must be rejected and leave the samples unconverted */
  samples[1] = NAN;
  REQUIRE ((mstl = mstl3_init (NULL)) != NULL, "mstl3_init() returned unexpected NULL");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");
  seg = mstl->traces.next[0]->first;
  CHECK (mstl3_convertsamples (seg, 'i', 1) == -1,
         "mstl3_convertsamples() did not reject a NaN double sample");
  CHECK (seg->sampletype == 'd', "Sample type changed after a rejected conversion");
  mstl3_free (&mstl, 0);

  /* Out-of-range must be rejected */
  samples[1] = 1.0e30;
  REQUIRE ((mstl = mstl3_init (NULL)) != NULL, "mstl3_init() returned unexpected NULL");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");
  seg = mstl->traces.next[0]->first;
  CHECK (mstl3_convertsamples (seg, 'i', 1) == -1,
         "mstl3_convertsamples() did not reject an out-of-range double sample");
  CHECK (seg->sampletype == 'd', "Sample type changed after a rejected conversion");
  mstl3_free (&mstl, 0);

  /* Representable values must still convert */
  samples[1] = -2.0;
  REQUIRE ((mstl = mstl3_init (NULL)) != NULL, "mstl3_init() returned unexpected NULL");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL,
           "mstl3_addmsr() returned unexpected NULL");
  seg = mstl->traces.next[0]->first;
  CHECK (mstl3_convertsamples (seg, 'i', 0) == 0,
         "mstl3_convertsamples() rejected integral double samples");
  CHECK (seg->sampletype == 'i', "Sample type was not converted to integer");
  CHECK (((int32_t *)seg->datasamples)[1] == -2, "Double sample did not convert as expected");
  mstl3_free (&mstl, 0);
}

/* Discard packed records */
static void
discard_record (char *record, int reclen, void *handlerdata)
{
  (void)record;
  (void)reclen;
  (void)handlerdata;
}

/* Add a 1 Hz integer series of 'count' samples starting at 'second' */
static MS3TraceSeg *
add_series (MS3TraceList *mstl, int64_t second, int64_t count)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceSeg *seg;
  int32_t *samples;
  int64_t idx;

  if (!(samples = (int32_t *)malloc ((size_t)count * sizeof (int32_t))))
    return NULL;

  for (idx = 0; idx < count; idx++)
    samples[idx] = (int32_t)idx;

  strcpy (msr.sid, "FDSN:XX_SORT__B_H_Z");
  msr.starttime = MS_EPOCH2NSTIME (1700000000 + second);
  msr.samprate = 1.0;
  msr.samplecnt = count;
  msr.numsamples = count;
  msr.sampletype = 'i';
  msr.datasamples = samples;

  seg = mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL);

  free (samples);

  return seg;
}

/* Test that a trace ID's segments are in start time order */
static int
segments_in_order (const MS3TraceList *mstl)
{
  const MS3TraceSeg *seg = mstl->traces.next[0]->first;

  for (; seg && seg->next; seg = seg->next)
    if (seg->starttime > seg->next->starttime)
      return 0;

  return 1;
}

/* Build overlapping segments [0,99] and [50,60] and a later [200,299] */
static MS3TraceList *
overlapping_segments (void)
{
  MS3TraceList *mstl = mstl3_init (NULL);

  if (!mstl || !add_series (mstl, 0, 100) || !add_series (mstl, 50, 11) ||
      !add_series (mstl, 200, 100))
    mstl3_free (&mstl, 1);

  return mstl;
}

/* Packing full records from the front of a segment advances its start time,
 * which must keep the segment list in start time order when segments overlap.
 * A record contiguous with a segment that ends up later in the list must
 * still be merged into it. */
TEST (tracelist, mstl3_pack_keeps_segment_order)
{
  MS3TraceListPacker *packer = NULL;
  MS3TraceList *mstl = NULL;
  int64_t packedsamples = 0;
  int64_t packedrecords;
  char *record = NULL;
  int32_t reclen = 0;
  int rv;

  /* Callback interface */
  mstl = overlapping_segments ();
  REQUIRE (mstl != NULL, "Cannot build trace list");
  REQUIRE (mstl->traces.next[0]->numsegments == 3, "Expected 3 segments before packing");

  packedrecords =
      mstl3_pack (mstl, discard_record, NULL, 256, DE_INT32, &packedsamples, 0, 0, NULL);
  CHECK (packedrecords > 0, "mstl3_pack() did not pack any records");
  CHECK (mstl->traces.next[0]->numsegments == 3, "Expected 3 segments after packing");
  CHECK (segments_in_order (mstl), "Segments not in start time order after mstl3_pack()");

  REQUIRE (add_series (mstl, 61, 5) != NULL, "Cannot add series contiguous with [50,60]");
  CHECK (mstl->traces.next[0]->numsegments == 3,
         "Contiguous series was not merged after mstl3_pack()");

  mstl3_free (&mstl, 1);

  /* Generator interface */
  mstl = overlapping_segments ();
  REQUIRE (mstl != NULL, "Cannot build trace list");

  packer = mstl3_pack_init (mstl, 256, DE_INT32, 0, 0, NULL, 0);
  REQUIRE (packer != NULL, "mstl3_pack_init() failed");

  packedrecords = 0;
  while ((rv = mstl3_pack_next (packer, 0, &record, &reclen)) == 1)
    packedrecords++;

  CHECK (rv == 0, "mstl3_pack_next() did not return expected 0");
  CHECK (packedrecords > 0, "mstl3_pack_next() did not pack any records");
  mstl3_pack_free (&packer, NULL);

  CHECK (mstl->traces.next[0]->numsegments == 3, "Expected 3 segments after packing");
  CHECK (segments_in_order (mstl), "Segments not in start time order after mstl3_pack_next()");

  REQUIRE (add_series (mstl, 61, 5) != NULL, "Cannot add series contiguous with [50,60]");
  CHECK (mstl->traces.next[0]->numsegments == 3,
         "Contiguous series was not merged after mstl3_pack_next()");

  mstl3_free (&mstl, 1);
}

/* Log output captured while synclist_capture is set, otherwise printed */
static int synclist_capture = 0;
static char synclist_output[2048];

static void
synclist_log_print (const char *message)
{
  if (synclist_capture)
    strncat (synclist_output, message, sizeof (synclist_output) - strlen (synclist_output) - 1);
  else
    fputs (message, stdout);
}

static void
synclist_diag_print (const char *message)
{
  if (!synclist_capture)
    fputs (message, stderr);
}

/* A source ID that cannot be split into codes is printed with empty codes,
 * not the codes of the previously printed trace ID */
TEST (tracelist, mstl3_printsynclist_unparsable_sid)
{
  MS3Record msr = MS3Record_INITIALIZER;
  MS3TraceList *mstl = NULL;
  const char *cp;
  int count = 0;

  mstl = mstl3_init (NULL);
  REQUIRE (mstl != NULL, "mstl3_init() failed");

  msr.starttime = MS_EPOCH2NSTIME (1700000000);
  msr.samprate = 1.0;

  strcpy (msr.sid, "FDSN:XX_AAA_00_B_H_Z");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL, "mstl3_addmsr() failed");

  strcpy (msr.sid, "XX_BBB");
  REQUIRE (mstl3_addmsr (mstl, &msr, 0, 1, 0, NULL) != NULL, "mstl3_addmsr() failed");

  synclist_output[0] = '\0';
  ms_loginit (synclist_log_print, "", synclist_diag_print, NULL);

  synclist_capture = 1;
  mstl3_printsynclist (mstl, NULL, NONE);
  synclist_capture = 0;

  for (cp = synclist_output; (cp = strstr (cp, "XX|AAA|00|BHZ|")); cp++)
    count++;

  CHECK (count == 1, "Codes of one trace ID were printed for another");
  CHECK (strstr (synclist_output, "\n||||") != NULL,
         "Unparsable source ID was not printed with empty codes");

  mstl3_free (&mstl, 1);
}

/* A record list read from a path with a byte range suffix refers to the
 * file by that path; unpacking must open the file without the suffix. */
TEST (tracelist, ms3_readtracelist_recptr_pnamerange)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  int64_t unpacked;
  int rv;

  char *path = "data/testdata-oneseries-mixedlengths-mixedorder.mseed2@0-16255";

  rv = ms3_readtracelist (&mstl, path, NULL, 0, MSF_RECORDLIST | MSF_PNAMERANGE, 0);
  REQUIRE (rv == MS_NOERROR, "ms3_readtracelist() did not return expected MS_NOERROR");

  id = mstl->traces.next[0];
  REQUIRE (id != NULL && id->first != NULL, "Trace list is not populated");
  REQUIRE (id->first->recordlist != NULL, "id->first->recordlist is not populated");
  CHECK (id->first->samplecnt == 3952, "id->first->samplecnt is not expected 3952");

  unpacked = mstl3_unpack_recordlist (id, id->first, NULL, 0, 0);

  CHECK (unpacked == 3952, "mstl3_unpack_recordlist() did not unpack expected 3952 samples");
  CHECK (id->first->numsamples == 3952, "id->first->numsamples is not expected 3952");

  mstl3_free (&mstl, 1);
}

/* A trace list allocated by the caller, rather than by mstl3_init(), can be
 * populated and freed by the library */
TEST (tracelist, mstl3_addmsr_caller_allocated_list)
{
  MS3TraceList *mstl = NULL;
  MS3TraceSeg *seg = NULL;

  mstl = (MS3TraceList *)calloc (1, sizeof (MS3TraceList));
  REQUIRE (mstl != NULL, "Cannot allocate trace list");

  /* Out of order series, filling the gaps last */
  REQUIRE (add_series (mstl, 0, 10) != NULL, "Cannot add series at 0");
  REQUIRE (add_series (mstl, 20, 10) != NULL, "Cannot add series at 20");
  REQUIRE (add_series (mstl, 40, 10) != NULL, "Cannot add series at 40");
  REQUIRE (add_series (mstl, 30, 10) != NULL, "Cannot add series at 30");
  REQUIRE (add_series (mstl, 10, 10) != NULL, "Cannot add series at 10");

  REQUIRE (mstl->numtraceids == 1, "mstl->numtraceids is not expected 1");
  CHECK (mstl->traces.next[0]->numsegments == 1, "Series were not merged into one segment");

  seg = mstl->traces.next[0]->first;
  CHECK (seg->samplecnt == 50, "Segment sample count is not expected 50");
  CHECK (seg->starttime == MS_EPOCH2NSTIME (1700000000), "Segment start is not expected");

  mstl3_free (&mstl, 1);
}

/* A segment unlinked and freed by the caller, keeping the list consistent,
 * must not be used by later additions to the trace list */
TEST (tracelist, mstl3_addmsr_after_caller_removes_segment)
{
  MS3TraceList *mstl = NULL;
  MS3TraceID *id = NULL;
  MS3TraceSeg *removed = NULL;

  mstl = mstl3_init (NULL);
  REQUIRE (mstl != NULL, "mstl3_init() failed");

  REQUIRE (add_series (mstl, 0, 10) != NULL, "Cannot add series at 0");
  REQUIRE (add_series (mstl, 100, 10) != NULL, "Cannot add series at 100");
  REQUIRE (add_series (mstl, 200, 10) != NULL, "Cannot add series at 200");

  id = mstl->traces.next[0];
  REQUIRE (id->numsegments == 3, "Expected 3 segments");

  /* Remove the last segment as a caller would */
  removed = id->last;
  id->last = removed->prev;
  id->last->next = NULL;
  id->numsegments -= 1;
  free (removed->datasamples);
  free (removed);

  /* Contiguous with the end of the first segment, found by searching the list */
  REQUIRE (add_series (mstl, 10, 5) != NULL, "Cannot add series at 10");

  CHECK (id->numsegments == 2, "Series was not merged into the first segment");
  CHECK (id->first->samplecnt == 15, "First segment sample count is not expected 15");
  CHECK (id->last->samplecnt == 10, "Last segment sample count is not expected 10");

  mstl3_free (&mstl, 1);
}

/* Non-miniSEED data in a buffer is skipped on request, resulting in the same
 * trace list as the records alone, and is otherwise an error. */
TEST (tracelist, mstl3_readbuffer_skipnotdata)
{
  MS3TraceList *mstl = NULL;
  MS3TraceList *mstl_skip = NULL;
  MS3TraceID *id = NULL;
  MS3TraceID *id_skip = NULL;
  MS3Record *msr = NULL;
  char *source;
  char *buffer;
  const uint64_t sourcelen = 56522;
  const uint64_t junklen = 800;
  uint64_t buflen = sourcelen + 3 * junklen;
  int32_t firstreclen;
  int64_t expected;
  int64_t rv;
  FILE *fp;

  char *path = "data/testdata-3channel-signal.mseed3";

  source = (char *)malloc (sourcelen);
  buffer = (char *)malloc (buflen);
  REQUIRE (source != NULL && buffer != NULL, "Cannot allocate buffers");

  fp = fopen (path, "rb");
  REQUIRE (fp != NULL, "File pointer is unexpected NULL");
  rv = fread (source, sourcelen, 1, fp);
  fclose (fp);
  REQUIRE (rv == 1, "fread() did not read entire file");

  REQUIRE (msr3_parse (source, sourcelen, &msr, 0, 0) == 0, "Cannot parse first record");
  firstreclen = msr->reclen;
  msr3_free (&msr);

  expected = mstl3_readbuffer (&mstl, source, sourcelen, 0, 0, NULL, 0);
  REQUIRE (expected > 2, "mstl3_readbuffer() did not return more than 2 records");

  /* Non-data, first record, non-data, remaining records, non-data */
  memset (buffer, 'X', buflen);
  memcpy (buffer + junklen, source, firstreclen);
  memcpy (buffer + 2 * junklen + firstreclen, source + firstreclen, sourcelen - firstreclen);

  rv = mstl3_readbuffer (&mstl_skip, buffer, buflen, 0, 0, NULL, 0);
  CHECK (rv == MS_NOTSEED, "mstl3_readbuffer() did not return expected MS_NOTSEED");
  mstl3_free (&mstl_skip, 1);

  rv = mstl3_readbuffer (&mstl_skip, buffer, buflen, 0, MSF_SKIPNOTDATA | MSF_RECORDLIST, NULL, 0);
  CHECK (rv == expected, "mstl3_readbuffer() did not return all records when skipping non-data");
  REQUIRE (mstl_skip != NULL, "mstl3_readbuffer() did not populate 'mstl'");
  CHECK (mstl_skip->numtraceids == mstl->numtraceids, "Trace ID count is not as expected");

  id = mstl->traces.next[0];
  id_skip = mstl_skip->traces.next[0];

  while (id && id_skip)
  {
    CHECK_STREQ (id_skip->sid, id->sid);
    CHECK (id_skip->earliest == id->earliest, "Trace ID earliest time is not as expected");
    CHECK (id_skip->latest == id->latest, "Trace ID latest time is not as expected");
    CHECK (id_skip->numsegments == id->numsegments, "Segment count is not as expected");
    REQUIRE (id_skip->first != NULL, "id->first is not populated");
    CHECK (id_skip->first->samplecnt == id->first->samplecnt, "Sample count is not as expected");

    id = id->next[0];
    id_skip = id_skip->next[0];
  }

  CHECK (id == NULL && id_skip == NULL, "Trace lists do not have the same trace IDs");

  /* The first record follows the leading non-data */
  id_skip = mstl_skip->traces.next[0];
  REQUIRE (id_skip != NULL && id_skip->first != NULL, "First trace ID is not populated");
  REQUIRE (id_skip->first->recordlist != NULL, "Record list is unexpected NULL");
  CHECK (id_skip->first->recordlist->first->bufferptr == buffer + junklen,
         "First record pointer is not at expected offset");

  mstl3_free (&mstl, 1);
  mstl3_free (&mstl_skip, 1);
  free (source);
  free (buffer);
}

/* A miniSEED 2 header with no determinable record length, followed by more
 * than the maximum record length of non-data, is a record length out of
 * range and not the end of the buffer. */
TEST (tracelist, mstl3_readbuffer_maxreclen)
{
  MS3TraceList *mstl = NULL;
  char *buffer;
  uint64_t buflen = MAXRECLEN + 1024;
  int64_t rv;

  buffer = (char *)calloc (1, buflen);
  REQUIRE (buffer != NULL, "Cannot allocate buffer");

  /* Detected as a miniSEED 2 header with no blockettes */
  memcpy (buffer, "000001D ", 8);

  /* Suppress expected error messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  rv = mstl3_readbuffer (&mstl, buffer, buflen, 0, 0, NULL, 0);
  CHECK (rv == MS_OUTOFRANGE, "mstl3_readbuffer() did not return expected MS_OUTOFRANGE");
  mstl3_free (&mstl, 1);

  rv = mstl3_readbuffer (&mstl, buffer, buflen, 0, MSF_SKIPNOTDATA, NULL, 0);
  CHECK (rv == 0, "mstl3_readbuffer() did not return expected 0 records");

  ms_rlog_free (NULL);
  ms_rloginit (NULL, NULL, NULL, NULL, 0);

  mstl3_free (&mstl, 1);
  free (buffer);
}
