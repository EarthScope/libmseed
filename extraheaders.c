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
