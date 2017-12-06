/***************************************************************************
 * lookup.c:
 *
 * Generic lookup routines for miniSEED information.
 *
 * Written by Chad Trabant, IRIS Data Management Center
 ***************************************************************************/

#include <string.h>

#include "libmseed.h"

/***************************************************************************
 * ms_samplesize():
 *
 * Returns the sample size based on type code or 0 for unknown.
 ***************************************************************************/
uint8_t
ms_samplesize (const char sampletype)
{
  switch (sampletype)
  {
  case 'a':
    return 1;
  case 'i':
  case 'f':
    return 4;
  case 'd':
    return 8;
  default:
    return 0;
  } /* end switch */

} /* End of ms_samplesize() */

/***************************************************************************
 * ms_encodingstr():
 *
 * Returns a string describing a data encoding format.
 ***************************************************************************/
char *
ms_encodingstr (const char encoding)
{
  switch (encoding)
  {
  case 0:
    return "ASCII text";
  case 1:
    return "16 bit integers";
  case 2:
    return "24 bit integers";
  case 3:
    return "32 bit integers";
  case 4:
    return "IEEE floating point";
  case 5:
    return "IEEE double precision float";
  case 10:
    return "STEIM 1 Compression";
  case 11:
    return "STEIM 2 Compression";
  case 12:
    return "GEOSCOPE Muxed 24 bit int";
  case 13:
    return "GEOSCOPE Muxed 16/3 bit gain/exp";
  case 14:
    return "GEOSCOPE Muxed 16/4 bit gain/exp";
  case 15:
    return "US National Network compression";
  case 16:
    return "CDSN 16 bit gain ranged";
  case 17:
    return "Graefenberg 16 bit gain ranged";
  case 18:
    return "IPG - Strasbourg 16 bit gain";
  case 19:
    return "STEIM 3 Compression";
  case 30:
    return "SRO Gain Ranged Format";
  case 31:
    return "HGLP Format";
  case 32:
    return "DWWSSN Format";
  case 33:
    return "RSTN 16 bit gain ranged";
  default:
    return "Unknown format code";
  } /* end switch */

} /* End of ms_encodingstr() */

/***************************************************************************
 * ms_errorstr():
 *
 * Return a string describing a given libmseed error code or NULL if the
 * code is unknown.
 ***************************************************************************/
char *
ms_errorstr (int errorcode)
{
  switch (errorcode)
  {
  case MS_ENDOFFILE:
    return "End of file reached";
  case MS_NOERROR:
    return "No error";
  case MS_GENERROR:
    return "Generic error";
  case MS_NOTSEED:
    return "No SEED data detected";
  case MS_WRONGLENGTH:
    return "Length of data read does not match record length";
  case MS_OUTOFRANGE:
    return "SEED record length out of range";
  case MS_UNKNOWNFORMAT:
    return "Unknown data encoding format";
  case MS_STBADCOMPFLAG:
    return "Bad Steim compression flag(s) detected";
  case MS_INVALIDCRC:
    return "Invalid CRC detected";
  } /* end switch */

  return NULL;

} /* End of ms_blktdesc() */
