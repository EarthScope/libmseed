#include "munit.h"
#include "libmseed.h"


static MunitResult
sid2nslc(const MunitParameter params[], void* data) {
  char net[11];
  char sta[11];
  char loc[11];
  char chan[34];
  int rv;

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  rv = ms_sid2nslc ("FDSN:XX_TEST__L_H_Z", net, sta, loc, chan);
  munit_assert_int (rv, ==, 0);
  munit_assert_string_equal (net, "XX");
  munit_assert_string_equal (sta, "TEST");
  munit_assert_string_equal (loc, "");
  munit_assert_string_equal (chan, "LHZ");

  rv = ms_sid2nslc ("FDSN:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  munit_assert_int (rv, ==, 0);
  munit_assert_string_equal (net, "XX");
  munit_assert_string_equal (sta, "TEST");
  munit_assert_string_equal (loc, "");
  munit_assert_string_equal (chan, "BB_SS_ZZ");

  rv = ms_sid2nslc ("FDSN:EXTRANS:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  munit_assert_int (rv, ==, 0);
  munit_assert_string_equal (net, "XX");
  munit_assert_string_equal (sta, "TEST");
  munit_assert_string_equal (loc, "");
  munit_assert_string_equal (chan, "BB_SS_ZZ");


  /* Error tests */
  rv = ms_sid2nslc ("XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  munit_assert_int (rv, ==, -1);

  rv = ms_sid2nslc ("MYDC:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  munit_assert_int (rv, ==, -1);

  rv = ms_sid2nslc ("FDSN:YY_STA", net, sta, NULL, NULL);
  munit_assert_int (rv, ==, -1);

  rv = ms_sid2nslc (NULL, NULL, NULL, NULL, NULL);
  munit_assert_int (rv, ==, -1);

  return MUNIT_OK;
}

static MunitResult
nslc2sid(const MunitParameter params[], void* data) {
  uint16_t flags = 0;
  char sid[LM_SIDLEN];
  int rv;

  /* Silence compiler warnings about unused parameters */
  (void) params;
  (void) data;

  rv = ms_nslc2sid (sid, sizeof(sid), flags, "XX", "TEST", "", "LHZ");
  munit_assert_string_equal (sid, "FDSN:XX_TEST__L_H_Z");
  munit_assert_int (rv, ==, 19);

  rv = ms_nslc2sid (sid, sizeof(sid), flags, "XX", "TEST", "", "L_H_Z");
  munit_assert_string_equal (sid, "FDSN:XX_TEST__L_H_Z");
  munit_assert_int (rv, ==, 19);

  rv = ms_nslc2sid (sid, sizeof (sid), flags, "XX", "TEST", "00", "BB_SS_ZZ");
  munit_assert_string_equal (sid, "FDSN:XX_TEST_00_BB_SS_ZZ");
  munit_assert_int (rv, ==, 24);


  /* Error tests */
  rv = ms_nslc2sid (sid, 20, flags, "NETWORK", "STATION", "LOCATION", "CHA_NN_EL");
  munit_assert_int (rv, ==, -1);

  rv = ms_nslc2sid (NULL, 0, flags, NULL, NULL, NULL, NULL);
  munit_assert_int (rv, ==, -1);

  return MUNIT_OK;
}

/* Create test array */
MunitTest sid_tests[] = {
  { (char*) "/sid2nslc", sid2nslc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/nslc2sid", nslc2sid, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};