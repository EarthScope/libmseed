#include <tau/tau.h>
#include <libmseed.h>

TEST (SID, ms_sid2nslc_n)
{
  char net[11];
  char sta[11];
  char loc[11];
  char chan[34];
  int rv;

  /* Suppress error and warning messages by accumulating them */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);

  rv = ms_sid2nslc_n ("FDSN:XX_TEST__L_H_Z", net, sizeof (net), sta, sizeof (sta),
                      loc, sizeof (loc), chan, sizeof (chan));
  CHECK (rv == 0, "ms_sid2nslc_n returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "");
  CHECK_STREQ (chan, "LHZ");

  rv = ms_sid2nslc_n ("FDSN:XX_TEST_00_BB_SS_ZZ", net, sizeof (net), sta, sizeof (sta),
                      loc, sizeof (loc), chan, sizeof (chan));
  CHECK (rv == 0, "ms_sid2nslc_n returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "00");
  CHECK_STREQ (chan, "BB_SS_ZZ");

  rv = ms_sid2nslc_n ("FDSN:EXTRANS:XX_TEST__BB_SS_ZZ", net, sizeof (net), sta, sizeof (sta),
                      loc, sizeof (loc), chan, sizeof (chan));
  CHECK (rv == 0, "ms_sid2nslc_n returned unexpected error");
  CHECK_STREQ (net, "XX");
  CHECK_STREQ (sta, "TEST");
  CHECK_STREQ (loc, "");
  CHECK_STREQ (chan, "BB_SS_ZZ");

  /* Error tests */
  rv = ms_sid2nslc_n ("XX_TEST__BB_SS_ZZ", net, sizeof (net), sta, sizeof (sta),
                      loc, sizeof (loc), chan, sizeof (chan));
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc_n ("MYDC:XX_TEST__BB_SS_ZZ", net, sizeof (net), sta, sizeof (sta),
                      loc, sizeof (loc), chan, sizeof (chan));
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc_n ("FDSN:YY_STA", net, sizeof (net), sta, sizeof (sta),
                      NULL, 0, NULL, 0);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");

  rv = ms_sid2nslc_n (NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
  CHECK (rv == -1, "ms_sid2nslc did not return expected -1");
}

TEST (SID, ms_nslc2sid) {
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

  /* SEED codes with spaces */
  rv = ms_nslc2sid (sid, sizeof (sid), flags, " XX", "TE ST ", "  ", " S ");
  CHECK_STREQ (sid, "FDSN:XX_TEST___S_");
  CHECK (rv == 17, "ms_nslc2sid did not return expected length");

  /* Error tests */
  rv = ms_nslc2sid (sid, 20, flags, "NETWORK", "STATION", "LOCATION", "CHA_NN_EL");
  CHECK (rv == -1, "ms_nslc2sid did not return expected -1");

  rv = ms_nslc2sid (NULL, 0, flags, NULL, NULL, NULL, NULL);
  CHECK (rv == -1, "ms_nslc2sid did not return expected -1");
}

TEST (SID, ms_seedchan2xchan) {
  char xchan[12];
  int rv;

  rv = ms_seedchan2xchan (xchan, "LHZ");
  CHECK_STREQ (xchan, "L_H_Z");
  CHECK (rv == 0, "ms_seedchan2xchan did not return expected 0 for success");

  /* SEED channel with invalid spaces for band and orientation codes */
  rv = ms_seedchan2xchan (xchan, " H ");
  CHECK_STREQ (xchan, "_H_");
  CHECK (rv == 0, "ms_seedchan2xchan did not return expected 0 for success");

  /* Error tests */
  rv = ms_seedchan2xchan (xchan, "NOTAVALIDCHANNEL");
  CHECK (rv == -1, "ms_seedchan2xchan did not return expected -1");
}

TEST (SID, ms_xchan2seedchan) {
  char seedchan[12];
  int rv;

  rv = ms_xchan2seedchan (seedchan, "L_H_Z");
  CHECK_STREQ (seedchan, "LHZ");
  CHECK (rv == 0, "ms_seedchan2xchan did not return expected 0 for success");

  /* Unspecified band and subsource are legal Source ID but un-mappable to SEED */
  rv = ms_xchan2seedchan (seedchan, "_H_");
  CHECK (rv == -1, "ms_seedchan2xchan did not return expected -1 for un-mappable channel codes");

  /* Illegal spaces in the extended channel can trick the conversion to bad SEED codes
   * Do NOT do this in code, no guarantee that it will be maintained. */
  rv = ms_xchan2seedchan (seedchan, " _H_ ");
  CHECK_STREQ (seedchan, " H ");

  /* Error tests, cannot map to SEED codes */
  rv = ms_xchan2seedchan (seedchan, "BB_SS_SS");
  CHECK (rv == -1, "ms_seedchan2xchan did not return expected -1");
}