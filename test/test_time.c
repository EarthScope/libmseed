#include "munit.h"
#include "libmseed.h"

static MunitResult
stringtests (const MunitParameter params[], void *data)
{
  char timestr[50];
  nstime_t nstime;

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  /* General parsing test to nstime_t */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788Z");
  munit_assert_int64 (nstime, ==, 1084345689123456788);

  /* Format variations */
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "2004-05-12T07:08:09.123456788");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "2004-05-12T07:08:09.123456788Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_DOY, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "2004-05-12T07:08:09.123456788 (133)");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_DOY_Z, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "2004-05-12T07:08:09.123456788Z (133)");

  ms_nstime2timestr (nstime, timestr, SEEDORDINAL, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "2004,133,07:08:09.123456788");

  ms_nstime2timestr (nstime, timestr, UNIXEPOCH, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "1084345689.123456788");

  ms_nstime2timestr (nstime, timestr, NANOSECONDEPOCH, NANO_MICRO_NONE);
  munit_assert_string_equal(timestr, "1084345689123456788");

  /* Subsecond variations */

  /* Nano subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.123456788Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.123456Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NONE);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09Z");

  /* Micro subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.1234Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.123400Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.123400Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO_NONE);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.123400Z");

  /* No subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09.000000Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO_NONE);
  munit_assert_string_equal (timestr, "2004-05-12T07:08:09Z");

  /* Time string variations */

  nstime = ms_timestr2nstime ("2004");
  munit_assert_int64 (nstime, ==, 1072915200000000000);

  nstime = ms_timestr2nstime ("2004-2-9");
  munit_assert_int64 (nstime, ==, 1076284800000000000);

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.12345Z");
  munit_assert_int64 (nstime, ==, 1084345689123450000);

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.12345");
  munit_assert_int64 (nstime, ==, 1084345689123450000);

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788");
  munit_assert_int64 (nstime, ==, 1084345689123456788);

  nstime = ms_timestr2nstime ("1084345689.123456788");
  munit_assert_int64 (nstime, ==, 1084345689123456788);

  nstime = ms_timestr2nstime ("1969,201,20,17,40.98");
  munit_assert_int64 (nstime, ==, -14182939020000000);

  nstime = ms_timestr2nstime ("1969-201T20:17:40.987654321");
  munit_assert_int64 (nstime, ==, -14182939012345679);

  nstime = ms_timestr2nstime ("-14182939.012345679");
  munit_assert_int64 (nstime, ==, -14182939012345679);

  /* Parsing error tests */
  nstime = ms_timestr2nstime ("this is not a time string");
  munit_assert_int64 (nstime, ==, NSTERROR);

  nstime = ms_timestr2nstime ("0000-00-00");
  munit_assert_int64 (nstime, ==, NSTERROR);

  nstime = ms_timestr2nstime ("5000-00-00");
  munit_assert_int64 (nstime, ==, NSTERROR);

  nstime = ms_timestr2nstime ("20040512T000000");
  munit_assert_int64 (nstime, ==, NSTERROR);

  return MUNIT_OK;
}

/* Create test array */
MunitTest time_tests[] = {
  { (char*) "/timestring", stringtests, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};