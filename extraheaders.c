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
 * Map elements.  The path of Map elemens is a sequence of Map keys
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
 * Map elements.  The path of Map elemens is a sequence of Map keys
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
    if ( output[idx] == ':')
      ms_log (0, ": ");
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
      ms_log (0, "%c", output[idx]);
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
