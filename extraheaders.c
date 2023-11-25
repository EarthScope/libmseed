/***************************************************************************
 * Routines for dealing with libmseed extra headers stored as JSON.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2023 Chad Trabant, EarthScope Data Services
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

#include "libmseed.h"
#include "extraheaders.h"

/* Private allocation wrappers for yyjson's allocator definition */
void *_priv_malloc(void *ctx, size_t size) {
    UNUSED(ctx);
    return libmseed_memory.malloc(size);
}

void *_priv_realloc(void *ctx, void *ptr, size_t oldsize, size_t size) {
    UNUSED(ctx);
    UNUSED(oldsize);
    return libmseed_memory.realloc(ptr, size);
}

void _priv_free(void *ctx, void *ptr) {
    UNUSED(ctx);
    libmseed_memory.free(ptr);
}

/***************************************************************************
 * Internal routine to parse JSON strings into internal state storage.
 *
 * The state container is allocated if needed and if JSON is supplied,
 * it is parsed into the immutable form.
 *
 * @param[in] jsonstring JSON string to parse
 * @param[in] length Length of JSON string
 * @param[in] parsed Internal parsed state
 *
 * @returns A LM_PARSED_JSON* on success or NULL on error
 *
 * \sa mseh_free_parsestate()
 ***************************************************************************/
static LM_PARSED_JSON*
parse_json (char *jsonstring, size_t length, LM_PARSED_JSON *parsed)
{
  yyjson_read_flag flg = YYJSON_READ_NOFLAG;
  yyjson_read_err err;
  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};

  /* Allocate parsed state if needed */
  if (!parsed)
  {
    if ((parsed = libmseed_memory.malloc (sizeof (LM_PARSED_JSON))) == NULL)
    {
      ms_log (2, "%s() Cannot allocate memory for internal JSON parsing state\n", __func__);
      return NULL;
    }
    else
    {
      parsed->doc     = NULL;
      parsed->mut_doc = NULL;
    }
  }

  /* Nothing to parse */
  if (jsonstring == NULL || length == 0)
  {
    return parsed;
  }

  /* Free existing immutable doc */
  if (parsed->doc)
  {
    yyjson_doc_free (parsed->doc);
    parsed->doc     = NULL;
  }

  /* Free existing mutable document */
  if (parsed->mut_doc != NULL)
  {
    yyjson_mut_doc_free (parsed->mut_doc);
    parsed->mut_doc = NULL;
  }

  /* Parse JSON into immutable form */
  if ((parsed->doc = yyjson_read_opts (jsonstring, length, flg, &alc, &err)) == NULL)
  {
    ms_log (2, "%s() Cannot parse extra header JSON: %s\n",
            __func__, (err.msg) ? err.msg : "Unknown error");
    return NULL;
  }

  return parsed;
}

/**********************************************************************/ /**
 * @brief Search for and return an extra header value.
 *
 * The extra header value is specified as a JSON Pointer (RFC 6901), e.g.
 * \c '/objectA/objectB/header'.
 *
 * This routine can get used to test for the existence of a value
 * without returning the value by setting \a value to NULL.
 *
 * If the target item is found (and \a value parameter is set) the
 * value will be copied into the memory specified by \c value.  The
 * \a type value specifies the data type expected.
 *
 * If a \a parsestate pointer is supplied, the parsed (deserialized) JSON
 * data are stored here.  This value may be used in subsequent calls to
 * avoid re-parsing the JSON.  The data must be freed with
 * mseh_free_parsestate() when done reading the JSON.  If this value
 * is NULL the parse state will be created and destroyed on each call.
 *
 * @param[in] msr Parsed miniSEED record to search
 * @param[in] ptr Header value desired, as JSON Pointer
 * @param[out] value Buffer for value, of type \c type
 * @param[in] type Type of value expected, one of:
 * @parblock
 * - \c 'n' - \a value is type \a double
 * - \c 'i' - \a value is type \a int64_t
 * - \c 's' - \a value is type \a char* (maximum length is: \c maxlength - 1)
 * - \c 'b' - \a value of type \a int (boolean value of 0 or 1)
 * @endparblock
 * @param[in] maxlength Maximum length of string value
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @retval 0 on success
 * @retval 1 when the value was not found
 * @retval 2 when the value is of a different type
 * @returns A (negative) libmseed error code on error
 *
 * \ref MessageOnError - this function logs a message on error
 *
 * \sa mseh_free_parsestate()
 ***************************************************************************/
int
mseh_get_ptr_r (const MS3Record *msr, const char *ptr,
                 void *value, char type, size_t maxlength,
                 LM_PARSED_JSON **parsestate)
{
  LM_PARSED_JSON *parsed  = (parsestate) ? *parsestate : NULL;

  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};
  yyjson_val *extravalue  = NULL;
  const char *stringvalue = NULL;

  int retval = 0;

  if (!msr || !ptr)
  {
    ms_log (2, "%s() Required argument not defined: 'msr' or 'ptr'\n", __func__);
    return MS_GENERROR;
  }

  /* Nothing can be found in no headers */
  if (!msr->extralength)
  {
    return 1;
  }

  if (!msr->extra)
  {
    ms_log (2, "%s() Expected extra headers (msr->extra) are not present\n", __func__);
    return MS_GENERROR;
  }

  /* Detect invalid JSON Pointer, i.e. with no root '/' designation */
  if (ptr[0] != '/')
  {
    ms_log (2, "%s() Unsupported ptr notation: %s\n", __func__, ptr);
    return MS_GENERROR;
  }

  /* Parse JSON extra headers if not available in state */
  if (parsed == NULL)
  {
    /* Parse to immutable state */
    parsed = parse_json(msr->extra, msr->extralength, parsed);

    if (parsed == NULL)
    {
      return MS_GENERROR;
    }

    /* Set supplied state pointer */
    if (parsestate != NULL)
    {
      *parsestate = parsed;
    }
  }

  /* Create immutable document from mutable if needed */
  if (parsed->mut_doc != NULL && parsed->doc == NULL)
  {
    if ((parsed->doc = yyjson_mut_doc_imut_copy (parsed->mut_doc, &alc)) == NULL)
    {
      ms_log (2, "%s() Cannot create immutable document from mutable\n", __func__);

      if (parsestate == NULL)
      {
        mseh_free_parsestate (&parsed);
      }

      return MS_GENERROR;
    }
  }

  /* Get target value */
  extravalue = yyjson_doc_ptr_get(parsed->doc, ptr);

  if (extravalue == NULL)
  {
    retval = 1;
  }
  else if (type == 'n' && yyjson_is_num (extravalue))
  {
    if (value)
      *((double *)value) = unsafe_yyjson_get_num (extravalue);
  }
  else if (type == 'i' && yyjson_is_int (extravalue))
  {
    if (value)
      *((int64_t *)value) = unsafe_yyjson_get_int (extravalue);
  }
  else if (type == 's' && yyjson_is_str (extravalue))
  {
    if (value)
    {
      stringvalue = unsafe_yyjson_get_str (extravalue);
      strncpy ((char *)value, stringvalue, maxlength - 1);
      ((char *)value)[maxlength - 1] = '\0';
    }
  }
  else if (type == 'b' && yyjson_is_bool(extravalue))
  {
    if (value)
      *((int *)value) = (unsafe_yyjson_get_bool (extravalue)) ? 1 : 0;
  }
  /* Return wrong type indicator if a value was requested */
  else if (value)
  {
    retval = 2;
  }

  /* Free parse state if not being retained */
  if (parsestate == NULL)
  {
    mseh_free_parsestate (&parsed);
  }

  return retval;
} /* End of mseh_get_ptr_r() */

/**********************************************************************/ /**
 * @brief Set the value of extra header values
 *
 * The extra header value is specified as a JSON Pointer (RFC 6901), e.g.
 * \c '/objectA/objectB/header'.
 *
 * For most value types, if the \a ptr or final header values do not exist
 * they will be created.  If the header value exists it will be replaced.
 * When the value type is 'M', for Merge Patch (RFC 7386), the location
 * indicated by \a ptr must exist.
 *
 * The \a type value specifies the data type expected for \c value.
 *
 * If a \a parsestate pointer is supplied, the parsed (deserialized) JSON
 * data are stored here.  This value may be used in subsequent calls to
 * avoid re-parsing the JSON.  When done setting headers using this
 * functionality the following \a must be done:
 * 1. call mseh_serialize() to create the JSON headers before writing the record
 * 2. free the \a parsestate data with mseh_free_parsestate()
 * If this value is NULL the parse state will be created and destroyed
 * on each call.
 *
 * @param[in] msr Parsed miniSEED record to modify
 * @param[in] ptr Header value to set as JSON Pointer, or JSON Merge Patch
 * @param[in] value Buffer for value, of type \c type
 * @param[in] type Type of value expected, one of:
 * @parblock
 * - \c 'n' - \a value is type \a double
 * - \c 'i' - \a value is type \a int64_t
 * - \c 's' - \a value is type \a char*
 * - \c 'b' - \a value is type \a int (boolean value of 0 or 1)
 * - \c 'M' - \a value is type \a char* and a Merge Patch to apply at \a ptr
 * - \c 'V' - \a value is type \a yyjson_mut_val* to _set/replace_ (internal use)
 * - \c 'A' - \a value is type \a yyjson_mut_val* to _append to array_ (internal use)
 * @endparblock
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @retval 0 on success, otherwise a (negative) libmseed error code.
 *
 * \ref MessageOnError - this function logs a message on error
 *
 * \sa mseh_free_parsestate()
 * \sa mseh_serialize()
 ***************************************************************************/
int
mseh_set_ptr_r (MS3Record *msr, const char *ptr,
                 void *value, char type,
                 LM_PARSED_JSON **parsestate)
{
  LM_PARSED_JSON *parsed = (parsestate) ? *parsestate : NULL;
  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};
  yyjson_doc *patch_idoc = NULL;
  yyjson_mut_doc *patch_doc = NULL;
  yyjson_mut_val *merged_val = NULL;
  yyjson_mut_val *target_val = NULL;
  yyjson_mut_val *array_val = NULL;
  bool rv = false;

  if (!msr || !ptr || !value)
  {
    ms_log (2, "%s() Required argument not defined: 'msr', 'ptr', or 'value'\n", __func__);
    return MS_GENERROR;
  }

  /* Detect invalid JSON Pointer, i.e. with no root '/' designation */
  if (ptr[0] != '/' && type != 'M')
  {
    ms_log (2, "%s() Unsupported JSON Pointer notation: %s\n", __func__, ptr);
    return MS_GENERROR;
  }

  /* Parse JSON extra headers if not available in state */
  if (parsed == NULL)
  {
    /* Allocate state container and parse to immutable form */
    parsed = parse_json (msr->extra, msr->extralength, parsed);

    if (parsed == NULL)
    {
      return MS_GENERROR;
    }

    /* Set supplied state pointer */
    if (parsestate != NULL)
    {
      *parsestate = parsed;
    }
  }

  /* Generate mutable document from immutable form if needed */
  if (parsed->doc != NULL && parsed->mut_doc == NULL)
  {
    if ((parsed->mut_doc = yyjson_doc_mut_copy (parsed->doc, &alc)) == NULL)
    {
      ms_log (2, "%s() Cannot create mutable JSON document\n", __func__);

      if (parsestate == NULL)
      {
        mseh_free_parsestate (&parsed);
      }

      return MS_GENERROR;
    }
  }

  /* Initialize empty mutable document if needed */
  if (parsed->mut_doc == NULL)
  {
    if ((parsed->mut_doc = yyjson_mut_doc_new (&alc)) == NULL)
    {
      ms_log (2, "%s() Cannot initialize mutable JSON document\n", __func__);

      if (parsestate == NULL)
      {
        mseh_free_parsestate (&parsed);
      }

      return MS_GENERROR;
    }
  }

  /* Set (or replace) header value at ptr */
  switch (type)
  {
  case 'n':
    rv = yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr,
                                 yyjson_mut_real (parsed->mut_doc, *((double *)value)));
    break;
  case 'i':
    rv = yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr,
                                 yyjson_mut_sint (parsed->mut_doc, *((int64_t *)value)));
    break;
  case 's':
    rv = yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr,
                                 yyjson_mut_strcpy (parsed->mut_doc, (const char *)value));
    break;
  case 'b':
    rv = yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr,
                                 yyjson_mut_bool (parsed->mut_doc, *((int *)value) ? true : false));
    break;
  case 'M':
    /* Parse supplied patch */
    if ((patch_idoc = yyjson_read_opts ((char *)value, strlen ((char *)value), 0, &alc, NULL)))
    {
      if ((patch_doc = yyjson_doc_mut_copy (patch_idoc, &alc)))
      {
        /* Get patch target value */
        if ((target_val = yyjson_mut_doc_ptr_get (parsed->mut_doc, ptr)))
        {
          /* Generate merged value */
          if ((merged_val = yyjson_mut_merge_patch (parsed->mut_doc,
                                                    target_val,
                                                    yyjson_mut_doc_get_root (patch_doc))))
          {
            /* Replace value at pointer with merged value */
            rv = yyjson_mut_doc_ptr_replace (parsed->mut_doc, ptr, merged_val);
          }
        }
      }
    }
    else
    {
      ms_log (2, "%s() Cannot parse JSON Merge Patch: %s'\n", __func__, (char *)value);
    }

    yyjson_doc_free(patch_idoc);
    yyjson_mut_doc_free (patch_doc);

    break;
  case 'V':
    rv = yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr,
                                 yyjson_mut_val_mut_copy (parsed->mut_doc, (yyjson_mut_val *)value));
    break;
  case 'A':
    /* Search for existing array, create if necessary */
    if ((array_val = yyjson_mut_doc_ptr_get (parsed->mut_doc, ptr)) == NULL)
    {
      if ((array_val = yyjson_mut_arr (parsed->mut_doc)))
      {
        if (yyjson_mut_doc_ptr_set (parsed->mut_doc, ptr, array_val) == false)
        {
          array_val = NULL;
        }
      }
    }

    /* Append supplied value, will return false if array_val == NULL */
    rv = yyjson_mut_arr_append (array_val,
                                yyjson_mut_val_mut_copy (parsed->mut_doc, (yyjson_mut_val *)value));
    break;
  default:
    ms_log (2, "%s() Unrecognized value type '%d'\n", __func__, type);
    rv = false;
  }

  /* Serialized extra headers and free parse state if not being retained */
  if (parsestate == NULL)
  {
    mseh_serialize (msr, &parsed);
    mseh_free_parsestate (&parsed);
  }
  /* If changes were applied, the immutable form of the document is now invalid */
  else if (rv ==true && parsed->doc != NULL)
  {
    yyjson_doc_free (parsed->doc);
    parsed->doc = NULL;
  }

  return (rv == true) ? 0 : MS_GENERROR;
} /* End of mseh_set_ptr_r() */

/**********************************************************************/ /**
 * @brief Add event detection to the extra headers of the given record.
 *
 * If \a ptr is NULL, the default is \c '/FDSN/Event/Detection'.
 *
 * @param[in] msr Parsed miniSEED record to query
 * @param[in] ptr Header value desired, specified in dot notation
 * @param[in] eventdetection Structure with event detection values
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @returns 0 on success, otherwise a (negative) libmseed error code.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mseh_add_event_detection_r (MS3Record *msr, const char *ptr,
                            MSEHEventDetection *eventdetection,
                            LM_PARSED_JSON **parsestate)
{
  yyjson_mut_val entry;
  yyjson_mut_val type_key, type;
  yyjson_mut_val sigamp_key, sigamp;
  yyjson_mut_val sigper_key, sigper;
  yyjson_mut_val bgest_key, bgest;
  yyjson_mut_val wave_key, wave;
  yyjson_mut_val units_key, units;
  yyjson_mut_val onset_key, onset;
  yyjson_mut_val medsnr_key, medsnr;
  yyjson_mut_val medsnr_value[6];
  yyjson_mut_val medlookback_key, medlookback;
  yyjson_mut_val medpickalg_key, medpickalg;
  yyjson_mut_val detector_key, detector;

  char timestring[40];
  int idx;

  if (!msr || !eventdetection)
  {
    ms_log (2, "%s() Required argument not defined: 'msr' or 'eventdetection'\n", __func__);
    return MS_GENERROR;
  }

  yyjson_mut_set_obj (&entry);

  /* Add elements to new object */
  if (eventdetection->type[0])
  {
    yyjson_mut_set_str (&type_key, "Type");
    yyjson_mut_set_str (&type, eventdetection->type);
    yyjson_mut_obj_add (&entry, &type_key, &type);
  }
  if (eventdetection->signalamplitude != 0.0)
  {
    yyjson_mut_set_str (&sigamp_key, "SignalAmplitude");
    yyjson_mut_set_real (&sigamp, eventdetection->signalamplitude);
    yyjson_mut_obj_add (&entry, &sigamp_key, &sigamp);
  }
  if (eventdetection->signalperiod != 0.0)
  {
    yyjson_mut_set_str (&sigper_key, "SignalPeriod");
    yyjson_mut_set_real (&sigper, eventdetection->signalperiod);
    yyjson_mut_obj_add (&entry, &sigper_key, &sigper);
  }
  if (eventdetection->backgroundestimate != 0.0)
  {
    yyjson_mut_set_str (&bgest_key, "BackgroundEstimate");
    yyjson_mut_set_real (&bgest, eventdetection->backgroundestimate);
    yyjson_mut_obj_add (&entry, &bgest_key, &bgest);
  }
  if (eventdetection->wave[0])
  {
    yyjson_mut_set_str (&wave_key, "Wave");
    yyjson_mut_set_str (&wave, eventdetection->wave);
    yyjson_mut_obj_add (&entry, &wave_key, &wave);
  }
  if (eventdetection->units[0])
  {
    yyjson_mut_set_str (&units_key, "Units");
    yyjson_mut_set_str (&units, eventdetection->units);
    yyjson_mut_obj_add (&entry, &units_key, &units);
  }
  if (eventdetection->onsettime != NSTERROR && eventdetection->onsettime != NSTUNSET)
  {
    if (ms_nstime2timestr (eventdetection->onsettime, timestring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&onset_key, "OnsetTime");
      yyjson_mut_set_str (&onset, timestring);
      yyjson_mut_obj_add (&entry, &onset_key, &onset);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %"PRId64"\n", __func__, eventdetection->onsettime);
      return MS_GENERROR;
    }
  }
  if (memcmp (eventdetection->medsnr, (uint8_t[]){0, 0, 0, 0, 0, 0}, 6))
  {
    yyjson_mut_set_str (&medsnr_key, "MEDSNR");
    yyjson_mut_set_arr (&medsnr);
    yyjson_mut_obj_add (&entry, &medsnr_key, &medsnr);

    /* Array containing the 6 SNR values */
    for (idx = 0; idx < 6; idx++)
    {
      yyjson_mut_set_uint (&(medsnr_value[idx]), eventdetection->medsnr[idx]);
      yyjson_mut_arr_append (&medsnr, &(medsnr_value[idx]));
    }
  }
  if (eventdetection->medlookback >= 0)
  {
    yyjson_mut_set_str (&medlookback_key, "MEDLookback");
    yyjson_mut_set_sint (&medlookback, eventdetection->medlookback);
    yyjson_mut_obj_add (&entry, &medlookback_key, &medlookback);
  }
  if (eventdetection->medpickalgorithm >= 0)
  {
    yyjson_mut_set_str (&medpickalg_key, "MEDPickAlgorithm");
    yyjson_mut_set_sint (&medpickalg, eventdetection->medpickalgorithm);
    yyjson_mut_obj_add (&entry, &medpickalg_key, &medpickalg);
  }
  if (eventdetection->detector[0])
  {
    yyjson_mut_set_str (&detector_key, "Detector");
    yyjson_mut_set_str (&detector, eventdetection->detector);
    yyjson_mut_obj_add (&entry, &detector_key, &detector);
  }

  /* Add new object to array, created 'value' will be free'd on successful return */
  if (mseh_set_ptr_r (msr, (ptr) ? ptr : "/FDSN/Event/Detection", &entry, 'A', parsestate))
  {
    return MS_GENERROR;
  }

  return 0;
} /* End of mseh_add_event_detection_r() */

/**********************************************************************/ /**
 * @brief Add calibration to the extra headers of the given record.
 *
 * If \a ptr is NULL, the default is \c '/FDSN/Calibration/Sequence'.
 *
 * @param[in] msr Parsed miniSEED record to query
 * @param[in] ptr Header value desired, specified in dot notation
 * @param[in] calibration Structure with calibration values
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @returns 0 on success, otherwise a (negative) libmseed error code.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mseh_add_calibration_r (MS3Record *msr, const char *ptr,
                        MSEHCalibration *calibration,
                        LM_PARSED_JSON **parsestate)
{
  yyjson_mut_val entry;
  yyjson_mut_val type_key, type;
  yyjson_mut_val begintime_key, begintime;
  yyjson_mut_val endtime_key, endtime;
  yyjson_mut_val steps_key, steps;
  yyjson_mut_val firstpp_key, firstpp;
  yyjson_mut_val altsign_key, altsign;
  yyjson_mut_val trigger_key, trigger;
  yyjson_mut_val continued_key, continued;
  yyjson_mut_val amplitude_key, amplitude;
  yyjson_mut_val inputunits_key, inputunits;
  yyjson_mut_val amprange_key, amprange;
  yyjson_mut_val duration_key, duration;
  yyjson_mut_val sineperiod_key, sineperiod;
  yyjson_mut_val stepbetween_key, stepbetween;
  yyjson_mut_val inputchannel_key, inputchannel;
  yyjson_mut_val refamp_key, refamp;
  yyjson_mut_val coupling_key, coupling;
  yyjson_mut_val rolloff_key, rolloff;
  yyjson_mut_val noise_key, noise;

  char beginstring[40];
  char endstring[40];

  if (!msr || !calibration)
  {
    ms_log (2, "%s() Required argument not defined: 'msr' or 'calibration'\n", __func__);
    return MS_GENERROR;
  }

  yyjson_mut_set_obj (&entry);

  /* Add elements to new object */
  if (calibration->type[0])
  {
    yyjson_mut_set_str (&type_key, "Type");
    yyjson_mut_set_str (&type, calibration->type);
    yyjson_mut_obj_add (&entry, &type_key, &type);
  }
  if (calibration->begintime != NSTERROR && calibration->begintime != NSTUNSET)
  {
    if (ms_nstime2timestr (calibration->begintime, beginstring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&begintime_key, "BeginTime");
      yyjson_mut_set_str (&begintime, beginstring);
      yyjson_mut_obj_add (&entry, &begintime_key, &begintime);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %" PRId64 "\n", __func__, calibration->begintime);
      return MS_GENERROR;
    }
  }
  if (calibration->endtime != NSTERROR && calibration->endtime != NSTUNSET)
  {
    if (ms_nstime2timestr (calibration->endtime, endstring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&endtime_key, "EndTime");
      yyjson_mut_set_str (&endtime, endstring);
      yyjson_mut_obj_add (&entry, &endtime_key, &endtime);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %" PRId64 "\n", __func__, calibration->endtime);
      return MS_GENERROR;
    }
  }
  if (calibration->steps >= 0)
  {
    yyjson_mut_set_str (&steps_key, "Steps");
    yyjson_mut_set_sint (&steps, calibration->steps);
    yyjson_mut_obj_add (&entry, &steps_key, &steps);
  }
  if (calibration->firstpulsepositive >= 0)
  {
    yyjson_mut_set_str (&firstpp_key, "StepFirstPulsePositive");
    yyjson_mut_set_bool (&firstpp, calibration->firstpulsepositive);
    yyjson_mut_obj_add (&entry, &firstpp_key, &firstpp);
  }
  if (calibration->alternatesign >= 0)
  {
    yyjson_mut_set_str (&altsign_key, "StepAlternateSign");
    yyjson_mut_set_bool (&altsign, calibration->alternatesign);
    yyjson_mut_obj_add (&entry, &altsign_key, &altsign);
  }
  if (calibration->trigger[0])
  {
    yyjson_mut_set_str (&trigger_key, "Trigger");
    yyjson_mut_set_str (&trigger, calibration->trigger);
    yyjson_mut_obj_add (&entry, &trigger_key, &trigger);
  }
  if (calibration->continued >= 0)
  {
    yyjson_mut_set_str (&continued_key, "Continued");
    yyjson_mut_set_bool (&continued, calibration->continued);
    yyjson_mut_obj_add (&entry, &continued_key, &continued);
  }
  if (calibration->amplitude != 0.0)
  {
    yyjson_mut_set_str (&amplitude_key, "Amplitude");
    yyjson_mut_set_real (&amplitude, calibration->amplitude);
    yyjson_mut_obj_add (&entry, &amplitude_key, &amplitude);
  }
  if (calibration->inputunits[0])
  {
    yyjson_mut_set_str (&inputunits_key, "InputUnits");
    yyjson_mut_set_str (&inputunits, calibration->inputunits);
    yyjson_mut_obj_add (&entry, &inputunits_key, &inputunits);
  }
  if (calibration->amplituderange[0])
  {
    yyjson_mut_set_str (&amprange_key, "AmplitudeRange");
    yyjson_mut_set_str (&amprange, calibration->amplituderange);
    yyjson_mut_obj_add (&entry, &amprange_key, &amprange);
  }
  if (calibration->duration != 0.0)
  {
    yyjson_mut_set_str (&duration_key, "Duration");
    yyjson_mut_set_real (&duration, calibration->duration);
    yyjson_mut_obj_add (&entry, &duration_key, &duration);
  }
  if (calibration->sineperiod != 0.0)
  {
    yyjson_mut_set_str (&sineperiod_key, "SinePeriod");
    yyjson_mut_set_real (&sineperiod, calibration->sineperiod);
    yyjson_mut_obj_add (&entry, &sineperiod_key, &sineperiod);
  }
  if (calibration->stepbetween != 0.0)
  {
    yyjson_mut_set_str (&stepbetween_key, "StepBetween");
    yyjson_mut_set_real (&stepbetween, calibration->stepbetween);
    yyjson_mut_obj_add (&entry, &stepbetween_key, &stepbetween);
  }
  if (calibration->inputchannel[0])
  {
    yyjson_mut_set_str (&inputchannel_key, "InputChannel");
    yyjson_mut_set_str (&inputchannel, calibration->inputchannel);
    yyjson_mut_obj_add (&entry, &inputchannel_key, &inputchannel);
  }
  if (calibration->refamplitude != 0.0)
  {
    yyjson_mut_set_str (&refamp_key, "ReferenceAmplitude");
    yyjson_mut_set_real (&refamp, calibration->refamplitude);
    yyjson_mut_obj_add (&entry, &refamp_key, &refamp);;
  }
  if (calibration->coupling[0])
  {
    yyjson_mut_set_str (&coupling_key, "Coupling");
    yyjson_mut_set_str (&coupling, calibration->coupling);
    yyjson_mut_obj_add (&entry, &coupling_key, &coupling);
  }
  if (calibration->rolloff[0])
  {
    yyjson_mut_set_str (&rolloff_key, "Rolloff");
    yyjson_mut_set_str (&rolloff, calibration->rolloff);
    yyjson_mut_obj_add (&entry, &rolloff_key, &rolloff);
  }
  if (calibration->noise[0])
  {
    yyjson_mut_set_str (&noise_key, "Noise");
    yyjson_mut_set_str (&noise, calibration->noise);
    yyjson_mut_obj_add (&entry, &noise_key, &noise);
  }

  /* Add new object to array, created 'value' will be free'd on successful return */
  if (mseh_set_ptr_r (msr, (ptr) ? ptr : "/FDSN/Calibration/Sequence", &entry, 'A', parsestate))
  {
    return MS_GENERROR;
  }

  return 0;
} /* End of mseh_add_calibration_r() */

/**********************************************************************/ /**
 * @brief Add timing exception to the extra headers of the given record.
 *
 * If \a ptr is NULL, the default is \c '/FDSN/Time/Exception'.
 *
 * @param[in] msr Parsed miniSEED record to query
 * @param[in] ptr Header value desired, specified in dot notation
 * @param[in] exception Structure with timing exception values
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @returns 0 on success, otherwise a (negative) libmseed error code.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mseh_add_timing_exception_r (MS3Record *msr, const char *ptr,
                             MSEHTimingException *exception,
                             LM_PARSED_JSON **parsestate)
{
  yyjson_mut_val entry;
  yyjson_mut_val etime_key, etime;
  yyjson_mut_val vcocorr_key, vcocorr;
  yyjson_mut_val recqual_key, recqual;
  yyjson_mut_val count_key, count;
  yyjson_mut_val type_key, type;
  yyjson_mut_val clockstatus_key, clockstatus;

  char timestring[40];

  if (!msr || !exception)
  {
    ms_log (2, "%s() Required argument not defined: 'msr' or 'exception'\n", __func__);
    return MS_GENERROR;
  }

  yyjson_mut_set_obj (&entry);

  /* Add elements to new object */
  if (exception->time != NSTERROR && exception->time != NSTUNSET)
  {
    if (ms_nstime2timestr (exception->time, timestring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&etime_key, "Time");
      yyjson_mut_set_str (&etime, timestring);
      yyjson_mut_obj_add (&entry, &etime_key, &etime);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %" PRId64 "\n", __func__, exception->time);
      return MS_GENERROR;
    }
  }
  if (exception->vcocorrection >= 0.0)
  {
    yyjson_mut_set_str (&vcocorr_key, "VCOCorrection");
    yyjson_mut_set_real (&vcocorr, exception->vcocorrection);
    yyjson_mut_obj_add (&entry, &vcocorr_key, &vcocorr);
  }
  if (exception->receptionquality >= 0)
  {
    yyjson_mut_set_str (&recqual_key, "ReceptionQuality");
    yyjson_mut_set_sint (&recqual, exception->receptionquality);
    yyjson_mut_obj_add (&entry, &recqual_key, &recqual);
  }
  if (exception->count > 0)
  {
    yyjson_mut_set_str (&count_key, "Count");
    yyjson_mut_set_sint (&count, exception->count);
    yyjson_mut_obj_add (&entry, &count_key, &count);
  }
  if (exception->type[0])
  {
    yyjson_mut_set_str (&type_key, "Type");
    yyjson_mut_set_str (&type, exception->type);
    yyjson_mut_obj_add (&entry, &type_key, &type);
  }
  if (exception->clockstatus[0])
  {
    yyjson_mut_set_str (&clockstatus_key, "ClockStatus");
    yyjson_mut_set_str (&clockstatus, exception->clockstatus);
    yyjson_mut_obj_add (&entry, &clockstatus_key, &clockstatus);
  }

  /* Add new object to array, created 'value' will be free'd on successful return */
  if (mseh_set_ptr_r (msr, (ptr) ? ptr : "/FDSN/Time/Exception", &entry, 'A', parsestate))
  {
    return MS_GENERROR;
  }

  return 0;
} /* End of mseh_add_timing_exception_r() */

/**********************************************************************/ /**
 * @brief Add recenter event to the extra headers of the given record.
 *
 * If \a ptr is NULL, the default is \c '/FDSN/Recenter/Sequence'.
 *
 * @param[in] msr Parsed miniSEED record to query
 * @param[in] ptr Header value desired, specified in dot notation
 * @param[in] recenter Structure with recenter values
 * @param[in] parsestate Parsed state for multiple operations, can be NULL
 *
 * @returns 0 on success, otherwise a (negative) libmseed error code.
 *
 * \ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
mseh_add_recenter_r (MS3Record *msr, const char *ptr, MSEHRecenter *recenter,
                     LM_PARSED_JSON **parsestate)
{
  yyjson_mut_val entry;
  yyjson_mut_val type_key, type;
  yyjson_mut_val begin_key, begin;
  yyjson_mut_val end_key, end;
  yyjson_mut_val trigger_key, trigger;
  char beginstring[40];
  char endstring[40];

  if (!msr || !recenter)
  {
    ms_log (2, "%s() Required argument not defined: 'msr' or 'recenter'\n", __func__);
    return MS_GENERROR;
  }

  yyjson_mut_set_obj (&entry);

  /* Add elements to new object */
  if (recenter->type[0])
  {
    yyjson_mut_set_str (&type_key, "Type");
    yyjson_mut_set_str (&type, recenter->type);
    yyjson_mut_obj_add (&entry, &type_key, &type);
  }
  if (recenter->begintime != NSTERROR && recenter->begintime != NSTUNSET)
  {
    if (ms_nstime2timestr (recenter->begintime, beginstring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&begin_key, "BeginTime");
      yyjson_mut_set_str (&begin, beginstring);
      yyjson_mut_obj_add (&entry, &begin_key, &begin);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %" PRId64 "\n", __func__, recenter->begintime);
      return MS_GENERROR;
    }
  }
  if (recenter->endtime != NSTERROR && recenter->endtime != NSTUNSET)
  {
    if (ms_nstime2timestr (recenter->endtime, endstring, ISOMONTHDAY_Z, NANO_MICRO_NONE))
    {
      yyjson_mut_set_str (&end_key, "EndTime");
      yyjson_mut_set_str (&end, endstring);
      yyjson_mut_obj_add (&entry, &end_key, &end);
    }
    else
    {
      ms_log (2, "%s() Cannot create time string for %" PRId64 "\n", __func__, recenter->endtime);
      return MS_GENERROR;
    }
  }
  if (recenter->trigger[0])
  {
    yyjson_mut_set_str (&trigger_key, "Trigger");
    yyjson_mut_set_str (&trigger, recenter->trigger);
    yyjson_mut_obj_add (&entry, &trigger_key, &trigger);
  }

  /* Add new object to array, created 'value' will be free'd on successful return */
  if (mseh_set_ptr_r (msr, (ptr) ? ptr : "/FDSN/Recenter/Sequence", &entry, 'A', parsestate))
  {
    return MS_GENERROR;
  }

  return 0;
} /* End of mseh_add_recenter_r() */

/**********************************************************************/ /**
 * @brief Generate extra headers string (serialize) from internal state
 *
 * Generate the extra headers JSON string from the internal parse state
 * created by mseh_set_ptr_r().
 *
 * @param[in] msr ::MS3Record to generate extra headers for
 * @param[in] parsestate Internal parsed state associated with \a msr
 *
 * @returns Length of extra headers on success, otherwise a (negative) libmseed error code
 *
 * \sa mseh_set_ptr_r()
 ***************************************************************************/
int
mseh_serialize (MS3Record *msr, LM_PARSED_JSON **parsestate)
{
  yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
  yyjson_write_err err;
  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};

  LM_PARSED_JSON *parsed = NULL;
  char *serialized    = NULL;
  size_t serialsize   = 0;

  if (!msr || !parsestate)
    return MS_GENERROR;

  parsed = *parsestate;

  if (!parsed || !parsed->mut_doc)
    return 0;

  /* Serialize new JSON string */
  serialized = yyjson_mut_write_opts (parsed->mut_doc, flg, &alc, &serialsize, &err);

  if (serialized == NULL)
  {
    ms_log (2, "%s() Cannot write extra header JSON: %s\n",
            __func__, (err.msg) ? err.msg : "Unknown error");
    return MS_GENERROR;
  }

  if (serialsize > UINT16_MAX)
  {
    ms_log (2, "%s() New serialization size exceeds limit of %d bytes: %" PRIu64 "\n",
            __func__, UINT16_MAX, (uint64_t)serialsize);
    libmseed_memory.free(serialized);
    return MS_GENERROR;
  }

  /* Set new extra headers, replacing existing headers */
  if (msr->extra)
    libmseed_memory.free (msr->extra);
  msr->extra       = serialized;
  msr->extralength = (uint16_t)serialsize;

  return msr->extralength;
}

/**********************************************************************/ /**
 * @brief Free internally parsed (deserialized) JSON data
 *
 * Free the memory associated with JSON data parsed by mseh_get_ptr_r()
 * or mseh_set_ptr_r(), specifically the data at the \a parsestate pointer.
 *
 * @param[in] parsestate Internal parsed state associated with \a msr
 *
 * \sa mseh_get_ptr_r()
 * \sa mseh_set_ptr_r()
 ***************************************************************************/
void
mseh_free_parsestate (LM_PARSED_JSON **parsestate)
{
  LM_PARSED_JSON *parsed = NULL;

  if (!parsestate || !*parsestate)
    return;

  parsed = *parsestate;

  if (parsed->doc)
    yyjson_doc_free (parsed->doc);

  if (parsed->mut_doc)
    yyjson_mut_doc_free (parsed->mut_doc);

  libmseed_memory.free(parsed);

  *parsestate = NULL;
}

/**********************************************************************/ /**
 * @brief Replace extra headers with supplied JSON
 *
 * Parse the supplied JSON string, re-serialize into compact form, and replace
 * the extra headers of \a msr with the result.
 *
 * To _remove_ all of the extra headers, set \a jsonstring to NULL.
 *
 * This function cannot be used in combination with the routines that use
 * a parsed state, i.e. mseh_get_ptr_r() and mseh_set_ptr_r().
 *
 * @param[in] msr ::MS3Record to generate extra headers for
 * @param[in] jsonstring JSON replacment for extra headers of \a msr
 *
 * @returns Length of extra headers on success, otherwise a (negative) libmseed error code
 ***************************************************************************/
int
mseh_replace (MS3Record *msr, char *jsonstring)
{
  yyjson_read_flag read_flg = YYJSON_READ_NOFLAG;
  yyjson_write_flag write_flg = YYJSON_WRITE_NOFLAG;
  yyjson_read_err read_err;
  yyjson_write_err write_err;
  yyjson_alc alc = {_priv_malloc, _priv_realloc, _priv_free, NULL};
  yyjson_doc *doc = NULL;

  char *serialized  = NULL;
  size_t serialsize = 0;

  if (!msr)
    return MS_GENERROR;

  if (jsonstring != NULL)
  {
    /* Parse JSON into immutable form */
    if ((doc = yyjson_read_opts (jsonstring, strlen (jsonstring), read_flg, &alc, &read_err)) == NULL)
    {
      ms_log (2, "%s() Cannot parse extra header JSON: %s\n",
              __func__, (read_err.msg) ? read_err.msg : "Unknown error");
      return MS_GENERROR;
    }

    /* Serialize new JSON string */
    serialized = yyjson_write_opts (doc, write_flg, &alc, &serialsize, &write_err);

    if (serialized == NULL)
    {
      ms_log (2, "%s() Cannot write extra header JSON: %s\n",
              __func__, (write_err.msg) ? write_err.msg : "Unknown error");
      return MS_GENERROR;
    }

    if (serialsize > UINT16_MAX)
    {
      ms_log (2, "%s() New serialization size exceeds limit of %d bytes: %" PRIu64 "\n",
              __func__, UINT16_MAX, (uint64_t)serialsize);
      libmseed_memory.free (serialized);
      return MS_GENERROR;
    }
  }

  /* Set new extra headers, replacing existing headers */
  if (msr->extra)
    libmseed_memory.free (msr->extra);
  msr->extra       = serialized;
  msr->extralength = (uint16_t)serialsize;

  return msr->extralength;
}

/**********************************************************************/ /**
 * @brief Print the extra header structure for the specified MS3Record.
 *
 * Output is printed in a pretty, formatted form for readability and
 * the anonymous, root object container (the outer {}) is not printed.
 *
 * @param[in] msr ::MS3Record with extra headers to pring
 * @param[in] indent Number of spaces to indent output
 *
 * @returns 0 on success and a (negative) libmseed error code on error.
 ***************************************************************************/
int
mseh_print (const MS3Record *msr, int indent)
{
  char *extra;
  int idx;
  int instring = 0;

  if (!msr)
    return MS_GENERROR;

  if (!msr->extra || !msr->extralength)
    return MS_NOERROR;

  extra = msr->extra;

  if (extra[0] != '{' || extra[msr->extralength - 1] != '}')
  {
    ms_log (1, "%s() Warning, something is wrong, extra headers are not a clean {object}\n", __func__);
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
      if (extra[idx] == ':')
      {
        ms_log (0, ": ");
      }
      else if (extra[idx] == ',')
      {
        ms_log (0, ",\n%*s", indent, "");
      }
      else if (extra[idx] == '{')
      {
        indent += 2;
        ms_log (0, "{\n%*s", indent, "");
      }
      else if (extra[idx] == '[')
      {
        indent += 2;
        ms_log (0, "[\n%*s", indent, "");
      }
      else if (extra[idx] == '}')
      {
        indent -= 2;
        ms_log (0, "\n%*s}", indent, "");
      }
      else if (extra[idx] == ']')
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
