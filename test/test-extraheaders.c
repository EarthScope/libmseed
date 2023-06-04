#include <tau/tau.h>
#include <libmseed.h>

char *testheaders = "{ \
  \"FDSN\": {\
    \"Time\": {\
      \"Quality\": 100,\
      \"Correction\": 1.234\
    },\
    \"Event\": { \
      \"Begin\": true,\
      \"End\": true,\
      \"InProgress\": true,\
      \"Detection\": [\
        {\
          \"Type\": \"MURDOCK\",\
          \"SignalAmplitude\": 80,\
          \"SignalPeriod\": 0.4,\
          \"BackgroundEstimate\": 18,\
          \"Wave\": \"DILATATION\",\
          \"Units\": \"COUNTS\",\
          \"OnsetTime\": \"2022-06-05T20:32:39.120000Z\",\
          \"MEDSNR\": [ 1, 3, 2, 1, 4, 0 ],\
          \"MEDLookback\": 2,\
          \"MEDPickAlgorithm\": 0,\
          \"Detector\": \"Z_SPWWSS\"\
        }\
      ]\
    }\
  }\
}";

TEST (extraheaders, get_set)
{
  MS3Record *msr = NULL;
  int64_t ivalue;
  double dvalue;
  char svalue[100];
  int bvalue;
  int rv;

  char *newstring = "Value";

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  msr->extra = malloc (strlen (testheaders));
  REQUIRE (msr->extra != NULL, "Error allocating memory for msr->extra");
  memcpy (msr->extra, testheaders, strlen (testheaders));
  msr->extralength = strlen (testheaders);

  /* Matching */
  rv = mseh_exists (msr, "/FDSN/Time/Quality");
  CHECK (rv, "mseh_exists() returned unexpected false");

  rv = mseh_exists (msr, "/FDSN/Event/Detection/0");
  CHECK (rv, "mseh_exists() returned unexpected false for array element");

  rv = mseh_get_number (msr, "/FDSN/Time/Correction", &dvalue);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (dvalue == 1.234, "/FDSN/Time/Correction is not expected 1.234");

  rv = mseh_get_int64 (msr, "/FDSN/Time/Quality", &ivalue);
  CHECK (rv == 0, "mseh_get_int64() returned unexpected non-match");
  CHECK (ivalue == 100, "/FDSN/Time/Quality is not expected 100");

  rv = mseh_get_string (msr, "/FDSN/Event/Detection/0/Type", svalue, sizeof(svalue));
  CHECK (rv == 0, "mseh_get_string() returned unexpected non-match");
  CHECK_STREQ (svalue, "MURDOCK");

  rv = mseh_get_boolean (msr, "/FDSN/Event/Begin", &bvalue);
  CHECK (rv == 0, "mseh_get_boolean() returned unexpected non-match");
  CHECK (bvalue, "/FDSN/Event/Begin is not expected True");

  /* Non matching */
  rv = mseh_exists (msr, "/FDSN/Event/Detection/1");
  CHECK (!rv, "mseh_exists() returned unexpected true for array element");

  rv = mseh_get_int64 (msr, "/A/Non/Existant/Header", &ivalue);
  CHECK (rv != 0, "mseh_get_int64() returned unexpected match");

  /* Set */
  rv = mseh_set_string (msr, "/New/Header", newstring);
  CHECK (rv == 0, "mseh_set() returned unexpected error");

  rv = mseh_get_string (msr, "/New/Header", svalue, sizeof(svalue));
  CHECK (rv == 0, "mseh_get_string() returned unexpected non-match");
  CHECK_STREQ (svalue, newstring);

  msr3_free (&msr);
}