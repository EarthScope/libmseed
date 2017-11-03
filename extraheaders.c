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
 * mseh_fetch:
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
  cbor_item_t item;

  if (!msr || !path)
    return MS_GENERROR;

  if (!msr->extralength)
    return 1;

  if (!msr->extra)
    return MS_GENERROR;

  cbor_init (&stream, msr->extra, msr->extralength);
  cbor_fetch_map_item (&stream, 0, &item, path);

  if (item.type < 0)
    return 1;

  if (type == 'i' && (item.type == CBOR_UINT || item.type == CBOR_NEGINT))
  {
    if (value)
    {
      if (item.type == CBOR_UINT)
        *((int64_t*)value) = item.value.u;  /* Overflow for huge unsigned values */
      else
        *((int64_t*)value) = item.value.i;
    }
  }
  else if (type == 'd' && (item.type == CBOR_FLOAT16 || item.type == CBOR_FLOAT32 || item.type == CBOR_FLOAT64))
  {
    if (value)
    {
      if (item.type == CBOR_FLOAT16 || item.type == CBOR_FLOAT32)
        *((double*)value) = item.value.f;
      else
        *((double*)value) = item.value.d;
    }
  }
  else if (type == 'c' && (item.type == CBOR_BYTES || item.type == CBOR_TEXT))
  {
    if (value)
    {
      /* Copy buffer and terminate */
      if (length > item.length)
      {
        memcpy (value, item.value.c, item.length);
        ((uint8_t *)value)[item.length] = '\0';
      }
      else
      {
        memcpy (value, item.value.c, length - 1);
        ((uint8_t *)value)[length - 1] = '\0';
      }
    }
  }
  else if (type == 'b' && (item.type == CBOR_TRUE || item.type == CBOR_FALSE))
  {
    if (value)
    {
      if (item.type == CBOR_TRUE)
        *((int*)value) = 1;
      else
        *((int*)value) = 0;
    }
  }
  /* Only return "wrong type" result when type is set */
  else if (type)
    return 2;

  return 0;
}

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
  char output[4096];
  int length;
  int idx;

  if (!msr)
    return MS_GENERROR;

  length = mseh_to_json (msr, output, sizeof(output));

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
