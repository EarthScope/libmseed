 /***************************************************************************
 * extraheaders.c
 *
 * Routines for dealing with libmseed extra headers stored as a CBOR Map.
 *
 * Written by Chad Trabant
 * IRIS Data Management Center
 ***************************************************************************/

#include "libmseed.h"
#include "cbor.h"

/***************************************************************************
 * mseh_fetch_path:
 *
 * Search for and return an extra header value specified as a path of
 * Map elements.  The path of Map elements is a sequence of Map keys
 * for nested structures.  This routine fetches the value of the last
 * Map key in the sequence.
 *
 * If the target item is found (and 'value' parameter is set) the
 * value will be copied into the memory specified by 'value'.  The
 * 'type' value specifies the data type expected:
 *
 * type  expected type for 'value'
 * ----  -------------------------
 * i     int64_t
 * d     double
 * c     unsigned char* (maximum length is: 'length' - 1)
 * b     int (boolean value of 0 or 1)
 *
 * Returns:
 *   0 on success,
 *   1 when the value was not found,
 *   2 when the value is of the wrong type,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_fetch_path (MS3Record *msr, void *value, char type, size_t length, const char *path[])
{
  cbor_stream_t stream;
  cbor_item_t vitem;

  if (!msr || !path)
    return MS_GENERROR;

  if (!msr->extralength)
    return 1;

  if (!msr->extra)
    return MS_GENERROR;

  cbor_init (&stream, msr->extra, msr->extralength);

  vitem.type = -1;
  cbor_fetch_map_value (&stream, 0, &vitem, path);

  if (vitem.type < 0)
    return 1;

  if (type == 'i' && (vitem.type == CBOR_UINT || vitem.type == CBOR_NEGINT))
  {
    if (value)
      *((int64_t*)value) = vitem.value.int64;
  }
  else if (type == 'd' && (vitem.type == CBOR_FLOAT16 || vitem.type == CBOR_FLOAT32 || vitem.type == CBOR_FLOAT64))
  {
    if (value)
      *((double*)value) = vitem.value.float64;
  }
  else if (type == 'c' && (vitem.type == CBOR_BYTES || vitem.type == CBOR_TEXT))
  {
    if (value)
    {
      /* Copy buffer and terminate */
      if (length > vitem.length)
      {
        memcpy (value, vitem.value.ucharp, vitem.length);
        ((uint8_t *)value)[vitem.length] = '\0';
      }
      else
      {
        memcpy (value, vitem.value.ucharp, length - 1);
        ((uint8_t *)value)[length - 1] = '\0';
      }
    }
  }
  else if (type == 'b' && (vitem.type == CBOR_TRUE || vitem.type == CBOR_FALSE))
  {
    if (value)
    {
      if (vitem.type == CBOR_TRUE)
        *((int*)value) = 1;
      else
        *((int*)value) = 0;
    }
  }
  /* Only return "wrong type" result when type is set */
  else if (type)
    return 2;

  return 0;
} /* End of mseh_fetch_path() */

/***************************************************************************
 * mseh_set_path:
 *
 * Set the value of a single extra header value specified as a path of
 * Map elements.  The path of Map elements is a sequence of Map keys
 * for nested structures.  This routine sets the value of the last
 * Map key in the sequence.
 *
 * If the path or final header values do not exist they will be created.
 *
 * The 'type' value specifies the data type expected for the value in
 * 'value':
 *
 * type  expected type for 'value'
 * ----  -------------------------
 * i     int64_t
 * d     double
 * c     unsigned char* (maximum length is: 'length' - 1)
 * b     int (boolean value of 0 or 1)
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_set_path (MS3Record *msr, void *value, char type, size_t length, const char *path[])
{
  cbor_stream_t stream;
  cbor_item_t vitem;

  if (!msr || !path || !value)
    return MS_GENERROR;

  switch (type)
  {
  case 'i':
    vitem.type = CBOR_NEGINT;
    vitem.valuetype = INT64T;
    vitem.value.int64 = *((int64_t *)value);
    break;
  case 'd':
    vitem.type = CBOR_FLOAT64;
    vitem.valuetype = FLOAT64T;
    vitem.value.float64 = *((double *)value);
    break;
  case 'c':
    vitem.type = CBOR_TEXT;
    vitem.valuetype = UCHARP;
    vitem.value.ucharp = (unsigned char *)value;
    vitem.length = length;
    break;
  case 'b':
    vitem.type = (*((int *)value)) ? CBOR_TRUE : CBOR_FALSE;
    vitem.valuetype = NONE;
    vitem.value.int64 = (vitem.type == CBOR_TRUE) ? 1 : 0;
    break;
  default:
    ms_log (2, "mseh_set_path(): Unrecognized type '%d'\n", type);
    return MS_GENERROR;
  }

  cbor_init (&stream, msr->extra, msr->extralength);

  if (!cbor_set_map_value (&stream, &vitem, path))
    return MS_GENERROR;

  msr->extra = stream.data;
  msr->extralength = stream.size;

  return 0;
} /* End of mseh_set_path() */

/***************************************************************************
 * mseh_add_event_detection:
 *
 * Add specified event detection to the extra headers of the given record.
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_event_detection (MS3Record *msr, MSEHEventDetection *eventdetection,
                          const char *path[])
{
#define MAXITEMS 11
  cbor_stream_t stream;
  cbor_stream_t array;
  cbor_item_t item[MAXITEMS];
  cbor_item_t *itemp[MAXITEMS];
  const char *keyp[MAXITEMS];
  char onsetstr[31];
  unsigned char arraydata[12];
  char *cp;
  int idx = 0;

  if (!msr)
    return MS_GENERROR;

  if (eventdetection->type[0])
  {
    keyp[idx] = "Type";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)eventdetection->type;
    item[idx].length = strlen(eventdetection->type);
    idx++;
  }
  if (eventdetection->signalamplitude != 0.0)
  {
    keyp[idx] = "SignalAmplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].valuetype = FLOAT64T;
    item[idx].value.float64 = eventdetection->signalamplitude;
    item[idx].length = 0;
    idx++;
  }
  if (eventdetection->signalperiod != 0.0)
  {
    keyp[idx] = "SignalPeriod";
    item[idx].type = CBOR_FLOAT64;
    item[idx].valuetype = FLOAT64T;
    item[idx].value.float64 = eventdetection->signalperiod;
    item[idx].length = 0;
    idx++;
  }
  if (eventdetection->backgroundestimate != 0.0)
  {
    keyp[idx] = "BackgroundEstimate";
    item[idx].type = CBOR_FLOAT64;
    item[idx].valuetype = FLOAT64T;
    item[idx].value.float64 = eventdetection->backgroundestimate;
    item[idx].length = 0;
    idx++;
  }
  if (eventdetection->detectionwave[0])
  {
    keyp[idx] = "DetectionWave";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)eventdetection->detectionwave;
    item[idx].length = strlen(eventdetection->detectionwave);
    idx++;
  }
  if (eventdetection->units[0])
  {
    keyp[idx] = "Units";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)eventdetection->units;
    item[idx].length = strlen(eventdetection->units);
    idx++;
  }
  if (eventdetection->onsettime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (eventdetection->onsettime, onsetstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "OnsetTime";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)onsetstr;
    item[idx].length = strlen (onsetstr);
    idx++;
  }
  if (memcmp (eventdetection->snrvalues, (uint8_t []){0,0,0,0,0,0}, 6))
  {
    /* Encode a small CBOR array containing the 6 SNR values */
    cbor_init (&array, arraydata, sizeof(arraydata));

    if (!cbor_serialize_array (&array, 6) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[0]) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[1]) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[2]) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[3]) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[4]) ||
        !cbor_serialize_int (&array, (int)eventdetection->snrvalues[5]))
    {
      return MS_GENERROR;
    }

    keyp[idx] = "MEDSNR";
    item[idx].type = 0;
    item[idx].valuetype = CBOR;
    item[idx].value.ucharp = array.data;
    item[idx].length = array.pos;
    idx++;
  }
  if (eventdetection->medlookback >= 0)
  {
    keyp[idx] = "MEDLookback";
    item[idx].type = CBOR_UINT;
    item[idx].valuetype = INT64T;
    item[idx].value.int64 = eventdetection->medlookback;
    item[idx].length = 0;
    idx++;
  }
  if (eventdetection->medpickalgorithm >= 0)
  {
    keyp[idx] = "MEDPickAlgorithm";
    item[idx].type = CBOR_UINT;
    item[idx].valuetype = INT64T;
    item[idx].value.int64 = eventdetection->medpickalgorithm;
    item[idx].length = 0;
    idx++;
  }
  if (eventdetection->detector[0])
  {
    keyp[idx] = "Detector";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)eventdetection->detector;
    item[idx].length = strlen(eventdetection->detector);
    idx++;
  }

  keyp[idx] = NULL;
  itemp[idx] = NULL;

  while (idx > 0)
  {
    idx--;
    itemp[idx] = &item[idx];
  }

  cbor_init (&stream, msr->extra, msr->extralength);

  /* Append Map entry to Array at specified or default path */
  if (!cbor_append_map_array (&stream, keyp, itemp,
                              (path) ? path : (const char *[]){"FDSN", "Event", "Detection", NULL}))
    return MS_GENERROR;

  msr->extra = stream.data;
  msr->extralength = stream.size;

  return 0;
#undef MAXITEMS
} /* End of mseh_add_event_detection() */

/***************************************************************************
 * mseh_add_calibration:
 *
 * Add specified calibration to the extra headers of the given record.
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_calibration (MS3Record *msr, MSEHCalibration *calibration,
                      const char *path[])
{
#define MAXITEMS 20
  cbor_stream_t stream;
  cbor_item_t item[MAXITEMS];
  cbor_item_t *itemp[MAXITEMS];
  const char *keyp[MAXITEMS];
  char beginstr[31];
  char endstr[31];
  char *cp;
  int idx = 0;

  if (!msr)
    return MS_GENERROR;

  if (calibration->type[0])
  {
    keyp[idx] = "Type";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->type;
    item[idx].length = strlen(calibration->type);
    idx++;
  }
  if (calibration->begintime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (calibration->begintime, beginstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "BeginTime";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)beginstr;
    item[idx].length = strlen(beginstr);
    idx++;
  }
  if (calibration->endtime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (calibration->endtime, endstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "EndTime";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)endstr;
    item[idx].length = strlen(endstr);
    idx++;
  }
  if (calibration->steps >= 0)
  {
    keyp[idx] = "Steps";
    item[idx].type = CBOR_UINT;
    item[idx].value.int64 = calibration->steps;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->firstpulsepositive >= 0)
  {
    keyp[idx] = "FirstPulsePositive";
    item[idx].type = (calibration->firstpulsepositive) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.int64 = (calibration->firstpulsepositive) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->alternatesign >= 0)
  {
    keyp[idx] = "AlternateSign";
    item[idx].type = (calibration->alternatesign) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.int64 = (calibration->alternatesign) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->trigger[0])
  {
    keyp[idx] = "Trigger";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->trigger;
    item[idx].length = strlen(calibration->trigger);
    idx++;
  }
  if (calibration->continued >= 0)
  {
    keyp[idx] = "Continued";
    item[idx].type = (calibration->continued) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.int64 = (calibration->continued) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->amplitude != 0.0)
  {
    keyp[idx] = "Amplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.float64 = calibration->amplitude;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->inputunits[0])
  {
    keyp[idx] = "InputUnits";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->inputunits;
    item[idx].length = strlen(calibration->inputunits);
    idx++;
  }
  if (calibration->amplituderange[0])
  {
    keyp[idx] = "AmplitudeRange";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->amplituderange;
    item[idx].length = strlen(calibration->amplituderange);
    idx++;
  }
  if (calibration->duration != 0.0)
  {
    keyp[idx] = "Duration";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.float64 = calibration->duration;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->sineperiod != 0.0)
  {
    keyp[idx] = "SinePeriod";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.float64 = calibration->sineperiod;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->stepbetween != 0.0)
  {
    keyp[idx] = "StepBetween";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.float64 = calibration->stepbetween;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->inputchannel[0])
  {
    keyp[idx] = "InputChannel";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->inputchannel;
    item[idx].length = strlen(calibration->inputchannel);
    idx++;
  }
  if (calibration->refamplitude != 0.0)
  {
    keyp[idx] = "ReferenceAmplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.float64 = calibration->refamplitude;
    item[idx].length = 0;
    idx++;
  }
  if (calibration->coupling[0])
  {
    keyp[idx] = "Coupling";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->coupling;
    item[idx].length = strlen(calibration->coupling);
    idx++;
  }
  if (calibration->rolloff[0])
  {
    keyp[idx] = "Rolloff";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->rolloff;
    item[idx].length = strlen(calibration->rolloff);
    idx++;
  }
  if (calibration->noise[0])
  {
    keyp[idx] = "Noise";
    item[idx].type = CBOR_TEXT;
    item[idx].value.ucharp = (unsigned char *)calibration->noise;
    item[idx].length = strlen(calibration->noise);
    idx++;
  }

  keyp[idx] = NULL;
  itemp[idx] = NULL;

  while (idx > 0)
  {
    idx--;
    itemp[idx] = &item[idx];
  }

  cbor_init (&stream, msr->extra, msr->extralength);

  /* Append Map entry to Array at specified or default path */
  if (!cbor_append_map_array (&stream, keyp, itemp,
                              (path) ? path : (const char *[]){"FDSN", "Calibration", NULL}))
    return MS_GENERROR;

  msr->extra = stream.data;
  msr->extralength = stream.size;

  return 0;
#undef MAXITEMS
} /* End of mseh_add_calibration() */

/***************************************************************************
 * mseh_add_timing_exception:
 *
 * Add specified timing exception to the extra headers of the given record.
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_timing_exception (MS3Record *msr, MSEHTimingException *exception,
                           const char *path[])
{
#define MAXITEMS 7
  cbor_stream_t stream;
  cbor_item_t item[MAXITEMS];
  cbor_item_t *itemp[MAXITEMS];
  const char *keyp[MAXITEMS];
  char timestr[31];
  char *cp;
  int idx = 0;

  if (!msr)
    return MS_GENERROR;

  if (exception->vcocorrection >= 0.0)
  {
    keyp[idx] = "VCOCorrection";
    item[idx].type = CBOR_FLOAT64;
    item[idx].valuetype = FLOAT64T;
    item[idx].value.float64 = exception->vcocorrection;
    item[idx].length = 0;
    idx++;
  }
  if (exception->time != NSTERROR)
  {
    cp = ms_nstime2isotimestr (exception->time, timestr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "Time";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)timestr;
    item[idx].length = strlen(timestr);
    idx++;
  }
  if (exception->receptionquality >= 0)
  {
    keyp[idx] = "ReceptionQuality";
    item[idx].type = CBOR_UINT;
    item[idx].valuetype = INT64T;
    item[idx].value.int64 = exception->receptionquality;
    item[idx].length = 0;
    idx++;
  }
  if (exception->count > 0)
  {
    keyp[idx] = "Count";
    item[idx].type = CBOR_UINT;
    item[idx].valuetype = INT64T;
    item[idx].value.int64 = exception->count;
    item[idx].length = 0;
    idx++;
  }
  if (exception->type[0])
  {
    keyp[idx] = "Type";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)exception->type;
    item[idx].length = strlen(exception->type);
    idx++;
  }
  if (exception->clockstatus[0])
  {
    keyp[idx] = "ClockStatus";
    item[idx].type = CBOR_TEXT;
    item[idx].valuetype = UCHARP;
    item[idx].value.ucharp = (unsigned char *)exception->clockstatus;
    item[idx].length = strlen(exception->clockstatus);
    idx++;
  }

  keyp[idx] = NULL;
  itemp[idx] = NULL;

  while (idx > 0)
  {
    idx--;
    itemp[idx] = &item[idx];
  }

  cbor_init (&stream, msr->extra, msr->extralength);

  /* Append Map entry to Array at specified or default path */
  if (!cbor_append_map_array (&stream, keyp, itemp,
                              (path) ? path : (const char *[]){"FDSN", "Time", "Exception", NULL}))
    return MS_GENERROR;

  msr->extra = stream.data;
  msr->extralength = stream.size;

  return 0;
#undef MAXITEMS
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
  char output[65535];
  int length;
  int idx;
  int instring = 0;

  if (!msr)
    return MS_GENERROR;

  if (!msr->extralength)
    return MS_NOERROR;

  length = mseh_to_json (msr, output, sizeof(output));

  if (!length)
  {
    ms_log (1, "Warning, something is wrong, JSON-like output from mseh_to_json() is empty\n");
    return MS_GENERROR;
  }

  if ( output[0] != '{' || output[length-1] != '}')
  {
    ms_log (1, "Warning, something is wrong, JSON-like output from mseh_to_json() is not an {object}\n");
  }

  /* Print "diganostic"/JSON-like output character-by-character, inserting
   * indentation, spaces and newlines for readability. */
  ms_log (0, "%*s", indent, "");
  for (idx = 1; idx < (length - 1); idx++)
  {
    /* Toggle "in string" flag for double quotes */
    if (output[idx] == '"')
      instring = (instring) ? 0 : 1;

    if (!instring)
    {
      if ( output[idx] == ':')
      {
        ms_log (0, ": ");
      }
      else if ( output[idx] == ',')
      {
        ms_log (0, ",\n%*s", indent, "");
      }
      else if ( output[idx] == '{' )
      {
        indent += 2;
        ms_log (0, "{\n%*s", indent, "");
      }
      else if ( output[idx] == '[' )
      {
        indent += 2;
        ms_log (0, "[\n%*s", indent, "");
      }
      else if ( output[idx] == '}' )
      {
        indent -= 2;
        ms_log (0, "\n%*s}", indent, "");
      }
      else if ( output[idx] == ']' )
      {
        indent -= 2;
        ms_log (0, "\n%*s]", indent, "");
      }
      else
      {
        ms_log (0, "%c", output[idx]);
      }
    }
    else
    {
      ms_log (0, "%c", output[idx]);
    }
  }
  ms_log (0, "\n");

  return MS_NOERROR;
} /* End of mseh_print() */

/***************************************************************************
 * mseh_to_json:
 *
 * Generate CBOR diagnostic/JSON-like output of the extra header
 * (CBOR Map) structure for the specified MS3Record.
 *
 * The output buffer is guaranteed to be terminated.
 *
 * Returns number of bytes written to output on success, otherwise a
 * negative libmseed error code.
 ***************************************************************************/
int
mseh_to_json (MS3Record *msr, char *output, int outputlength)
{
  cbor_stream_t stream;
  size_t readsize;
  size_t offset = 0;
  size_t outputoffset = 0;

  if (!msr || !output)
    return MS_GENERROR;

  cbor_init (&stream, msr->extra, msr->extralength);
  output[outputoffset] = '\0';

  /* Traverse CBOR items and build diagnostic/JSON-like output */
  while (offset < stream.size)
  {
    offset += readsize = cbor_map_to_diag (&stream, offset, 60, output, &outputoffset, outputlength);

    if (readsize == 0)
    {
      ms_log (2, "mseh_to_json(): Cannot decode CBOR, game over\n");
      return MS_GENERROR;
    }
  }

  output[outputoffset] = '\0';

  return (int)outputoffset;
} /* End of mseh_to_json() */

/***************************************************************************
 * mseh_print_raw:
 *
 * Print the extra header (CBOR Map) structure in the specified buffer.
 *
 * Returns number of bytes printed on success, otherwise a negative
 * libmseed error code.
 ***************************************************************************/
int
mseh_print_raw (unsigned char *cbor, size_t length)
{
  cbor_stream_t stream;
  char output[65535];
  size_t readsize;
  size_t offset = 0;
  size_t outputoffset = 0;

  if (!cbor)
    return MS_GENERROR;

  cbor_init (&stream, cbor, length);
  output[outputoffset] = '\0';

  /* Traverse CBOR items and build diagnostic/JSON-like output */
  while (offset < stream.size)
  {
    offset += readsize = cbor_map_to_diag (&stream, offset, 60, output, &outputoffset, sizeof(output));

    if (readsize == 0)
    {
      ms_log (2, "mseh_to_json(): Cannot decode CBOR, game over\n");
      return MS_GENERROR;
    }
  }

  output[outputoffset] = '\0';

  printf ("%s\n", output);

  return (int)outputoffset;
} /* End of mseh_print_raw() */
