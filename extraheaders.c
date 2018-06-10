 /***************************************************************************
 * extraheaders.c
 *
 * Routines for dealing with libmseed extra headers stored as JSON.
 *
 * Written by Chad Trabant
 * IRIS Data Management Center
 ***************************************************************************/

#include "libmseed.h"
#include "parson.h"

/***************************************************************************
 * mseh_get_path:
 *
 * Search for and return an extra header value specified as a path in
 * dot notation, e.g "objectA.objectB.value"
 *
 * This routine can get used to test for the existence of a value
 * without returning the value by setting 'value' to NULL.
 *
 * If the target item is found (and 'value' parameter is set) the
 * value will be copied into the memory specified by 'value'.  The
 * 'type' value specifies the data type expected:
 *
 * type  expected type for 'value'
 * ----  -------------------------
 * n     double
 * s     unsigned char* (maximum length is: 'length' - 1)
 * b     int (boolean value of 0 or 1)
 *
 * Returns:
 *   0 on success,
 *   1 when the value was not found,
 *   2 when the value is of the wrong type,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_get_path (MS3Record *msr, const char *path, void *value, char type, size_t length)
{
  JSON_Object *rootobject = NULL;
  JSON_Value *rootvalue = NULL;
  JSON_Value *extravalue = NULL;
  JSON_Value_Type valuetype;
  const char *stringvalue = NULL;
  int retval = 0;

  if (!msr || !path)
    return MS_GENERROR;

  if (!msr->extralength)
    return 1;

  if (!msr->extra)
    return MS_GENERROR;

  /* Parse JSON extra headers */
  rootvalue = json_parse_string (msr->extra);

  if (!rootvalue)
  {
    ms_log (2, "mseh_fetch_path(): Extra headers are not JSON\n", type);
    return MS_GENERROR;
  }

  /* Get expected root object */
  rootobject = json_value_get_object (rootvalue);

  if (!rootobject)
  {
    ms_log (2, "mseh_fetch_path(): Extra headers are not a JSON object\n", type);
    json_value_free(rootvalue);
    return MS_GENERROR;
  }

  /* Get target value */
  extravalue = json_object_dotget_value (rootobject, path);

  if (!extravalue)
  {
    json_value_free(rootvalue);
    return 1;
  }

  valuetype = json_value_get_type (extravalue);

  if (type == 'n' && valuetype == JSONNumber)
  {
    if (value)
      *((double*)value) = json_value_get_number (extravalue);
  }
  else if (type == 's' && valuetype == JSONString)
  {
    if (value)
    {
      stringvalue = json_value_get_string (extravalue);
      strncpy ((char *)value, stringvalue, length);
    }
  }
  else if (type == 'b' && valuetype == JSONBoolean)
  {
    if (value)
      *((int*)value) = json_value_get_boolean (extravalue);
  }
  /* Return wrong type indicator if a value was requested */
  else if (value)
  {
    retval = 2;
  }

  json_value_free(rootvalue);

  return retval;
} /* End of mseh_fetch_path() */

/***************************************************************************
 * mseh_set_path:
 *
 * Set the value of a single extra header value specified as a path in
 * dot notation, e.g "objectA.objectB.value".
 *
 * If the path or final header values do not exist they will be
 * created.  If the header value exists it will be replaced.
 *
 * The 'type' value specifies the data type expected for the value in
 * 'value':
 *
 * type  expected type for 'value'
 * ----  -------------------------
 * n     double
 * s     unsigned char* (of 'length' bytes)
 * b     int (boolean value of 0 or 1)
 * A     Array element as JSON_Value*
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_set_path (MS3Record *msr, const char *path, void *value, char type, size_t length)
{
  JSON_Object *rootobject = NULL;
  JSON_Value *rootvalue = NULL;
  JSON_Array *array = NULL;
  size_t serializationsize = 0;
  char *serialized = NULL;

#define EVALSET(KEY, SET)                                 \
  if (SET != JSONSuccess)                                 \
  {                                                       \
    ms_log (2, "mseh_set_path(): Cannot set: %s\n", KEY); \
    if (rootvalue)                                        \
      json_value_free (rootvalue);                        \
    return MS_GENERROR;                                   \
  }

  if (!msr || !value || !path)
    return MS_GENERROR;

  /* Parse existing extra headers */
  if (msr->extra && msr->extralength > 0)
  {
    rootvalue = json_parse_string (msr->extra);

    if (!rootvalue)
    {
      ms_log (2, "mseh_fetch_path(): Extra headers are not JSON\n", type);
      return MS_GENERROR;
    }

    /* Get expected root object */
    rootobject = json_value_get_object (rootvalue);

    if (!rootobject)
    {
      ms_log (2, "mseh_fetch_path(): Extra headers are not a JSON object\n", type);
      json_value_free (rootvalue);
      return MS_GENERROR;
    }
  }
  /* If no existig headers, initialize a new base object */
  else
  {
    rootvalue = json_value_init_object ();
    rootobject = json_value_get_object (rootvalue);

    if (!rootobject)
    {
      ms_log (2, "mseh_fetch_path(): Cannot initialize new JSON object\n", type);
      if (rootvalue)
        json_value_free (rootvalue);
      return MS_GENERROR;
    }
  }

  switch (type)
  {
  case 'n':
    EVALSET (path, json_object_dotset_number (rootobject, path, *((double *)value)));
    break;
  case 's':
    EVALSET (path, json_object_dotset_string (rootobject, path, (const char *)value));
    break;
  case 'b':
    EVALSET (path, json_object_dotset_boolean (rootobject, path, *((int *)value)));
    break;
  case 'A':
    array = json_object_dotget_array (rootobject, path);

    /* Create array if it was not found */
    if (!array)
    {
      EVALSET (path, json_object_dotset_value (rootobject, path, json_value_init_array ()));

      array = json_object_dotget_array (rootobject, path);

      if (!array)
      {
        ms_log (2, "mseh_set_path(): Cannot get array\n");
        if (value)
          json_value_free (rootvalue);
        return MS_GENERROR;
      }
    }

    EVALSET ("Array JSON_Value", json_array_append_value (array, (JSON_Value *)value));
    break;
  default:
    ms_log (2, "mseh_set_path(): Unrecognized type '%d'\n", type);
    json_value_free (rootvalue);
    return MS_GENERROR;
  }

  /* Serialize new JSON */
  serializationsize = json_serialization_size (rootvalue);

  if (!serializationsize)
  {
    ms_log (2, "mseh_set_path(): Cannot determine new serialization size\n", type);
    json_value_free (rootvalue);
    return MS_GENERROR;
  }

  if (serializationsize > 65535)
  {
    ms_log (2, "mseh_set_path(): New serialization size exceeds limit of 65,535 bytes: %" PRIu64 "\n",
            (uint64_t)serializationsize);
    json_value_free (rootvalue);
    return MS_GENERROR;
  }

  serialized = (char *)malloc (serializationsize);

  if (!serialized)
  {
    ms_log (2, "mseh_set_path(): Cannot determine new serialization size\n", type);
    json_value_free (rootvalue);
    return MS_GENERROR;
  }

  if (json_serialize_to_buffer (rootvalue, serialized, serializationsize) != JSONSuccess)
  {
    ms_log (2, "mseh_set_path(): Error serializing JSON for extra headers\n");
    json_value_free (rootvalue);
    return MS_GENERROR;
  }

  /* Set new extra headers, replacing existing headers */
  if (msr->extra)
    free (msr->extra);
  msr->extra = serialized;
  msr->extralength = (uint16_t)serializationsize - 1;

  return 0;
#undef EVALSET
} /* End of mseh_set_path() */

/***************************************************************************
 * mseh_add_event_detection:
 *
 * Add specified event detection to the extra headers of the given record.
 *
 * If 'path' is NULL, the default is "FDSN.Event.Detection".
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_event_detection (MS3Record *msr, const char *path,
                          MSEHEventDetection *eventdetection)
{
  JSON_Value *value = NULL;
  JSON_Object *object = NULL;
  JSON_Array *array = NULL;
  char timestring[31];
  char *cp = NULL;

#define EVALSET(KEY, SET)                                            \
  if (SET != JSONSuccess)                                            \
  {                                                                  \
    ms_log (2, "mseh_add_event_detection(): Cannot set: %s\n", KEY); \
    if (value)                                                       \
      json_value_free (value);                                       \
    return MS_GENERROR;                                              \
  }

  if (!msr || !eventdetection)
    return MS_GENERROR;

  /* Initialize a new object */
  value = json_value_init_object();
  object = json_value_get_object(value);

  if (!object)
  {
    ms_log (2, "mseh_add_event_detection(): Cannot initialize new JSON object\n");
    if (value)
      json_value_free(value);
    return MS_GENERROR;
  }

  /* Add elements to new object */
  if (eventdetection->type[0])
  {
    EVALSET ("Type", json_object_set_string (object, "Type", eventdetection->type));
  }
  if (eventdetection->signalamplitude != 0.0)
  {
    EVALSET ("SignalAmplitude", json_object_set_number (object, "SignalAmplitude", eventdetection->signalamplitude));
  }
  if (eventdetection->signalperiod != 0.0)
  {
    EVALSET ("SignalPeriod", json_object_set_number (object, "SignalPeriod", eventdetection->signalperiod));
  }
  if (eventdetection->backgroundestimate != 0.0)
  {
    EVALSET ("BackgroundEstimate", json_object_set_number (object, "BackgroundEstimate", eventdetection->backgroundestimate));
  }
  if (eventdetection->detectionwave[0])
  {
    EVALSET ("DetectionWave", json_object_set_string (object, "DetectionWave", eventdetection->detectionwave));
  }
  if (eventdetection->units[0])
  {
    EVALSET ("Units", json_object_set_string (object, "Units", eventdetection->units));
  }
  if (eventdetection->onsettime != NSTERROR)
  {
    /* Create ISO-formatted time string with (UTC) Z suffix */
    cp = ms_nstime2isotimestr (eventdetection->onsettime, timestring, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    EVALSET ("OnsetTime", json_object_set_string (object, "OnsetTime", timestring));
  }
  if (memcmp (eventdetection->medsnr, (uint8_t []){0,0,0,0,0,0}, 6))
  {
    EVALSET ("MEDSNR", json_object_set_value(object, "MEDSNR", json_value_init_array()));

    array = json_object_get_array(object, "MEDSNR");

    if (!array)
    {
      ms_log (2, "mseh_add_event_detection(): Cannot get MEDSNR array\n");
      if (value)
        json_value_free (value);
      return MS_GENERROR;
    }

    /* Array containing the 6 SNR values */
    EVALSET ("MEDSNR-0", json_array_append_number(array, (double)eventdetection->medsnr[0]));
    EVALSET ("MEDSNR-1", json_array_append_number(array, (double)eventdetection->medsnr[1]));
    EVALSET ("MEDSNR-2", json_array_append_number(array, (double)eventdetection->medsnr[2]));
    EVALSET ("MEDSNR-3", json_array_append_number(array, (double)eventdetection->medsnr[3]));
    EVALSET ("MEDSNR-4", json_array_append_number(array, (double)eventdetection->medsnr[4]));
    EVALSET ("MEDSNR-5", json_array_append_number(array, (double)eventdetection->medsnr[5]));
  }
  if (eventdetection->medlookback >= 0)
  {
    EVALSET ("MEDLookback", json_object_set_number (object, "MEDLookback", (double)eventdetection->medlookback));
  }
  if (eventdetection->medpickalgorithm >= 0)
  {
    EVALSET ("MEDPickAlgorithm", json_object_set_number (object, "MEDPickAlgorithm", (double)eventdetection->medpickalgorithm));
  }
  if (eventdetection->detector[0])
  {
    EVALSET ("Detector", json_object_set_string (object, "Detector", eventdetection->detector));
  }

  /* Add new object to array */
  if (mseh_set_path (msr, (path) ? path : "FDSN.Event.Detection", value, 'A', 0))
  {
    ms_log (2, "mseh_add_event_detection(): Cannot add new array entry\n");
    if (value)
      json_value_free (value);
    return MS_GENERROR;
  }

  if (value)
    json_value_free (value);

  return 0;
#undef EVALSET
} /* End of mseh_add_event_detection() */

/***************************************************************************
 * mseh_add_calibration:
 *
 * Add specified calibration to the extra headers of the given record.
 *
 * If 'path' is NULL, the default is "FDSN.Calibration.Sequence".
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_calibration (MS3Record *msr, const char *path,
                      MSEHCalibration *calibration)
{
  JSON_Value *value = NULL;
  JSON_Object *object = NULL;
  char beginstring[31];
  char endstring[31];
  char *cp = NULL;

#define EVALSET(KEY, SET)                                        \
  if (SET != JSONSuccess)                                        \
  {                                                              \
    ms_log (2, "mseh_add_calibration(): Cannot set: %s\n", KEY); \
    if (value)                                                   \
      json_value_free (value);                                   \
    return MS_GENERROR;                                          \
  }

  if (!msr || !calibration)
    return MS_GENERROR;

  /* Initialize a new object */
  value = json_value_init_object();
  object = json_value_get_object(value);

  if (!object)
  {
    ms_log (2, "mseh_add_calibration(): Cannot initialize new JSON object\n");
    if (value)
      json_value_free(value);
    return MS_GENERROR;
  }

  /* Add elements to new object */
  if (calibration->type[0])
  {
    EVALSET ("Type", json_object_set_string (object, "Type", calibration->type));
  }
  if (calibration->begintime != NSTERROR)
  {
    /* Create ISO-formatted time string with (UTC) Z suffix */
    cp = ms_nstime2isotimestr (calibration->begintime, beginstring, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    EVALSET ("BeginTime", json_object_set_string (object, "BeginTime", beginstring));
  }
  if (calibration->endtime != NSTERROR)
  {
    /* Create ISO-formatted time string with (UTC) Z suffix */
    cp = ms_nstime2isotimestr (calibration->endtime, endstring, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    EVALSET ("EndTime", json_object_set_string (object, "EndTime", endstring));
  }
  if (calibration->steps >= 0)
  {
    EVALSET ("Steps", json_object_set_number (object, "Steps", (double)calibration->steps));
  }
  if (calibration->firstpulsepositive >= 0)
  {
    EVALSET ("FirstPulsePositive", json_object_set_boolean (object, "FirstPulsePositive", calibration->firstpulsepositive));
  }
  if (calibration->alternatesign >= 0)
  {
    EVALSET ("AlternateSign", json_object_set_boolean (object, "AlternateSign", calibration->alternatesign));
  }
  if (calibration->trigger[0])
  {
    EVALSET ("Trigger", json_object_set_string (object, "Trigger", calibration->trigger));
  }
  if (calibration->continued >= 0)
  {
    EVALSET ("Continued", json_object_set_boolean (object, "Continued", calibration->continued));
  }
  if (calibration->amplitude != 0.0)
  {
    EVALSET ("Amplitude", json_object_set_number (object, "Amplitude", calibration->amplitude));
  }
  if (calibration->inputunits[0])
  {
    EVALSET ("InputUnits", json_object_set_string (object, "InputUnits", calibration->inputunits));
  }
  if (calibration->amplituderange[0])
  {
    EVALSET ("AmplitudeRange", json_object_set_string (object, "AmplitudeRange", calibration->amplituderange));
  }
  if (calibration->duration != 0.0)
  {
    EVALSET ("Duration", json_object_set_number (object, "Duration", calibration->duration));
  }
  if (calibration->sineperiod != 0.0)
  {
    EVALSET ("SinePeriod", json_object_set_number (object, "SinePeriod", calibration->sineperiod));
  }
  if (calibration->stepbetween != 0.0)
  {
    EVALSET ("StepBetween", json_object_set_number (object, "StepBetween", calibration->stepbetween));
  }
  if (calibration->inputchannel[0])
  {
    EVALSET ("InputChannel", json_object_set_string (object, "InputChannel", calibration->inputchannel));
  }
  if (calibration->refamplitude != 0.0)
  {
    EVALSET ("ReferenceAmplitude", json_object_set_number (object, "ReferenceAmplitude", calibration->refamplitude));
  }
  if (calibration->coupling[0])
  {
    EVALSET ("Coupling", json_object_set_string (object, "Coupling", calibration->coupling));
  }
  if (calibration->rolloff[0])
  {
    EVALSET ("Rolloff", json_object_set_string (object, "Rolloff", calibration->rolloff));
  }
  if (calibration->noise[0])
  {
    EVALSET ("Noise", json_object_set_string (object, "Noise", calibration->noise));
  }

  /* Add new object to array */
  if (mseh_set_path (msr, (path) ? path : "FDSN.Calibration.Sequence", value, 'A', 0))
  {
    ms_log (2, "mseh_add_calibration(): Cannot add new array entry\n");
    if (value)
      json_value_free (value);
    return MS_GENERROR;
  }

  if (value)
    json_value_free (value);

  return 0;
#undef EVALSET
} /* End of mseh_add_calibration() */

/***************************************************************************
 * mseh_add_timing_exception:
 *
 * Add specified timing exception to the extra headers of the given record.
 *
 * If 'path' is NULL, the default is "FDSN.Time.Exception".
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_timing_exception (MS3Record *msr, const char *path,
                           MSEHTimingException *exception)
{
  JSON_Value *value = NULL;
  JSON_Object *object = NULL;
  char timestring[31];
  char *cp = NULL;

#define EVALSET(KEY, SET)                                             \
  if (SET != JSONSuccess)                                             \
  {                                                                   \
    ms_log (2, "mseh_add_timing_exception(): Cannot set: %s\n", KEY); \
    if (value)                                                        \
      json_value_free (value);                                        \
    return MS_GENERROR;                                               \
  }

  if (!msr || !exception)
    return MS_GENERROR;

  /* Initialize a new object */
  value = json_value_init_object();
  object = json_value_get_object(value);

  if (!object)
  {
    ms_log (2, "mseh_add_timing_exception(): Cannot initialize new JSON object\n");
    if (value)
      json_value_free(value);
    return MS_GENERROR;
  }

  /* Add elements to new object */
  if (exception->vcocorrection >= 0.0)
  {
    EVALSET ("VCOCorrection", json_object_set_number (object, "VCOCorrection", (double)exception->vcocorrection));
  }
  if (exception->time != NSTERROR)
  {
    /* Create ISO-formatted time string with (UTC) Z suffix */
    cp = ms_nstime2isotimestr (exception->time, timestring, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    EVALSET ("Time", json_object_set_string (object, "Time", timestring));
  }
  if (exception->receptionquality >= 0)
  {
    EVALSET ("ReceptionQuality", json_object_set_number (object, "ReceptionQuality", (double)exception->receptionquality));
  }
  if (exception->count > 0)
  {
    EVALSET ("Count", json_object_set_number (object, "Count", (double)exception->count));
  }
  if (exception->type[0])
  {
    EVALSET ("Type", json_object_set_string (object, "Type", exception->type));
  }
  if (exception->clockstatus[0])
  {
    EVALSET ("ClockStatus", json_object_set_string (object, "ClockStatus", exception->clockstatus));
  }

  if (mseh_set_path (msr, (path) ? path : "FDSN.Time.Exception", value, 'A', 0))
  {
    ms_log (2, "mseh_add_timing_exception(): Cannot add new array entry\n");
    if (value)
      json_value_free (value);
    return MS_GENERROR;
  }

  if (value)
    json_value_free (value);

  return 0;
#undef EVALSET
} /* End of mseh_add_timing_exception() */

/***************************************************************************
 * mseh_print:
 *
 * Print the extra header (CBOR Map) structure for the specified
 * MS3Record.
 *
 * Output is printed in a pretty, formatted form for readability and
 * the root object is not printed.
 *
 * Returns MS_NOERROR on success and a libmseed error code on error.
 ***************************************************************************/
int
mseh_print (MS3Record *msr, int indent)
{
  char *extra;
  int idx;
  int instring = 0;

  if (!msr)
    return MS_GENERROR;

  if (!msr->extra || !msr->extralength)
    return MS_NOERROR;

  extra = msr->extra;

  if ( extra[0] != '{' || extra[msr->extralength-1] != '}')
  {
    ms_log (1, "Warning, something is wrong, extra headers are not a clean {object}\n");
  }

  /* Print JSON character-by-character, inserting
   * indentation, spaces and newlines for readability. */
  ms_log (0, "%*s", indent, "");
  for (idx = 1; idx < (msr->extralength - 1); idx++)
  {
    /* Toggle "in string" flag for double quotes */
    if (extra[idx] == '"')
      instring = (instring) ? 0 : 1;

    if (!instring)
    {
      if ( extra[idx] == ':')
      {
        ms_log (0, ": ");
      }
      else if ( extra[idx] == ',')
      {
        ms_log (0, ",\n%*s", indent, "");
      }
      else if ( extra[idx] == '{' )
      {
        indent += 2;
        ms_log (0, "{\n%*s", indent, "");
      }
      else if ( extra[idx] == '[' )
      {
        indent += 2;
        ms_log (0, "[\n%*s", indent, "");
      }
      else if ( extra[idx] == '}' )
      {
        indent -= 2;
        ms_log (0, "\n%*s}", indent, "");
      }
      else if ( extra[idx] == ']' )
      {
        indent -= 2;
        ms_log (0, "\n%*s]", indent, "");
      }
      else
      {
        ms_log (0, "%c", extra[idx]);
      }
    }
    else
    {
      ms_log (0, "%c", extra[idx]);
    }
  }
  ms_log (0, "\n");

  return MS_NOERROR;
} /* End of mseh_print() */
