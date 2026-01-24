/***************************************************************************
 * A program demonstrating manipulation of extra headers in miniSEED records
 *
 * Extra headers are stored as JSON and accessed using JSON Pointer syntax
 * (RFC 6901).
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2026 Chad Trabant, EarthScope Data Services
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <libmseed.h>

const char *
header_type (int type)
{
  switch (type)
  {
  case 'u':
    return "unsigned integer";
  case 'i':
    return "signed integer";
  case 'n':
    return "number (real)";
  case 's':
    return "string";
  case 'b':
    return "boolean";
  case 'a':
    return "array";
  case 'o':
    return "object";
  default:
    return "unknown or not found";
  }
}

int
main ()
{
  MS3Record *msr = NULL;

  /* Values for setting and getting */
  uint64_t quality;
  int64_t leap_second;
  double correction;
  int event_begin;

  /* Create a new miniSEED record */
  msr = msr3_init (NULL);
  if (!msr)
  {
    ms_log (2, "Error initializing MS3Record\n");
    return -1;
  }

  /* Populate basic header fields */
  strcpy (msr->sid, "FDSN:XX_TEST__L_H_Z");
  msr->reclen = 512;
  msr->pubversion = 1;
  msr->starttime = ms_timestr2nstime ("2024-01-24T12:00:00.000000Z");
  msr->samprate = 1.0;
  msr->encoding = DE_STEIM2;
  msr->numsamples = 100;
  msr->datasize = 0;

  printf ("Setting FDSN and custom headers:\n");

  quality = 100;
  if (mseh_set_uint64 (msr, "/FDSN/Time/Quality", &quality))
  {
    ms_log (2, "Error setting /FDSN/Time/Quality header\n");
  }

  leap_second = -1;
  if (mseh_set_int64 (msr, "/FDSN/Time/LeapSecond", &leap_second))
  {
    ms_log (2, "Error setting /FDSN/Time/LeapSecond header\n");
  }

  correction = 1.234567;
  if (mseh_set_number (msr, "/FDSN/Time/Correction", &correction))
  {
    ms_log (2, "Error setting /FDSN/Time/Correction header\n");
  }

  event_begin = 1;
  if (mseh_set_boolean (msr, "/FDSN/Event/Begin", &event_begin))
  {
    ms_log (2, "Error setting /FDSN/Event/Begin header\n");
  }

  /* Set custom headers of string type */
  char *status = "Down";
  if (mseh_set_string (msr, "/Endor/Shield/Status", status))
  {
    ms_log (2, "Error setting /Endor/Shield/Status header\n");
  }
  char *time_string = "1983-05-25T09:14:00.000000Z";
  if (mseh_set_string (msr, "/Endor/Shield/BootTime", time_string))
  {
    ms_log (2, "Error setting /Endor/Shield/BootTime header\n");
  }

  /* Print all extra headers */
  printf ("\n==== Printing all extra headers ====\n");
  if (mseh_print (msr, 2) < 0)
  {
    ms_log (2, "Error printing extra headers\n");
  }

  printf ("\n==== Checking existence of headers ====\n");

  if (mseh_exists (msr, "/FDSN/Time/Quality"))
  {
    printf ("  /FDSN/Time/Quality exists\n");
  }

  if (mseh_exists (msr, "/FDSN/Time/MaxEstimatedError") == 0)
  {
    printf ("  /FDSN/Time/MaxEstimatedError DOES NOT exist\n");
  }

  /* Get values */
  if (mseh_get_uint64 (msr, "/FDSN/Time/Quality", &quality) == 0)
  {
    printf ("  Got /FDSN/Time/Quality = %" PRIu64 "\n", quality);
  }

  if (mseh_get_int64 (msr, "/FDSN/Time/LeapSecond", &leap_second) == 0)
  {
    printf ("  Got /FDSN/Time/LeapSecond = %lld\n", (long long)leap_second);
  }

  if (mseh_get_number (msr, "/FDSN/Time/Correction", &correction) == 0)
  {
    printf ("  Got /FDSN/Time/Correction = %.6f\n", correction);
  }

  if (mseh_get_boolean (msr, "/FDSN/Event/Begin", &event_begin) == 0)
  {
    printf ("  Got /FDSN/Event/Begin = %s\n", event_begin ? "true" : "false");
  }

  char get_status[100];
  if (mseh_get_string (msr, "/Endor/Shield/Status", get_status,
                       sizeof (get_status)) == 0)
  {
    printf ("  Got /Endor/Shield/Status = \"%s\"\n", get_status);
  }

  char get_time_string[100];
  if (mseh_get_string (msr, "/Endor/Shield/BootTime", get_time_string,
                       sizeof (get_time_string)) == 0)
  {
    printf ("  Got /Endor/Shield/BootTime = \"%s\"\n", get_time_string);
  }

  printf ("\n==== Checking header types ====\n");

  int type = mseh_get_ptr_type (msr, "/FDSN/Time", NULL);
  printf ("  /FDSN/Time type: %s\n", header_type (type));

  type = mseh_get_ptr_type (msr, "/FDSN/Time/Quality", NULL);
  printf ("  /FDSN/Time/Quality type: %s\n", header_type (type));

  type = mseh_get_ptr_type (msr, "/Endor/Shield/BootTime", NULL);
  printf ("  /Endor/Shield/BootTime type: %s\n", header_type (type));

  printf ("\n==== Apply JSON Merge Patch to modify headers ====\n");

  /* Create a merge patch that:
   *   - Adds /FDSN/Event/End
   *   - Removes /FDSN/Event/Begin
   *   - Modifies /FDSN/Time/Quality to 96 */
  char *merge_patch = "{\"FDSN\": {\"Event\": {\"End\": true, \"Begin\": null}, \"Time\": {\"Quality\": 96}}}";
  if (mseh_set_ptr_r (msr, "", merge_patch, 'M', NULL) < 0)
  {
    ms_log (2, "Error applying merge patch\n");
  }

  if (mseh_print (msr, 2) < 0)
  {
    ms_log (2, "Error printing extra headers\n");
  }

  printf ("\n==== Replace all extra headers ====\n");

  char *new_headers = "{\"Operator\": {\"Base\": \"Hoth\", \"Temperature\": -32.1}}";
  if (mseh_replace (msr, new_headers) < 0)
  {
    ms_log (2, "Error replacing extra headers\n");
  }
  else
  {
    printf ("\nNew extra headers:\n");
    mseh_print (msr, 2);
  }

  /* Clean up */
  msr3_free (&msr);

  return 0;
}
