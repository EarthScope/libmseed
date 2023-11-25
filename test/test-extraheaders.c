#include <tau/tau.h>
#include <libmseed.h>
#include <yyjson.h>

#include "../yyjson.h"

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
  int64_t getint;
  double getnum;
  char getstr[100];
  int getbool;
  int rv;

  int64_t setint;
  double setnum;
  char *setstr;
  int setbool;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  msr->extralength = strlen (testheaders);
  msr->extra = malloc (msr->extralength);
  REQUIRE (msr->extra != NULL, "Error allocating memory for msr->extra");
  memcpy (msr->extra, testheaders, msr->extralength);

  /* Matching */
  rv = mseh_exists (msr, "/FDSN/Time/Quality");
  CHECK (rv, "mseh_exists() returned unexpected false");

  rv = mseh_exists (msr, "/FDSN/Event/Detection/0");
  CHECK (rv, "mseh_exists() returned unexpected false for array element");

  rv = mseh_get_number (msr, "/FDSN/Time/Correction", &getnum);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (getnum == 1.234, "/FDSN/Time/Correction is not expected 1.234");

  rv = mseh_get_int64 (msr, "/FDSN/Time/Quality", &getint);
  CHECK (rv == 0, "mseh_get_int64() returned unexpected non-match");
  CHECK (getint == 100, "/FDSN/Time/Quality is not expected 100");

  /* Key in first (0th) object of /FDSN/Event/Detection array */
  rv = mseh_get_string (msr, "/FDSN/Event/Detection/0/Type", getstr, sizeof(getstr));
  CHECK (rv == 0, "mseh_get_string() returned unexpected non-match");
  CHECK_STREQ (getstr, "MURDOCK");

  rv = mseh_get_boolean (msr, "/FDSN/Event/Begin", &getbool);
  CHECK (rv == 0, "mseh_get_boolean() returned unexpected non-match");
  CHECK (getbool, "/FDSN/Event/Begin is not expected True");

  /* Non matching */
  rv = mseh_exists (msr, "/FDSN/Event/Detection/1");
  CHECK (!rv, "mseh_exists() returned unexpected true for array element");

  rv = mseh_get_int64 (msr, "/A/Non/Existant/Header", &getint);
  CHECK (rv != 0, "mseh_get_int64() returned unexpected match");

  /* Set and get */
  setnum = 3.14159;
  rv = mseh_set_number (msr, "/New/Number", &setnum);
  CHECK (rv == 0, "mseh_set_num() returned unexpected error");

  rv = mseh_get_number (msr, "/New/Number", &getnum);
  CHECK (rv == 0, "mseh_get_num() returned unexpected non-match");
  CHECK (getnum == setnum);

  setint = 51204;
  rv = mseh_set_int64 (msr, "/New/Integer", &setint);
  CHECK (rv == 0, "mseh_set_int64() returned unexpected error");

  rv = mseh_get_int64 (msr, "/New/Integer", &getint);
  CHECK (rv == 0, "mseh_set_int64() returned unexpected non-match");
  CHECK (getint == setint);

  setstr = "Value";
  rv = mseh_set_string (msr, "/New/String", setstr);
  CHECK (rv == 0, "mseh_set_string() returned unexpected error");

  rv = mseh_get_string (msr, "/New/String", getstr, sizeof(getstr));
  CHECK (rv == 0, "mseh_get_string() returned unexpected non-match");
  CHECK_STREQ (getstr, setstr);

  setbool = true;
  rv = mseh_set_boolean (msr, "/New/Boolean", &setbool);
  CHECK (rv == 0, "mseh_set_boolean() returned unexpected error");

  rv = mseh_get_boolean (msr, "/New/Boolean", &getbool);
  CHECK (rv == 0, "mseh_set_boolean() returned unexpected non-match");
  CHECK (getbool == setbool);

  msr3_free (&msr);
}

TEST (extraheaders, mergepatch)
{
  MS3Record *msr = NULL;
  char *jsondoc;
  char *patchdoc;
  int rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  /* Populate initial header JSON */
  jsondoc = "{\"root\":{\"string\":\"value\"}}";
  msr->extralength = strlen (jsondoc);
  msr->extra = malloc (msr->extralength);
  REQUIRE (msr->extra != NULL, "Error allocating memory for msr->extra");
  memcpy (msr->extra, jsondoc, msr->extralength);

  /* Replace /root/string value with a pointer to the whole document ("") */
  patchdoc = "{\"root\":{\"string\":\"Updated value\"}}";
  rv = mseh_set_ptr_r (msr, "", patchdoc, 'M', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  REQUIRE (msr->extra != NULL, "msr->extra cannot be NULL");
  jsondoc = patchdoc;
  CHECK_SUBSTREQ (msr->extra, jsondoc, strlen(jsondoc));

  /* Add the /root/array value with pointer to /root */
  patchdoc = "{\"array\":[1,2,3]}";
  rv = mseh_set_ptr_r (msr, "/root", patchdoc, 'M', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  REQUIRE (msr->extra != NULL, "msr->extra cannot be NULL");
  jsondoc = "{\"root\":{\"string\":\"Updated value\",\"array\":[1,2,3]}}";
  CHECK_SUBSTREQ (msr->extra, jsondoc, strlen(jsondoc));

  /* Remove /root/string, /root/array, and add /root/boolean */
  patchdoc = "{\"root\":{\"string\":null,\"array\":null,\"boolean\":true}}";
  rv = mseh_set_ptr_r (msr, "", patchdoc, 'M', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  REQUIRE (msr->extra != NULL, "msr->extra cannot be NULL");
  jsondoc = "{\"root\":{\"boolean\":true}}";
  CHECK_SUBSTREQ (msr->extra, jsondoc, strlen(jsondoc));

  msr3_free (&msr);
}

TEST (extraheaders, replace)
{
  MS3Record *msr = NULL;
  char *jsondoc;
  char *newdoc;
  char *newdoc_uncompact;
  int rv;

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  /* Populate initial header JSON */
  jsondoc = "{\"root\":{\"string\":\"value\"}}";
  msr->extralength = strlen (jsondoc);
  msr->extra = malloc (msr->extralength);
  REQUIRE (msr->extra != NULL, "Error allocating memory for msr->extra");
  memcpy (msr->extra, jsondoc, msr->extralength);

  /* Replace extra headers with new, compact doc */
  newdoc = "{\"new\":{\"string\":\"Updated value\"}}";
  rv = mseh_replace (msr, newdoc);
  CHECK (rv == strlen(newdoc), "mseh_replace() returned unexpected error");
  REQUIRE (msr->extra != NULL, "msr->extra cannot be NULL");
  CHECK_SUBSTREQ (msr->extra, newdoc, strlen(newdoc));

  /* Replace extra headers with new, uncompact doc */
  newdoc = "{\"new\":{\"string\":\"Updated value\"}}";
  newdoc_uncompact = "{  \"new\":\n  {  \"string\"  :  \n  \"Updated value\"  }  }";
  rv = mseh_replace (msr, newdoc_uncompact);
  CHECK (rv == strlen(newdoc), "mseh_replace() returned unexpected error");
  REQUIRE (msr->extra != NULL, "msr->extra cannot be NULL");
  CHECK_SUBSTREQ (msr->extra, newdoc, strlen(newdoc));

  /* Remove extra headers */
  rv = mseh_replace (msr, NULL);
  CHECK (rv == 0, "mseh_replace() returned unexpected error");
  REQUIRE (msr->extra == NULL, "msr->extra MUST be NULL");

  msr3_free (&msr);
}

TEST (extraheaders, internal)
{
  MS3Record *msr = NULL;
  yyjson_mut_val mut_val;
  int64_t getint;
  double getnum;
  int getbool;
  char *string;
  int rv;

  /* Test internal functionality of raw yyjson values and array appending */

  msr = msr3_init (msr);
  REQUIRE (msr != NULL, "msr3_init() returned unexpected NULL");

  yyjson_mut_set_str (&mut_val, "value");
  rv = mseh_set_ptr_r (msr, "/root/string", &mut_val, 'V', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  string = "{\"root\":{\"string\":\"value\"}}";
  CHECK_SUBSTREQ (msr->extra, string, strlen(string));

  yyjson_mut_set_real (&mut_val, 123.456);
  rv = mseh_set_ptr_r (msr, "/root/real", &mut_val, 'V', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  rv = mseh_get_number (msr, "/root/real", &getnum);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (getnum == 123.456, "mseh_get_number() did not return expected value");

  yyjson_mut_set_sint (&mut_val, -123456);
  rv = mseh_set_ptr_r (msr, "/root/int", &mut_val, 'V', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  rv = mseh_get_int64 (msr, "/root/int", &getint);
  CHECK (rv == 0, "mseh_get_int64() returned unexpected non-match");
  CHECK (getint == -123456, "mseh_get_int64() did not return expected value");

  yyjson_mut_set_bool (&mut_val, false);
  rv = mseh_set_ptr_r (msr, "/root/bool", &mut_val, 'V', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");
  rv = mseh_get_boolean (msr, "/root/bool", &getbool);
  CHECK (rv == 0, "mseh_get_boolean() returned unexpected non-match");
  CHECK (getbool == false, "mseh_get_boolean() did not return expected value");

  /* Build array of values */
  yyjson_mut_set_str (&mut_val, "value");
  rv = mseh_set_ptr_r (msr, "/root/array", &mut_val, 'A', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");

  yyjson_mut_set_real (&mut_val, 123.456);
  rv = mseh_set_ptr_r (msr, "/root/array", &mut_val, 'A', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");

  yyjson_mut_set_sint (&mut_val, -123456);
  rv = mseh_set_ptr_r (msr, "/root/array", &mut_val, 'A', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");

  yyjson_mut_set_bool (&mut_val, false);
  rv = mseh_set_ptr_r (msr, "/root/array", &mut_val, 'A', NULL);
  CHECK (rv == 0, "mseh_set_ptr_r() returned unexpected error");

  /* Get 2nd value */
  rv = mseh_get_number (msr, "/root/array/1", &getnum);
  CHECK (rv == 0, "mseh_get_number() returned unexpected non-match");
  CHECK (getnum == 123.456, "mseh_get_number() did not return expected value");

  msr3_free (&msr);
}