#include <tau/tau.h>
#include <libmseed.h>

TEST (selection, match)
{
  MS3Selections *selections      = NULL;
  const MS3Selections *match     = NULL;
  const MS3SelectTime *timematch = NULL;
  nstime_t starttime;
  nstime_t endtime;
  int rv;

  starttime = ms_timestr2nstime ("2010-02-27T06:50:00.069539Z");
  endtime = ms_timestr2nstime ("2010-02-27T07:55:51.069539Z");

  rv = ms3_addselect (&selections, "FDSN:XX_*", NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == 0, "ms3_addselect() did not return expected 0");

  rv = ms3_addselect_comp (&selections, "YY", "STA1", "", "B_H_Z", NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == 0, "ms3_addselect_comp() did not return expected 0");

  rv = ms3_addselect_comp (&selections, "YY", "STA1", "", "LHZ", starttime, endtime, 2);
  REQUIRE (rv == 0, "ms3_addselect_comp() did not return expected 0");

  /* Matches */
  match = ms3_matchselect (selections, "FDSN:XX_S2__L_H_Z", NSTUNSET, NSTUNSET, 1, NULL);
  CHECK (match != NULL, "ms3_matchselect() did not return expected match");

  match = ms3_matchselect (selections, "FDSN:YY_STA1__B_H_Z", starttime, endtime, 2, &timematch);
  CHECK (match != NULL, "ms3_matchselect() did not return expected match");
  CHECK (timematch != NULL, "ms3_matchselect() did not return expected time match");

  match = ms3_matchselect (selections, "FDSN:YY_STA1__L_H_Z", starttime, endtime, 2, &timematch);
  CHECK (match != NULL, "ms3_matchselect() did not return expected match");
  CHECK (timematch != NULL, "ms3_matchselect() did not return expected time match");

  /* Non matches */
  match = ms3_matchselect (selections, "FDSN:YY_STA2__B_H_Z", starttime, endtime, 0, &timematch);
  CHECK (match == NULL, "ms3_matchselect() returned unexpected match");
  CHECK (timematch == NULL, "ms3_matchselect() returned unexpected time match");

  match = ms3_matchselect (selections, "FDSN:YY_STA1__L_H_Z", 0, 10, 0, &timematch);
  CHECK (match == NULL, "ms3_matchselect() returned unexpected match");
  CHECK (timematch == NULL, "ms3_matchselect() returned unexpected time match");

  match = ms3_matchselect (selections, "FDSN:YY_STA1__L_H_Z", starttime, endtime, 3, &timematch);
  CHECK (match == NULL, "ms3_matchselect() returned unexpected match");
  CHECK (timematch == NULL, "ms3_matchselect() returned unexpected time match");

  ms3_freeselections (selections);
}

TEST (selection, error)
{
  MS3Selections *selections  = NULL;
  const MS3Selections *match = NULL;
  int rv;

  rv = ms3_addselect (NULL, "FDSN:XX_*", NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == -1, "ms3_addselect() did not return expected -1");

  rv = ms3_addselect (&selections, NULL, NSTUNSET, NSTUNSET, 0);
  REQUIRE (rv == -1, "ms3_addselect() did not return expected -1");

  match = ms3_matchselect (NULL, "FDSN:YY_STA1__L_H_Z", NSTUNSET, NSTUNSET, 1, NULL);
  REQUIRE (match == NULL, "ms3_matchselect() did not return expected NULL");

  match = ms3_matchselect (selections, "FDSN:YY_STA1__L_H_Z", NSTUNSET, NSTUNSET, 1, NULL);
  REQUIRE (match == NULL, "ms3_matchselect() did not return expected NULL");
}