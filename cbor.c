/***************************************************************************
 * cbor.c:
 *
 * CBOR handling routines extracted from RIOT-OS:
 * https://github.com/RIOT-OS/RIOT
 *
 * All copyrights and license notifications are retained below.
 *
 * This version started with commit
 * 4839a1cbbf71631c529a49b40a53a8f1f8702cb4 to RIOT-OS and has been
 * modified to work in libmseed as follows:
 * - Replace byteswapping routines with those in libmseed
 * - Formatting to match libmseed source
 * - Removing unneeded code (e.g. [de]serialize_date_time*, priters, dump_memory)
 ***************************************************************************/

/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2014 Kevin Funk <kfunk@kde.org>
 * Copyright (C) 2014 Jana Cavojska <jana.cavojska9@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @author      Kevin Funk <kfunk@kde.org>
 * @author      Jana Cavojska <jana.cavojska9@gmail.com>
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cbor.h"
#include "libmseed.h"

/* Ensure that @p stream is big enough to fit @p bytes bytes, otherwise return 0 */
#define CBOR_ENSURE_SIZE(stream, bytes)      \
  do                                         \
  {                                          \
    if (stream->pos + bytes >= stream->size) \
    {                                        \
      return 0;                              \
    }                                        \
  } while (0)

#define CBOR_ENSURE_SIZE_READ(stream, bytes) \
  do                                         \
  {                                          \
    if (bytes > stream->size)                \
    {                                        \
      return 0;                              \
    }                                        \
  } while (0)

#ifndef INFINITY
#define INFINITY (1.0 / 0.0)
#endif
#ifndef NAN
#define NAN (0.0 / 0.0)
#endif

/**
 * Source: CBOR RFC reference implementation
 */
double
decode_float_half (unsigned char *halfp)
{
  int half = (halfp[0] << 8) + halfp[1];
  int exp = (half >> 10) & 0x1f;
  int mant = half & 0x3ff;
  double val;

  if (exp == 0)
  {
    val = ldexp (mant, -24);
  }
  else if (exp != 31)
  {
    val = ldexp (mant + 1024, exp - 25);
  }
  else
  {
    val = mant == 0 ? INFINITY : NAN;
  }

  return (half & 0x8000) ? -val : val;
}

/**
 * Source: According to http://gamedev.stackexchange.com/questions/17326/conversion-of-a-number-from-single-precision-floating-point-representation-to-a
 */
static uint16_t
encode_float_half (float x)
{
  union u {
    float f;
    uint32_t i;
  } u = {.f = x};

  uint16_t bits = (u.i >> 16) & 0x8000; /* Get the sign */
  uint16_t m = (u.i >> 12) & 0x07ff; /* Keep one extra bit for rounding */
  unsigned int e = (u.i >> 23) & 0xff; /* Using int is faster here */

  /* If zero, or denormal, or exponent underflows too much for a denormal
     * half, return signed zero. */
  if (e < 103)
  {
    return bits;
  }

  /* If NaN, return NaN. If Inf or exponent overflow, return Inf. */
  if (e > 142)
  {
    bits |= 0x7c00u;
    /* If exponent was 0xff and one mantissa bit was set, it means NaN,
         * not Inf, so make sure we set one mantissa bit too. */
    bits |= (e == 255) && (u.i & 0x007fffffu);
    return bits;
  }

  /* If exponent underflows but not too much, return a denormal */
  if (e < 113)
  {
    m |= 0x0800u;
    /* Extra rounding may overflow and set mantissa to 0 and exponent
         * to 1, which is OK. */
    bits |= (m >> (114 - e)) + ((m >> (113 - e)) & 1);
    return bits;
  }

  bits |= ((e - 112) << 10) | (m >> 1);
  /* Extra rounding. An overflow will set mantissa to 0 and increment
     * the exponent, which is OK. */
  bits += m & 1;
  return bits;
}

void
cbor_init (cbor_stream_t *stream, unsigned char *buffer, size_t size)
{
  if (!stream)
  {
    return;
  }

  stream->data = buffer;
  stream->size = size;
  stream->pos = 0;
}

void
cbor_clear (cbor_stream_t *stream)
{
  if (!stream)
  {
    return;
  }

  stream->pos = 0;
}

void
cbor_destroy (cbor_stream_t *stream)
{
  if (!stream)
  {
    return;
  }

  stream->data = 0;
  stream->size = 0;
  stream->pos = 0;
}

/**
 * Return additional info field value for input value @p val
 *
 * @return Byte with the additional info bits set
 */
static unsigned char
uint_additional_info (uint64_t val)
{
  if (val < CBOR_UINT8_FOLLOWS)
  {
    return val;
  }
  else if (val <= 0xff)
  {
    return CBOR_UINT8_FOLLOWS;
  }
  else if (val <= 0xffff)
  {
    return CBOR_UINT16_FOLLOWS;
  }
  else if (val <= 0xffffffffL)
  {
    return CBOR_UINT32_FOLLOWS;
  }

  return CBOR_UINT64_FOLLOWS;
}

/**
 * Return the number of bytes that would follow the additional info field @p additional_info
 *
 * @param additional_info Must be in the range [CBOR_UINT8_FOLLOWS, CBOR_UINT64_FOLLOWS]
 */
static unsigned char
uint_bytes_follow (unsigned char additional_info)
{
  if (additional_info < CBOR_UINT8_FOLLOWS || additional_info > CBOR_UINT64_FOLLOWS)
  {
    return 0;
  }

  static const unsigned char BYTES_FOLLOW[] = {1, 2, 4, 8};
  return BYTES_FOLLOW[additional_info - CBOR_UINT8_FOLLOWS];
}

static size_t
encode_int (unsigned char major_type, cbor_stream_t *s, uint64_t val)
{
  if (!s)
  {
    return 0;
  }

  unsigned char additional_info = uint_additional_info (val);
  unsigned char bytes_follow = uint_bytes_follow (additional_info);
  CBOR_ENSURE_SIZE (s, bytes_follow + 1);
  s->data[s->pos++] = major_type | additional_info;

  for (int i = bytes_follow - 1; i >= 0; --i)
  {
    s->data[s->pos++] = (val >> (8 * i)) & 0xff;
  }

  return bytes_follow + 1;
}

static size_t
decode_int (const cbor_stream_t *s, size_t offset, uint64_t *val)
{
  uint16_t u16;
  uint32_t u32;

  if (!s)
  {
    return 0;
  }


  CBOR_ENSURE_SIZE_READ (s, offset + 1);

  unsigned char additional_info = CBOR_ADDITIONAL_INFO (s, offset);
  unsigned char bytes_follow = uint_bytes_follow (additional_info);

  CBOR_ENSURE_SIZE_READ (s, offset + 1 + bytes_follow);

  if (val)
  {
    *val = 0; /* clear val first */

    switch (bytes_follow)
    {
    case 0:
      *val = ((uint8_t)s->data[offset] & CBOR_INFO_MASK);
      break;
    case 1:
      *val = (uint8_t)s->data[offset + 1];
      break;
    case 2:
      memcpy (&u16, (s->data + offset + 1), sizeof (uint16_t));
      if (!ms_bigendianhost ())
        ms_gswap2a (&u16);
      *val = u16;
      break;
    case 4:
      memcpy (&u32, (s->data + offset + 1), sizeof (uint32_t));
      if (!ms_bigendianhost ())
        ms_gswap4a (&u32);
      *val = u32;
      break;
    default:
      memcpy (val, (s->data + offset + 1), sizeof (uint64_t));
      if (!ms_bigendianhost ())
        ms_gswap8a (val);
      break;
    }
  }

  return bytes_follow + 1;
}

static size_t
encode_bytes (unsigned char major_type, cbor_stream_t *s, const char *data,
              size_t length)
{
  size_t length_field_size = uint_bytes_follow (uint_additional_info (length)) + 1;
  CBOR_ENSURE_SIZE (s, length_field_size + length);

  size_t bytes_start = encode_int (major_type, s, (uint64_t)length);

  if (!bytes_start)
  {
    return 0;
  }

  memcpy (&(s->data[s->pos]), data, length); /* copy byte string into our cbor struct */
  s->pos += length;
  return (bytes_start + length);
}

static size_t
decode_bytes (const cbor_stream_t *s, size_t offset, char *out, size_t length)
{
  CBOR_ENSURE_SIZE_READ (s, offset + 1);

  if ((CBOR_TYPE (s, offset) != CBOR_BYTES && CBOR_TYPE (s, offset) != CBOR_TEXT) || !out)
  {
    return 0;
  }

  uint64_t bytes_length;
  size_t bytes_start = decode_int (s, offset, &bytes_length);

  if (!bytes_start)
  {
    return 0;
  }

  if (bytes_length == SIZE_MAX || length < bytes_length + 1)
  {
    return 0;
  }

  CBOR_ENSURE_SIZE_READ (s, offset + bytes_start + bytes_length);

  memcpy (out, &s->data[offset + bytes_start], bytes_length);
  out[bytes_length] = '\0';
  return (bytes_start + bytes_length);
}

/* A zero copy version of decode_bytes.
   Will not null termiante input, but will tell you the size of what you read.
   Great for reading byte strings which could contain nulls inside of unknown size
   without forced copies.
*/
static size_t
decode_bytes_no_copy (const cbor_stream_t *s, size_t offset, unsigned char **out, size_t *length)
{
  CBOR_ENSURE_SIZE_READ (s, offset + 1);

  if ((CBOR_TYPE (s, offset) != CBOR_BYTES && CBOR_TYPE (s, offset) != CBOR_TEXT))
  {
    return 0;
  }

  uint64_t bytes_length;
  size_t bytes_start = decode_int (s, offset, &bytes_length);

  if (!bytes_start)
  {
    return 0;
  }

  CBOR_ENSURE_SIZE_READ (s, offset + bytes_start + bytes_length);
  if (out)
    *out = &(s->data[offset + bytes_start]);
  if (length)
    *length = bytes_length;
  return (bytes_start + bytes_length);
}

size_t
cbor_deserialize_int (const cbor_stream_t *stream, size_t offset, int *val)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if ((CBOR_TYPE (stream, offset) != CBOR_UINT && CBOR_TYPE (stream, offset) != CBOR_NEGINT) || !val)
  {
    return 0;
  }

  uint64_t buf;
  size_t read_bytes = decode_int (stream, offset, &buf);

  if (!read_bytes)
  {
    return 0;
  }

  if (CBOR_TYPE (stream, offset) == CBOR_UINT)
  {
    *val = buf; /* resolve as CBOR_UINT */
  }
  else
  {
    *val = -1 - buf; /* resolve as CBOR_NEGINT */
  }

  return read_bytes;
}

size_t
cbor_serialize_int (cbor_stream_t *s, int val)
{
  if (val >= 0)
  {
    /* Major type 0: an unsigned integer */
    return encode_int (CBOR_UINT, s, val);
  }
  else
  {
    /* Major type 1: an negative integer */
    return encode_int (CBOR_NEGINT, s, -1 - val);
  }
}

size_t
cbor_deserialize_uint64_t (const cbor_stream_t *stream, size_t offset, uint64_t *val)
{
  if (CBOR_TYPE (stream, offset) != CBOR_UINT || !val)
  {
    return 0;
  }

  return decode_int (stream, offset, val);
}

size_t
cbor_serialize_uint64_t (cbor_stream_t *s, uint64_t val)
{
  return encode_int (CBOR_UINT, s, val);
}

size_t
cbor_deserialize_int64_t (const cbor_stream_t *stream, size_t offset, int64_t *val)
{
  if ((CBOR_TYPE (stream, offset) != CBOR_UINT && CBOR_TYPE (stream, offset) != CBOR_NEGINT) || !val)
  {
    return 0;
  }

  uint64_t buf;
  size_t read_bytes = decode_int (stream, offset, &buf);

  if (CBOR_TYPE (stream, offset) == CBOR_UINT)
  {
    *val = buf; /* resolve as CBOR_UINT */
  }
  else
  {
    *val = -1 - buf; /* resolve as CBOR_NEGINT */
  }

  return read_bytes;
}

size_t
cbor_serialize_int64_t (cbor_stream_t *s, int64_t val)
{
  if (val >= 0)
  {
    /* Major type 0: an unsigned integer */
    return encode_int (CBOR_UINT, s, val);
  }
  else
  {
    /* Major type 1: an negative integer */
    return encode_int (CBOR_NEGINT, s, -1 - val);
  }
}

size_t
cbor_deserialize_bool (const cbor_stream_t *stream, size_t offset, bool *val)
{
  if (CBOR_TYPE (stream, offset) != CBOR_7 || !val)
  {
    return 0;
  }

  unsigned char byte = stream->data[offset];
  *val = (byte == CBOR_TRUE);
  return 1;
}

size_t
cbor_serialize_bool (cbor_stream_t *s, bool val)
{
  CBOR_ENSURE_SIZE (s, 1);
  s->data[s->pos++] = val ? CBOR_TRUE : CBOR_FALSE;
  return 1;
}

size_t
cbor_deserialize_float_half (const cbor_stream_t *stream, size_t offset, float *val)
{
  if (CBOR_TYPE (stream, offset) != CBOR_7)
  {
    return 0;
  }

  unsigned char *data = &stream->data[offset];

  if (*data == CBOR_FLOAT16)
  {
    if (val)
      *val = (float)decode_float_half (data + 1);
    return 3;
  }

  return 0;
}

size_t
cbor_serialize_float_half (cbor_stream_t *s, float val)
{
  uint16_t encoded_val;
  CBOR_ENSURE_SIZE (s, 3);
  s->data[s->pos++] = CBOR_FLOAT16;
  encoded_val = encode_float_half (val);
  if (!ms_bigendianhost ())
    ms_gswap2a (&encoded_val);
  memcpy (s->data + s->pos, &encoded_val, 2);
  s->pos += 2;
  return 3;
}

size_t
cbor_deserialize_float (const cbor_stream_t *stream, size_t offset, float *val)
{
  if (CBOR_TYPE (stream, offset) != CBOR_7)
  {
    return 0;
  }

  if (stream->data[offset] == CBOR_FLOAT32)
  {
    if (val)
    {
      memcpy (val, &stream->data[offset+1], sizeof(float));
      if (!ms_bigendianhost())
        ms_gswap4a(val);
    }

    return 5;
  }

  return 0;
}

size_t
cbor_serialize_float (cbor_stream_t *s, float val)
{
  uint32_t encoded_val;
  CBOR_ENSURE_SIZE (s, 5);
  s->data[s->pos++] = CBOR_FLOAT32;
  encoded_val = val;
  if (!ms_bigendianhost())
    ms_gswap4a(&encoded_val);
  memcpy (s->data + s->pos, &encoded_val, 4);
  s->pos += 4;
  return 5;
}

size_t
cbor_deserialize_double (const cbor_stream_t *stream, size_t offset, double *val)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if (CBOR_TYPE (stream, offset) != CBOR_7)
  {
    return 0;
  }

  if (stream->data[offset] == CBOR_FLOAT64)
  {
    CBOR_ENSURE_SIZE_READ (stream, offset + 9);
    if (val)
    {
      memcpy (val, &stream->data[offset+1], sizeof(double));
      if (!ms_bigendianhost())
        ms_gswap8a(val);
    }

    return 9;
  }

  return 0;
}

size_t
cbor_serialize_double (cbor_stream_t *s, double val)
{
  uint64_t encoded_val;
  CBOR_ENSURE_SIZE (s, 9);
  s->data[s->pos++] = CBOR_FLOAT64;
  encoded_val = val;
  if (!ms_bigendianhost())
    ms_gswap8a(&encoded_val);
  memcpy (s->data + s->pos, &encoded_val, 8);
  s->pos += 8;
  return 9;
}

size_t
cbor_deserialize_byte_string (const cbor_stream_t *stream, size_t offset, char *val,
                              size_t length)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if (CBOR_TYPE (stream, offset) != CBOR_BYTES)
  {
    return 0;
  }

  return decode_bytes (stream, offset, val, length);
}

size_t
cbor_deserialize_byte_string_no_copy (const cbor_stream_t *stream, size_t offset, unsigned char **val,
                                      size_t *length)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if (CBOR_TYPE (stream, offset) != CBOR_BYTES)
  {
    return 0;
  }

  return decode_bytes_no_copy (stream, offset, val, length);
}

size_t
cbor_serialize_byte_string (cbor_stream_t *stream, const char *val)
{
  return encode_bytes (CBOR_BYTES, stream, val, strlen (val));
}

size_t
cbor_serialize_byte_stringl (cbor_stream_t *stream, const char *val, size_t length)
{
  return encode_bytes (CBOR_BYTES, stream, val, length);
}

size_t
cbor_deserialize_unicode_string (const cbor_stream_t *stream, size_t offset, char *val,
                                 size_t length)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if (CBOR_TYPE (stream, offset) != CBOR_TEXT)
  {
    return 0;
  }

  return decode_bytes (stream, offset, val, length);
}

size_t
cbor_deserialize_unicode_string_no_copy (const cbor_stream_t *stream, size_t offset, unsigned char **val,
                                         size_t *length)
{
  CBOR_ENSURE_SIZE_READ (stream, offset + 1);

  if (CBOR_TYPE (stream, offset) != CBOR_TEXT)
  {
    return 0;
  }

  return decode_bytes_no_copy (stream, offset, val, length);
}

size_t
cbor_serialize_unicode_string (cbor_stream_t *stream, const char *val)
{
  return encode_bytes (CBOR_TEXT, stream, val, strlen (val));
}

size_t
cbor_deserialize_array (const cbor_stream_t *s, size_t offset, size_t *array_length)
{
  if (CBOR_TYPE (s, offset) != CBOR_ARRAY)
  {
    return 0;
  }

  uint64_t val;
  size_t read_bytes = decode_int (s, offset, &val);
  if (array_length)
    *array_length = (size_t)val;
  return read_bytes;
}

size_t
cbor_serialize_array (cbor_stream_t *s, size_t array_length)
{
  /* serialize number of array items */
  return encode_int (CBOR_ARRAY, s, array_length);
}

size_t
cbor_serialize_array_indefinite (cbor_stream_t *s)
{
  CBOR_ENSURE_SIZE (s, 1);
  s->data[s->pos++] = CBOR_ARRAY | CBOR_VAR_FOLLOWS;
  return 1;
}

size_t
cbor_deserialize_array_indefinite (const cbor_stream_t *s, size_t offset)
{
  if (s->data[offset] != (CBOR_ARRAY | CBOR_VAR_FOLLOWS))
  {
    return 0;
  }

  return 1;
}

size_t
cbor_serialize_map_indefinite (cbor_stream_t *s)
{
  CBOR_ENSURE_SIZE (s, 1);
  s->data[s->pos++] = CBOR_MAP | CBOR_VAR_FOLLOWS;
  return 1;
}

size_t
cbor_deserialize_map_indefinite (const cbor_stream_t *s, size_t offset)
{
  if (s->data[offset] != (CBOR_MAP | CBOR_VAR_FOLLOWS))
  {
    return 0;
  }

  return 1;
}

size_t
cbor_deserialize_map (const cbor_stream_t *s, size_t offset, size_t *map_length)
{
  if (CBOR_TYPE (s, offset) != CBOR_MAP)
  {
    return 0;
  }

  uint64_t val;
  size_t read_bytes = decode_int (s, offset, &val);
  if (map_length)
    *map_length = (size_t)val;
  return read_bytes;
}

size_t
cbor_serialize_map (cbor_stream_t *s, size_t map_length)
{
  /* serialize number of item key-value pairs */
  return encode_int (CBOR_MAP, s, map_length);
}

size_t
cbor_write_tag (cbor_stream_t *s, unsigned char tag)
{
  CBOR_ENSURE_SIZE (s, 1);
  s->data[s->pos++] = CBOR_TAG | tag;
  return 1;
}

bool
cbor_at_tag (const cbor_stream_t *s, size_t offset)
{
  return cbor_at_end (s, offset) || CBOR_TYPE (s, offset) == CBOR_TAG;
}

size_t
cbor_write_break (cbor_stream_t *s)
{
  CBOR_ENSURE_SIZE (s, 1);
  s->data[s->pos++] = CBOR_BREAK;
  return 1;
}

bool
cbor_at_break (const cbor_stream_t *s, size_t offset)
{
  return cbor_at_end (s, offset) || s->data[offset] == CBOR_BREAK;
}

bool
cbor_at_end (const cbor_stream_t *s, size_t offset)
{
  /* cbor_stream_t::pos points at the next *free* byte, hence the -1 */
  return s ? offset >= s->pos - 1 : true;
}


/* Routines below added for libmseed usage */

/* Traverse CBOR (recursively) and print diagnostic output */
size_t
cbor_map_to_diag (cbor_stream_t *stream, size_t offset, int maxstringprint,
                  char *output, size_t *outputoffset, const size_t outputmax)
{
  size_t readbytes = 0;
  size_t containerlength;
  bool is_indefinite;
  size_t remaining;
  int printlength;

  /* Decoded values */
  unsigned char *cval;
  size_t length;
  uint64_t u64val;
  int64_t i64val;
  float fval;
  double dval;

  /* Simple macro for adding output to buffer with local variables */
#define ADD_PRINTF_OUTPUT(...)                                               \
  remaining = (*outputoffset < outputmax) ? (outputmax - *outputoffset) : 0; \
  *outputoffset += snprintf (output + *outputoffset, remaining, __VA_ARGS__);

  if (!stream || !output || !outputoffset)
    return 0;

  if (*outputoffset >= outputmax)
  {
    ms_log (2, "cbor_map_to_diag(): output buffer not big enough for CBOR Map\n");
    return 0;
  }

  switch (CBOR_TYPE (stream, offset))
  {
  case CBOR_UINT:
    readbytes = cbor_deserialize_uint64_t (stream, offset, &u64val);
    ADD_PRINTF_OUTPUT ("%" PRIu64, u64val);
    break;

  case CBOR_NEGINT:
    readbytes = cbor_deserialize_int64_t (stream, offset, &i64val);
    ADD_PRINTF_OUTPUT ("%" PRId64, i64val);
    break;

  case CBOR_BYTES:
    readbytes = cbor_deserialize_byte_string_no_copy (stream, offset, &cval, &length);
    printlength = (maxstringprint > 0 && length > maxstringprint) ? maxstringprint : length;
    ADD_PRINTF_OUTPUT ("\"%.*s%s\"", printlength, cval,
                       (maxstringprint > 0 && length > maxstringprint) ? "..." : "");
    break;

  case CBOR_TEXT:
    readbytes = cbor_deserialize_unicode_string_no_copy (stream, offset, &cval, &length);
    printlength = (maxstringprint > 0 && length > maxstringprint) ? maxstringprint : length;
    ADD_PRINTF_OUTPUT ("\"%.*s%s\"", printlength, cval,
                       (maxstringprint > 0 && length > maxstringprint) ? "..." : "");
    break;

  case CBOR_ARRAY:
    is_indefinite = (stream->data[offset] == (CBOR_ARRAY | CBOR_VAR_FOLLOWS));

    if (is_indefinite)
    {
      offset += readbytes = cbor_deserialize_array_indefinite (stream, offset);
    }
    else
    {
      containerlength = 0;
      offset += readbytes = cbor_deserialize_array (stream, offset, &containerlength);
    }

    ADD_PRINTF_OUTPUT ("[");

    while (is_indefinite ? !cbor_at_break (stream, offset) : containerlength > 0)
    {
      size_t innerreadbytes;

      offset += innerreadbytes = cbor_map_to_diag (stream, offset, maxstringprint, output, outputoffset, outputmax);

      if (innerreadbytes == 0)
        break;

      readbytes += innerreadbytes;
      containerlength--;

      if (is_indefinite ? !cbor_at_break (stream, offset) : containerlength > 0)
      {
        ADD_PRINTF_OUTPUT (",");
      }
    }

    ADD_PRINTF_OUTPUT ("]");

    readbytes += cbor_at_break (stream, offset);
    break;

  case CBOR_MAP:
    is_indefinite = stream->data[offset] == (CBOR_MAP | CBOR_VAR_FOLLOWS);

    if (is_indefinite)
    {
      offset += readbytes = cbor_deserialize_map_indefinite (stream, offset);
    }
    else
    {
      containerlength = 0;
      offset += readbytes = cbor_deserialize_map (stream, offset, &containerlength);
    }

    ADD_PRINTF_OUTPUT ("{");

    while (is_indefinite ? !cbor_at_break (stream, offset) : containerlength > 0)
    {
      size_t key_read_bytes, value_read_bytes;

      /* Key */
      offset += key_read_bytes = cbor_map_to_diag (stream, offset, maxstringprint, output, outputoffset, outputmax);

      ADD_PRINTF_OUTPUT (":");

      /* Value */
      offset += value_read_bytes = cbor_map_to_diag (stream, offset, maxstringprint, output, outputoffset, outputmax);

      if (key_read_bytes == 0 || value_read_bytes == 0)
        break;

      readbytes += key_read_bytes + value_read_bytes;
      containerlength--;

      if (is_indefinite ? !cbor_at_break (stream, offset) : containerlength > 0)
      {
        ADD_PRINTF_OUTPUT (",");
      }
    }

    ADD_PRINTF_OUTPUT ("}");

    readbytes += cbor_at_break (stream, offset);
    break;

  case CBOR_TAG:
    /* No printing */
    readbytes = 1;
    break;

  case CBOR_7:
    switch (stream->data[offset])
    {
    case CBOR_FALSE:
      readbytes = 1;
      ADD_PRINTF_OUTPUT ("false");
      break;

    case CBOR_TRUE:
      readbytes = 1;
      ADD_PRINTF_OUTPUT ("true");
      break;

    case CBOR_NULL:
      ADD_PRINTF_OUTPUT ("null");
      readbytes = 1;
      break;

    case CBOR_UNDEFINED:
      readbytes = 1;
      ADD_PRINTF_OUTPUT ("\"undefined\"");
      break;

    case CBOR_FLOAT16:
      readbytes = cbor_deserialize_float_half (stream, offset, &fval);
      ADD_PRINTF_OUTPUT ("%g", fval);
      break;

    case CBOR_FLOAT32:
      readbytes = cbor_deserialize_float (stream, offset, &fval);
      ADD_PRINTF_OUTPUT ("%g", fval);
      break;

    case CBOR_FLOAT64:
      readbytes = cbor_deserialize_double (stream, offset, &dval);
      ADD_PRINTF_OUTPUT ("%g", dval);
      break;

    case CBOR_BREAK:
      readbytes = 1;
      ADD_PRINTF_OUTPUT ("\"break\"");
      break;
    }
    break;

  default:
    ms_log (2, "cbor_map_to_diag(): Unrecognized CBOR type: 0x%xd\n", CBOR_TYPE (stream, offset));
  }

#undef ADD_PRINTF_OUTPUT
  return readbytes;
} /* End of cbor_map_to_diag() */


/* Decode a CBOR item from stream at offset.
 *
 * item->type is set to one of the main CBOR types and denotes the
 * original type regardless of representation in cbor_item_t.
 *
 * item->length is the element count for maps and arrays, the number
 * of bytes for string types and is set to 0 for other types.
 *
 * Returns length of serialized item.
 */
size_t
cbor_decode_item (cbor_stream_t *stream, size_t offset, cbor_item_t *item)
{
  size_t readbytes = 0;
  uint64_t u64val;
  float fval;

  if (!stream)
    return 0;

  if (item)
  {
    item->value.i = 0;
    item->type = -1;
    item->length = 0;
    item->offset = offset;
  }

  switch (CBOR_TYPE (stream, offset))
  {
  case CBOR_UINT:
    readbytes = decode_int (stream, offset, (item) ? &u64val : NULL);
    if (item)
    {
      if (u64val > INT64_MAX)
        ms_log (2, "cbor_decode_item(): uint64_t too large for int64_t item value: %" PRIu64 "\n", u64val);
      item->value.i = u64val;
      item->type = CBOR_UINT;
    }
    break;

  case CBOR_NEGINT:
    readbytes = decode_int (stream, offset, (item) ? &u64val : NULL);
    if (item)
    {
      item->value.i = -1 - u64val; /* Resolve to negative int */
      item->type = CBOR_NEGINT;
    }
    break;

  case CBOR_BYTES:
  case CBOR_TEXT:
    readbytes = decode_bytes_no_copy (stream, offset,
                                      (item) ? &item->value.c : NULL,
                                      (item) ? &item->length : NULL);
    if (item)
      item->type = (CBOR_TYPE (stream, offset) == CBOR_BYTES) ? CBOR_BYTES : CBOR_TEXT;
    break;

  case CBOR_ARRAY:
    if (stream->data[offset] == (CBOR_ARRAY | CBOR_VAR_FOLLOWS))
      readbytes = cbor_deserialize_array_indefinite (stream, offset);
    else
      readbytes = cbor_deserialize_array (stream, offset, (item) ? &item->length: NULL);
    if (item)
      item->type = CBOR_ARRAY;
    break;

  case CBOR_MAP:
    if (stream->data[offset] == (CBOR_MAP | CBOR_VAR_FOLLOWS))
      readbytes = cbor_deserialize_map_indefinite (stream, offset);
    else
      readbytes = cbor_deserialize_map (stream, offset, (item) ? &item->length: NULL);
    if (item)
      item->type = CBOR_MAP;
    break;

  case CBOR_TAG:
    readbytes = 1;
    if (item)
      item->type = CBOR_TAG;
    break;

  case CBOR_7:
    switch (stream->data[offset])
    {
    case CBOR_FALSE:
      readbytes = 1;
      if (item)
        item->type = CBOR_FALSE;
      break;

    case CBOR_TRUE:
      readbytes = 1;
      if (item)
        item->type = CBOR_TRUE;
      break;

    case CBOR_NULL:
      readbytes = 1;
      if (item)
        item->type = CBOR_NULL;
      break;

    case CBOR_UNDEFINED:
      readbytes = 1;
      if (item)
        item->type = CBOR_UNDEFINED;
      break;

    case CBOR_FLOAT16:
      readbytes = cbor_deserialize_float_half (stream, offset, (item) ? &fval : NULL);
      if (item)
      {
        item->value.d = fval;
        item->type = CBOR_FLOAT16;
      }
      break;

    case CBOR_FLOAT32:
      readbytes = cbor_deserialize_float (stream, offset, (item) ? &fval : NULL);
      if (item)
      {
        item->value.d = fval;
        item->type = CBOR_FLOAT32;
      }
      break;

    case CBOR_FLOAT64:
      readbytes = cbor_deserialize_double (stream, offset, (item) ? &item->value.d : NULL);
      if (item)
        item->type = CBOR_FLOAT64;
      break;

    case CBOR_BREAK:
      readbytes = 1;
      if (item)
        item->type = CBOR_BREAK;
      break;
    }
    break;

  default:
    ms_log (2, "cbor_decode_item(): Unrecognized CBOR type: 0x%X\n", CBOR_TYPE (stream, offset));
  }

  return readbytes;
} /* End of cbor_decode_item() */

/*
 * Fetch the key and/or value of a key-value pair identified with a
 * path, where a path is a series of keys in potentially nested Maps.
 *
 * Map keys cannot be containers.
 */
size_t
cbor_fetch_map_value (cbor_stream_t *stream, size_t offset,
                      cbor_item_t *value, const char *path[])
{
  cbor_item_t currentitem;
  cbor_item_t keyitem;
  cbor_item_t valueitem;
  size_t keyoffset = 0;
  size_t readbytes = 0;
  size_t keybytes = 0;
  size_t valuebytes = 0;
  size_t containerlength;
  int follow;

  if (!stream || !path)
    return 0;

  /* Sanity check that at least one path element is specified */
  if (!path[0])
    return 0;

  offset += readbytes = cbor_decode_item(stream, offset, &currentitem);

  /* Check for indefinite Map|Array, which are not supported */
  if (stream->data[offset] == (CBOR_MAP | CBOR_VAR_FOLLOWS) ||
      stream->data[offset] == (CBOR_ARRAY | CBOR_VAR_FOLLOWS))
  {
    ms_log (2, "cbor_fetch_map_value(): Provided CBOR contains an indefinite Map/Array, not supported\n");
    return 0;
  }

  /* Iterate through Arrays */
  if (currentitem.type == CBOR_ARRAY)
  {
    containerlength = currentitem.length;

    while (containerlength > 0)
    {
      offset += keybytes = cbor_fetch_map_value (stream, offset, NULL, path);

      if (!keybytes)
      {
        ms_log (2, "cbor_fetch_map_value(): Cannot decode Array element\n");
        return 0;
      }

      readbytes += keybytes;
      containerlength--;
    }
  }
  /* Iterate through Maps, keys cannot be Array or Map */
  if (currentitem.type == CBOR_MAP)
  {
    containerlength = currentitem.length;

    while (containerlength > 0)
    {
      keyoffset = offset;
      offset += keybytes = cbor_decode_item(stream, offset, &keyitem);

      if (!keybytes)
      {
        ms_log (2, "cbor_fetch_map_value(): Cannot decode Map key\n");
        return 0;
      }

      valuebytes = cbor_decode_item(stream, offset, &valueitem);

      if (!valuebytes)
      {
        ms_log (2, "cbor_fetch_map_value(): Cannot decode Map value\n");
        return 0;
      }

      /* Determine if this value matches the path */
      follow = 0;
      if (keyitem.type == CBOR_TEXT)
      {
        /* Compare to key Text to first path element */
        if (!strncmp(path[0], (char *)keyitem.value.c, keyitem.length))
        {
          /* If this is the final path element, store in provided items */
          if (!path[1])
          {
            if (value)
              cbor_decode_item(stream, offset, value);
            return 0;
          }

          follow = 1;
        }
      }

      /* Consume value item, potentially recursing into Array or Map */
      if (follow)
        offset += valuebytes = cbor_fetch_map_value (stream, offset, value, &path[1]);
      else
        offset += valuebytes = cbor_fetch_map_value (stream, offset, NULL, path);

      readbytes += keybytes + valuebytes;
      containerlength--;
    } /* Done looping through Map elements */
  } /* type == CBOR_MAP */

  return readbytes;
} /* End of cbor_fetch_map_value() */

/*
 * Set the value of a key-value pair identified with a path, where a
 * path is a series of keys in potentially nested Maps.
 *
 * This operation involves re-creating the CBOR structure and
 * replacing the extra header buffer.
 *
 * All Maps and the final key-value pair are created if nessesary.
 *
 * Map keys cannot be containers.
 *
 * Returns 0 on success and non-zero otherwise.
 */
size_t
cbor_set_map_value (cbor_stream_t *stream, cbor_item_t *item, const char *path[])
{
  uint8_t newextra[65535];
  //cbor_item_t mapkey;
  cbor_item_t mapvalue;
  size_t readbytes = 0;
  size_t containerlength;

  size_t readoffset = 0;
  size_t writeoffset = 0;

  const char **mappath;
  cbor_item_t **pathitems;

  // here, pathitems should be a list of values (containers) of the path.

  int pathcount;
  int pathidx;

  if (!stream || !item || !path)
    return 0;

  // Search for last existing Map along path, get offset to last container
  // Add new maps as needed, set to contain single pair(1)
  // Change or create key-value pair.

  /* Sanity check that at least one path element is specified */
  if (!path[0])
    return -1;

  /* Count elements in path and allocate mirror array */
  pathcount = 0;
  while (path[pathcount])
    pathcount++;

  mappath = (const char **)malloc ((pathcount + 1) * sizeof (char *));

  if (!mappath)
  {
    ms_log (2, "cbor_set_map_value(): Cannot allocate memory for map path\n");
    return -1;
  }

  printf ("DB: pathcount: %d\n", pathcount);

  /* Find the deepest existing Map in path */
  pathidx = 0;
  while (path[pathidx])
  {
    mappath[pathidx] = path[pathidx];
    mappath[pathidx + 1] = NULL;

    mapvalue.type = -1;
    cbor_fetch_map_value (stream, 0, &mapvalue, mappath);

    if (mapvalue.type == -1)
    {
      pathidx--;
      break;
    }

    pathidx++;
  }

  printf ("DB: deepest existing is index: %d -> '%s'\n", pathidx, (pathidx >= 0) ? path[pathidx-1] : "ROOT");

  if (mappath)
    free (mappath);

  return writeoffset;
} /* End of cbor_set_map_value() */

/* Print a cbor_item_t value.
 *
 * Return number of bytes printed.
 */
size_t
cbor_print_item (cbor_item_t *item, int indent, char *prefix, char *suffix)
{
  size_t printed = 0;

  if (!item)
    return 0;

  if (indent > 0)
    printed += printf ("%*s", indent, "");

  if (prefix)
    printed += printf ("%s", prefix);

  switch (item->type)
  {
  case CBOR_UINT:
  case CBOR_NEGINT:
    printed += printf ("%" PRIi64, item->value.i);
    break;

  case CBOR_BYTES:
  case CBOR_TEXT:
    printed += printf ("%.*s", (int)item->length, item->value.c);
    break;

  case CBOR_ARRAY:
    printed += printf ("ARRAY");
    break;

  case CBOR_MAP:
    printed += printf ("MAP");
    break;

  case CBOR_TAG:
    printed += printf ("TAG");
    break;

  case CBOR_FALSE:
    printed += printf ("FALSE");
    break;

  case CBOR_TRUE:
    printed += printf ("TRUE");
    break;

  case CBOR_NULL:
    printed += printf ("NULL");
    break;

  case CBOR_UNDEFINED:
    printed += printf ("UNDEFINED");
    break;

  case CBOR_FLOAT16:
  case CBOR_FLOAT32:
  case CBOR_FLOAT64:
    printed += printf ("%g", item->value.d);
    break;

  case CBOR_BREAK:
    printed += printf ("BREAK");
    break;

  default:
    ms_log (2, "cbor_print_item(): Unrecognized CBOR type: %d\n", item->type);
  }

  if (suffix)
    printed += printf ("%s", suffix);

  return printed;
} /* End of cbor_print_item() */
