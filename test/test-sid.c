#include <tau/tau.h>
#include <libmseed.h>

TEST (SID, sid2nslc)
{
  char net[11];
  char sta[11];
  char loc[11];
  char chan[34];
  int rv;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  rv = ms_sid2nslc ("FDSN:XX_TEST__L_H_Z", net, sta, loc, chan);
  CHECK (rv == 0, "ms_sid2nslc returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "");
  CHECK_STREQ (chan, "LHZ");

  rv = ms_sid2nslc ("FDSN:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  CHECK (rv == 0, "ms_sid2nslc returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "");
  CHECK_STREQ (chan, "BB_SS_ZZ");

  rv = ms_sid2nslc ("FDSN:EXTRANS:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  CHECK (rv == 0, "ms_sid2nslc returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "");
  CHECK_STREQ (chan, "BB_SS_ZZ");


  /* Error tests */
  rv = ms_sid2nslc ("XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc ("MYDC:XX_TEST__BB_SS_ZZ", net, sta, loc, chan);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc ("FDSN:YY_STA", net, sta, NULL, NULL);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc (NULL, NULL, NULL, NULL, NULL);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");
}

TEST (SID, nslc2sid) {
  uint16_t flags = 0;
  char sid[LM_SIDLEN];
  int rv;

  rv = ms_nslc2sid (sid, sizeof(sid), flags, "XX", "TEST", "", "LHZ");
  CHECK_STREQ (sid, "FDSN:XX_TEST__L_H_Z");
  CHECK (rv == 19, "ms_nslc2sid did not return expected length");

  rv = ms_nslc2sid (sid, sizeof(sid), flags, "XX", "TEST", "", "L_H_Z");
  CHECK_STREQ (sid, "FDSN:XX_TEST__L_H_Z");
  CHECK (rv == 19, "ms_nslc2sid did not return expected length");

  rv = ms_nslc2sid (sid, sizeof (sid), flags, "XX", "TEST", "00", "BB_SS_ZZ");
  CHECK_STREQ (sid, "FDSN:XX_TEST_00_BB_SS_ZZ");
  CHECK (rv == 24, "ms_nslc2sid did not return expected length");


  /* Error tests */
  rv = ms_nslc2sid (sid, 20, flags, "NETWORK", "STATION", "LOCATION", "CHA_NN_EL");
  CHECK (rv == -1, "ms_nslc2sid did not return expected -1");

  rv = ms_nslc2sid (NULL, 0, flags, NULL, NULL, NULL, NULL);
  CHECK (rv == -1, "ms_nslc2sid did not return expected -1");
}