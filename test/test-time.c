#include <tau/tau.h>
#include <libmseed.h>

TEST (time, nstime)
{
  char timestr[50];
  nstime_t nstime;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  /* General parsing test to nstime_t */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788Z");
  CHECK (nstime == 1084345689123456788, "Failed to convert time string: '2004-05-12T7:8:9.123456788Z'");

  /* Format variations */
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456788");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456788Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_DOY, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456788 (133)");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_DOY_Z, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456788Z (133)");

  ms_nstime2timestr (nstime, timestr, SEEDORDINAL, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004,133,07:08:09.123456788");

  ms_nstime2timestr (nstime, timestr, UNIXEPOCH, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "1084345689.123456788");

  ms_nstime2timestr (nstime, timestr, NANOSECONDEPOCH, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "1084345689123456788");

  /* Subsecond variations */

  /* Nano subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456788Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123456Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09Z");

  /* Micro subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.1234Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123400Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123400Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.123400Z");

  /* No subseconds */
  nstime = ms_timestr2nstime ("2004-05-12T7:8:9Z");
  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, NANO_MICRO);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09.000000Z");

  ms_nstime2timestr (nstime, timestr, ISOMONTHDAY_Z, MICRO_NONE);
  CHECK_STREQ (timestr, "2004-05-12T07:08:09Z");

  /* Time string variations */

  nstime = ms_timestr2nstime ("2004");
  CHECK (nstime == 1072915200000000000, "Failed to convert time string: '2004'");

  nstime = ms_timestr2nstime ("2004-2-9");
  CHECK (nstime == 1076284800000000000, "Failed to convert time string: '2004-2-9'");

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.12345Z");
  CHECK (nstime == 1084345689123450000, "Failed to convert time string: '2004-05-12T7:8:9.12345Z'");

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.12345");
  CHECK (nstime == 1084345689123450000, "Failed to convert time string: '2004-05-12T7:8:9.12345'");

  nstime = ms_timestr2nstime ("2004-05-12T7:8:9.123456788");
  CHECK (nstime == 1084345689123456788, "Failed to convert time string: '2004-05-12T7:8:9.123456788'");

  nstime = ms_timestr2nstime ("1084345689.123456788");
  CHECK (nstime == 1084345689123456788, "Failed to convert time string: '1084345689.123456788'");

  nstime = ms_timestr2nstime ("1969,201,20,17,40.98");
  CHECK (nstime == -14182939020000000, "Failed to convert time string: '1969,201,20,17,40.98'");

  nstime = ms_timestr2nstime ("1969-201T20:17:40.987654321");
  CHECK (nstime == -14182939012345679, "Failed to convert time string: '1969-201T20:17:40.987654321'");

  nstime = ms_timestr2nstime ("-14182939.012345679");
  CHECK (nstime == -14182939012345679, "Failed to convert time string: '-14182939.012345679'");

  /* Parsing error tests */
  nstime = ms_timestr2nstime ("this is not a time string");
  CHECK (nstime == NSTERROR, "Failed to produce error for time string: 'this is not a time string'");

  nstime = ms_timestr2nstime ("0000-00-00");
  CHECK (nstime == NSTERROR, "Failed to produce error for time string: '0000-00-00'");

  nstime = ms_timestr2nstime ("5000-00-00");
  CHECK (nstime == NSTERROR, "Failed to produce error for time string: '5000-00-00'");

  nstime = ms_timestr2nstime ("20040512T000000");
  CHECK (nstime == NSTERROR, "Failed to produce error for time string: '20040512T000000'");
}