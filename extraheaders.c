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
      *((int64_t*)value) = vitem.value.i;
  }
  else if (type == 'd' && (vitem.type == CBOR_FLOAT16 || vitem.type == CBOR_FLOAT32 || vitem.type == CBOR_FLOAT64))
  {
    if (value)
      *((double*)value) = vitem.value.d;
  }
  else if (type == 'c' && (vitem.type == CBOR_BYTES || vitem.type == CBOR_TEXT))
  {
    if (value)
    {
      /* Copy buffer and terminate */
      if (length > vitem.length)
      {
        memcpy (value, vitem.value.c, vitem.length);
        ((uint8_t *)value)[vitem.length] = '\0';
      }
      else
      {
        memcpy (value, vitem.value.c, length - 1);
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
    vitem.value.i = *((int64_t *)value);
    break;
  case 'd':
    vitem.type = CBOR_FLOAT64;
    vitem.value.d = *((double *)value);
    break;
  case 'c':
    vitem.type = CBOR_TEXT;
    vitem.value.c = (unsigned char *)value;
    vitem.length = length;
    break;
  case 'b':
    vitem.type = (*((int *)value)) ? CBOR_TRUE : CBOR_FALSE;
    vitem.value.i = (vitem.type == CBOR_TRUE) ? 1 : 0;
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
 * Each of the content values is optional, if a specific value is
 * passed it will not be included in the extra header structure.
 *
 * (char *) type                Detector type (e.g. "Murdock"), NULL = not included
 * (char *) detector            Detector name, NULL = not included
 * (double) signalamplitude     SignalAmplitude, 0.0 = not included
 * (double) signalperiod        Signal period, 0.0 = not included
 * (double) backgroundestimate  Background estimate, 0.0 = not included
 * (char *) detectionwave       Detection wave (e.g. "Dilitation"), NULL = not included
 * (nstime_t) onsettime         Onset time, NSTERROR = not included
 * (double) snr                 Signal to noise ratio, 0.0 = not included
 * (int) medlookback            Murdock event detection lookback value, -1 = not included
 * (int) medpickalgorithm       Murdock event detection pick algoritm, -1 = not included
 * (const char *path[])         Path to detection Array, if NULL {"FDSN", "Event", "Detection"}
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_event_detection (MS3Record *msr, char *type, char *detector,
                          double signalamplitude, double signalperiod,
                          double backgroundestimate, char *detectionwave,
                          nstime_t onsettime, double snr,
                          int medlookback, int medpickalgorithm,
                          const char *path[])
{
#define MAXITEMS 11
  cbor_stream_t stream;
  cbor_item_t item[MAXITEMS];
  cbor_item_t *itemp[MAXITEMS];
  const char *keyp[MAXITEMS];
  char onsetstr[31];
  char *cp;
  int idx = 0;

  if (!msr)
    return MS_GENERROR;

  if (type)
  {
    keyp[idx] = "Type";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)type;
    item[idx].length = strlen(type);
    idx++;
  }
  if (detector)
  {
    keyp[idx] = "Detector";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)detector;
    item[idx].length = strlen(detector);
    idx++;
  }
  if (signalamplitude != 0.0)
  {
    keyp[idx] = "SignalAmplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = signalamplitude;
    item[idx].length = 0;
    idx++;
  }
  if (signalperiod != 0.0)
  {
    keyp[idx] = "SignalPeriod";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = signalperiod;
    item[idx].length = 0;
    idx++;
  }
  if (backgroundestimate != 0.0)
  {
    keyp[idx] = "BackgroundEstimate";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = backgroundestimate;
    item[idx].length = 0;
    idx++;
  }
  if (detectionwave)
  {
    keyp[idx] = "DetectionWave";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)detectionwave;
    item[idx].length = strlen(detectionwave);
    idx++;
  }
  if (onsettime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (onsettime, onsetstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "OnsetTime";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)onsetstr;
    item[idx].length = strlen (onsetstr);
    idx++;
  }
  if (snr != 0.0)
  {
    keyp[idx] = "SNR";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = snr;
    item[idx].length = 0;
    idx++;
  }
  if (medlookback >= 0)
  {
    keyp[idx] = "MEDLookback";
    item[idx].type = CBOR_UINT;
    item[idx].value.i = medlookback;
    item[idx].length = 0;
    idx++;
  }
  if (medpickalgorithm >= 0)
  {
    keyp[idx] = "MEDPickAlgorithm";
    item[idx].type = CBOR_UINT;
    item[idx].value.i = medpickalgorithm;
    item[idx].length = 0;
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
 * Each of the content values is optional, if a specific value is
 * passed it will not be included in the extra header structure.
 *
 * (char *) type                Detector type (e.g. "STEP", "SINE", "PSEUDORANDOM"), NULL = not included
 * (nstime_t) begintime         Begin time, NSTERROR = not included
 * (nstime_t) endtime           End time, NSTERROR = not included
 * (int) steps                  Number of step calibrations, -1 = not included
 * (int) firstpulsepositive     Boolean, step cal. first pulse, -1 = not included
 * (int) alternatesign          Boolean, step cal. alt. sign, -1 = not included
 * (char *) trigger             Trigger, e.g. AUTOMATIC or MANUAL, NULL = not included
 * (int) continued              Boolean, continued from prev. record, -1 = not included
 * (double) amplitude           Amp. of calibration signal, 0.0 = not included
 * (char *) inputunits          Units of input (e.g. volts, amps), NULL = not included
 * (char *) amplituderange      E.g PEAKTOPTEAK, ZEROTOPEAK, RMS, RANDOM, NULL = not included
 * (double) duration            Cal. duration in seconds, 0.0 = not included
 * (double) sineperiod          Period of sine, 0.0 = not included
 * (double) stepbetween         Interval bewteen steps, 0.0 = not included
 * (char *) inputchannel        Channel of input, NULL = not included
 * (double) refamplitude        Reference amplitude, 0.0 = not included
 * (char *) coupling            Coupling, e.g. Resistive, Capacitive, NULL = not included
 * (char *) rolloff             Rolloff of filters, NULL = not included
 * (char *) noise               Noise for PR cals, e.g. White or Red, NULL = not included
 * (const char *path[])         Path to detection Array, if NULL {"FDSN", "Calibration"}
 *
 * Returns:
 *   0 on success,
 *   otherwise a (negative) libmseed error code.
 ***************************************************************************/
int
mseh_add_calibration (MS3Record *msr, char *type,
                      nstime_t begintime, nstime_t endtime,
                      int steps, int firstpulsepositive, int alternatesign,
                      char *trigger, int continued, double amplitude,
                      char *inputunits, char *amplituderange,
                      double duration, double sineperiod, double stepbetween,
                      char *inputchannel, double refamplitude,
                      char *coupling, char *rolloff, char *noise,
                      const char *path[])
{
#define MAXITEMS 20
  cbor_stream_t stream;
  cbor_item_t item[MAXITEMS];
  cbor_item_t *itemp[MAXITEMS];
  const char *keyp[MAXITEMS];
  char beginstr[30];
  char endstr[30];
  char *cp;
  int idx = 0;

  if (!msr)
    return MS_GENERROR;

  if (type)
  {
    keyp[idx] = "Type";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)type;
    item[idx].length = strlen(type);
    idx++;
  }
  if (begintime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (begintime, beginstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "BeginTime";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)beginstr;
    item[idx].length = strlen(beginstr);
    idx++;
  }
  if (endtime != NSTERROR)
  {
    cp = ms_nstime2isotimestr (endtime, endstr, -1);
    while (*cp)
      cp++;
    *cp++ = 'Z';
    *cp = '\0';

    keyp[idx] = "EndTime";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)endstr;
    item[idx].length = strlen(endstr);
    idx++;
  }
  if (steps >= 0)
  {
    keyp[idx] = "Steps";
    item[idx].type = CBOR_UINT;
    item[idx].value.i = steps;
    item[idx].length = 0;
    idx++;
  }
  if (firstpulsepositive >= 0)
  {
    keyp[idx] = "FirstPulsePositive";
    item[idx].type = (firstpulsepositive) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.i = (firstpulsepositive) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (alternatesign >= 0)
  {
    keyp[idx] = "AlternateSign";
    item[idx].type = (alternatesign) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.i = (alternatesign) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (trigger)
  {
    keyp[idx] = "Trigger";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)trigger;
    item[idx].length = strlen(trigger);
    idx++;
  }
  if (continued >= 0)
  {
    keyp[idx] = "Continued";
    item[idx].type = (continued) ? CBOR_TRUE : CBOR_FALSE;
    item[idx].value.i = (continued) ? 1 : 0;
    item[idx].length = 0;
    idx++;
  }
  if (amplitude != 0.0)
  {
    keyp[idx] = "Amplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = amplitude;
    item[idx].length = 0;
    idx++;
  }
  if (inputunits)
  {
    keyp[idx] = "InputUnits";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)inputunits;
    item[idx].length = strlen(inputunits);
    idx++;
  }
  if (amplituderange)
  {
    keyp[idx] = "AmplitudeRange";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)amplituderange;
    item[idx].length = strlen(amplituderange);
    idx++;
  }
  if (duration != 0.0)
  {
    keyp[idx] = "Duration";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = duration;
    item[idx].length = 0;
    idx++;
  }
  if (sineperiod != 0.0)
  {
    keyp[idx] = "SinePeriod";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = sineperiod;
    item[idx].length = 0;
    idx++;
  }
  if (stepbetween != 0.0)
  {
    keyp[idx] = "StepBetween";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = stepbetween;
    item[idx].length = 0;
    idx++;
  }
  if (inputchannel)
  {
    keyp[idx] = "InputChannel";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)inputchannel;
    item[idx].length = strlen(inputchannel);
    idx++;
  }
  if (refamplitude != 0.0)
  {
    keyp[idx] = "ReferenceAmplitude";
    item[idx].type = CBOR_FLOAT64;
    item[idx].value.d = refamplitude;
    item[idx].length = 0;
    idx++;
  }
  if (coupling)
  {
    keyp[idx] = "Coupling";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)coupling;
    item[idx].length = strlen(coupling);
    idx++;
  }
  if (rolloff)
  {
    keyp[idx] = "Rolloff";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)rolloff;
    item[idx].length = strlen(rolloff);
    idx++;
  }
  if (noise)
  {
    keyp[idx] = "Noise";
    item[idx].type = CBOR_TEXT;
    item[idx].value.c = (unsigned char *)noise;
    item[idx].length = strlen(noise);
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
