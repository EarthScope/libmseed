/***************************************************************************
 * cbor.h:
 *
 * CBOR handling routines extracted from RIOT-OS:
 * https://github.com/RIOT-OS/RIOT
 *
 * All copyrights and license notifications are retained below.
 *
 * This version started with commit
 * 29e8bd1351666e36e7d6f237753d29d4f89ba84e to RIOT-OS and has been
 * modified to work in libmseed.
 ***************************************************************************/

/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2014 Kevin Funk <kfunk@kde.org>
 * Copyright (C) 2014 Jana Cavojska <jana.cavojska9@gmail.com>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_cbor    CBOR
 * @ingroup     sys
 *
 * @brief CBOR serializer/deserializer
 *
 * This is an implementation suited for constrained devices
 * Characteristics:
 * - No dynamic memory allocation (i.e. no calls to `malloc()`, `free()`) used
 *   throughout the implementation
 * - User may allocate static buffers, this implementation uses the space
 *   provided by them (cf. @ref cbor_stream_t)
 *
 * # Supported types (categorized by major type (MT))
 *
 * - Major type 0 (unsigned integer): Full support. Relevant functions:
 *   - cbor_serialize_int(), cbor_deserialize_int()
 *   - cbor_serialize_uint64_t(), cbor_deserialize_uint64_t()
 *
 * - Major type 1 (negative integer): Full support. Relevant functions:
 *   - cbor_serialize_int(), cbor_deserialize_int()
 *   - cbor_serialize_int64_t(), cbor_deserialize_int64_t()
 *
 * - Major type 2 (byte string): Full support. Relevant functions:
 *   - cbor_serialize_byte_string(), cbor_deserialize_byte_string()
 *
 * - Major type 3 (unicode string): Basic support (see below). Relevant functions:
 *   - cbor_serialize_unicode_string(), cbor_deserialize_unicode_string()
 *
 * - Major type 4 (array of data items): Full support. Relevant functions:
 *   - cbor_serialize_array(), cbor_deserialize_array()
 *   - cbor_serialize_array_indefinite(), cbor_deserialize_array_indefinite(),
 *     cbor_at_break()
 *
 * - Major type 5 (map of pairs of data items): Full support. Relevant functions:
 *   - cbor_serialize_map(), cbor_deserialize_map()
 *   - cbor_serialize_map_indefinite(), cbor_deserialize_map_indefinite(),
 *     cbor_at_break()
 *
 * - Major type 6 (optional semantic tagging of other major types): Basic
 *   support (see below). Relevant functions:
 *   - cbor_write_tag()
 *
 * - Major type 7 (floating-point numbers and values with no content): Basic
 *   support (see below). Relevant functions:
 *   - cbor_serialize_float_half(), cbor_deserialize_float_half()
 *   - cbor_serialize_float(), cbor_deserialize_float()
 *   - cbor_serialize_double(), cbor_deserialize_double()
 *   - cbor_serialize_bool(), cbor_deserialize_bool()
 *
 * ## Notes about major type 3
 * Since we do not have a standardised C type for representing Unicode code
 * points, we just provide API to serialize/deserialize `char` arrays. The
 * user then has to transform that into a meaningful representation
 *
 * Since we do not have C types for representing
 * bignums/bigfloats/decimal-fraction we do not provide API to
 * serialize/deserialize them at all. You can still read out the actual data
 * item behind the tag (via cbor_deserialize_byte_string()) and interpret it
 * yourself.
 *
 * @see [RFC7049, section 2.4](https://tools.ietf.org/html/rfc7049#section-2.4)
 *
 * # Notes about major type 7 and simple values
 * Simple values:
 *
 * | Simple value types      | Support                                                        |
 * | :---------------------- | :------------------------------------------------------------- |
 * |   0-19: (Unassigned)    | not supported                                                  |
 * |  20,21: True, False     | supported (see cbor_serialize_bool(), cbor_deserialize_bool()) |
 * |  22,23: Null, Undefined | not supported (what's the use-case?)                           |
 * |  24-31: (Reserved)      | not supported                                                  |
 * | 32-255: (Unassigned)    | not supported                                                  |
 *
 * @see [RFC7049, section 2.4](https://tools.ietf.org/html/rfc7049#section-2.3)
 *
 * @todo API for Indefinite-Length Byte Strings and Text Strings
 *       (see https://tools.ietf.org/html/rfc7049#section-2.2.2)
 * @{
 */

/**
 * @file
 * @brief       CBOR definitions
 *
 * @author      Kevin Funk <kfunk@kde.org>
 * @author      Jana Cavojska <jana.cavojska9@gmail.com>
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 *
 */

#ifndef CBOR_H
#define CBOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fundamental CBOR definitions */

#define CBOR_TYPE_MASK          0xE0    /* top 3 bits */
#define CBOR_INFO_MASK          0x1F    /* low 5 bits */

#define CBOR_BYTE_FOLLOWS       24      /* indicator that the next byte is part of this item */

/* Jump Table for Initial Byte (cf. table 5) */
#define CBOR_UINT       0x00            /* type 0 */
#define CBOR_NEGINT     0x20            /* type 1 */
#define CBOR_BYTES      0x40            /* type 2 */
#define CBOR_TEXT       0x60            /* type 3 */
#define CBOR_ARRAY      0x80            /* type 4 */
#define CBOR_MAP        0xA0            /* type 5 */
#define CBOR_TAG        0xC0            /* type 6 */
#define CBOR_7          0xE0            /* type 7 (float and other types) */

/* Major types (cf. section 2.1) */
/* Major type 0: Unsigned integers */
#define CBOR_UINT8_FOLLOWS      24      /* 0x18 */
#define CBOR_UINT16_FOLLOWS     25      /* 0x19 */
#define CBOR_UINT32_FOLLOWS     26      /* 0x1a */
#define CBOR_UINT64_FOLLOWS     27      /* 0x1b */

/* Indefinite Lengths for Some Major types (cf. section 2.2) */
#define CBOR_VAR_FOLLOWS        31      /* 0x1f */

/* Major type 6: Semantic tagging */
#define CBOR_DATETIME_STRING_FOLLOWS        0
#define CBOR_DATETIME_EPOCH_FOLLOWS         1

/* Major type 7: Float and other types */
#define CBOR_FALSE      (CBOR_7 | 20)
#define CBOR_TRUE       (CBOR_7 | 21)
#define CBOR_NULL       (CBOR_7 | 22)
#define CBOR_UNDEFINED  (CBOR_7 | 23)
/* CBOR_BYTE_FOLLOWS == 24 */
#define CBOR_FLOAT16    (CBOR_7 | 25)
#define CBOR_FLOAT32    (CBOR_7 | 26)
#define CBOR_FLOAT64    (CBOR_7 | 27)
#define CBOR_BREAK      (CBOR_7 | 31)

#define CBOR_TYPE(stream, offset) (stream->data[offset] & CBOR_TYPE_MASK)
#define CBOR_ADDITIONAL_INFO(stream, offset) (stream->data[offset] & CBOR_INFO_MASK)

/* A general container for a CBOR item */
typedef struct cbor_item_s {
  union {
    unsigned char *c;
    int64_t i;
    double d;
  } value;
  int type;
  size_t length;
  size_t offset;
} cbor_item_t;

/**
 * @brief Struct containing CBOR-encoded data
 *
 * A typical usage of CBOR looks like:
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.c}
 * unsigned char data[1024];
 * cbor_stream_t stream;
 * cbor_init(&stream, data, sizeof(data));
 *
 * cbor_serialize_int(&stream, 5);
 * // (...)
 * // <data contains CBOR encoded items now>
 *
 * cbor_destroy(&stream);
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @see cbor_init
 * @see cbor_clear
 * @see cbor_destroy
 */
typedef struct {
    unsigned char *data;    /**< Array containing CBOR encoded data */
    size_t size;            /**< Size of the array */
    size_t pos;             /**< Index to the next free byte */
} cbor_stream_t;

/**
 * @brief Initialize cbor struct
 *
 * @note Does *not* take ownership of @p buffer
 * @param[in] stream The cbor struct to initialize
 * @param[in] buffer The buffer used for storing CBOR-encoded data
 * @param[in] size   The size of buffer @p buffer
 */
void cbor_init(cbor_stream_t *stream, unsigned char *buffer, size_t size);

/**
 * @brief Clear cbor struct
 *
 *        Sets pos to zero
 *
 * @param[in, out] stream Pointer to the cbor struct
 */
void cbor_clear(cbor_stream_t *stream);

/**
 * @brief Destroy the cbor struct
 *
 * @note Does *not* free data
 *
 * @param[in, out] stream Pointer to the cbor struct
 */
void cbor_destroy(cbor_stream_t *stream);

/**
 * @brief Serializes an integer
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The integer to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_int(cbor_stream_t *stream, int val);

/**
 * @brief Deserialize integers from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_int(const cbor_stream_t *stream, size_t offset,
                            int *val);

/**
 * @brief Serializes an unsigned 64 bit value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The 64 bit integer to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_uint64_t(cbor_stream_t *stream, uint64_t val);

/**
 * @brief Deserialize unsigned 64 bit values from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_uint64_t(const cbor_stream_t *stream, size_t offset,
                                 uint64_t *val);

/**
 * @brief Serializes a signed 64 bit value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The 64 bit integer to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_int64_t(cbor_stream_t *stream, int64_t val);

/**
 * @brief Deserialize signed 64 bit values from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_int64_t(const cbor_stream_t *stream, size_t offset,
                                int64_t *val);

/**
 * @brief Serializes a boolean value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The boolean value to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_bool(cbor_stream_t *stream, bool val);

/**
 * @brief Deserialize boolean values from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_bool(const cbor_stream_t *stream, size_t offset,
                             bool *val);

/**
 * @brief Serializes a half-width floating point value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The half-width floating point value to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_float_half(cbor_stream_t *stream, float val);

/**
 * @brief Deserialize half-width floating point values values from @p stream to
 *        @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_float_half(const cbor_stream_t *stream, size_t offset,
                                   float *val);
/**
 * @brief Serializes a floating point value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The float to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_float(cbor_stream_t *stream, float val);

/**
 * @brief Deserialize floating point values values from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_float(const cbor_stream_t *stream, size_t offset,
                              float *val);
/**
 * @brief Serializes a double precision floating point value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The double to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_double(cbor_stream_t *stream, double val);

/**
 * @brief Deserialize double precision floating point valeus from @p stream to
 *        @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_double(const cbor_stream_t *stream, size_t offset,
                               double *val);

/**
 * @brief Serializes a signed 64 bit value
 *
 * @param[out] stream   The destination stream for serializing the array
 * @param[in] val       The 64 bit integer to serialize
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_byte_string(cbor_stream_t *stream, const char *val);

/**
 * @brief Serializes an arbitrary byte string
 *
 * @param[out] stream   The destination stream for serializing the byte stream
 * @param[in] val       The arbitrary byte string which may include null bytes
 * @param[in] length    The size of the byte string in bytes
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_byte_stringl(cbor_stream_t *stream, const char *val, size_t length);

/**
 * @brief Deserialize bytes from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 * @param[in] length Length of destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_byte_string(const cbor_stream_t *stream, size_t offset,
                                    char *val, size_t length);

/**
 * @brief Serializes a unicode string.
 *
 * @param[out] stream   The destination stream for serializing the unicode
 *                      string
 * @param[out] val      The zero-terminated unicode string to serialize.
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_unicode_string(cbor_stream_t *stream, const char *val);

/**
 * @brief Deserialize bytes from @p stream to @p val (without copy)
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to a char *
 * @param[out] length Pointer to a size_t to store the size of the string
 *
 * @return Number of bytes written into @p val
 */
size_t cbor_deserialize_byte_string_no_copy(const cbor_stream_t *stream, size_t offset,
                                            unsigned char **val, size_t *length);

/**
 * @brief Deserialize unicode string from @p stream to @p val (without copy)
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to a char *
 * @param[out] length Pointer to a size_t to store the size of the string
 *
 * @return Number of bytes written into @p val
 */
size_t cbor_deserialize_unicode_string_no_copy(const cbor_stream_t *stream, size_t offset,
                                               unsigned char **val, size_t *length);

/**
 * @brief Deserialize unicode string from @p stream to @p val
 *
 * @param[in] stream The stream to deserialize
 * @param[in] offset The offset within the stream where to start deserializing
 * @param[out] val   Pointer to destination array
 * @param[in] length Length of destination array
 *
 * @return Number of bytes read from @p stream
 */
size_t cbor_deserialize_unicode_string(const cbor_stream_t *stream,
                                       size_t offset, char *val, size_t length);

/**
 * @brief Serialize array of length @p array_length
 *
 * Basic usage:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.c}
 * cbor_serialize_array(&stream, 2); // array of length 2 follows
 * cbor_serialize_int(&stream, 1)); // write item 1
 * cbor_serialize_int(&stream, 2)); // write item 2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @note You have to make sure to serialize the correct amount of items.
 * If you exceed the length @p array_length, items will just be appened as normal
 *
 * @param[out] stream       The destination stream for serializing the array
 * @param[in]  array_length Length of the array of items which follows
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_array(cbor_stream_t *stream, size_t array_length);

/**
 * @brief Deserialize array of items
 *
 * Basic usage:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.c}
 * size_t array_length;
 * // read out length of the array
 * size_t offset = cbor_deserialize_array(&stream, 0, &array_length);
 * int i1, i2;
 * offset += cbor_deserialize_int(&stream, offset, &i1); // read item 1
 * offset += cbor_deserialize_int(&stream, offset, &i2); // read item 2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @param[in] stream       The stream to deserialize
 * @param[in] offset       The offset within the stream
 * @param[in] array_length Where the array length is stored
 *
 * @return Number of deserialized bytes from stream
 */
size_t cbor_deserialize_array(const cbor_stream_t *stream, size_t offset,
                              size_t *array_length);

/**
 * @brief Serialize array of infite length
 *
 * @param[out] stream       The destination stream for serializing the array
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_array_indefinite(cbor_stream_t *stream);

/**
 * @brief Deserialize array of items
 *
 * @param[in] stream       The stream to deserialize
 * @param[in] offset       The offset within the stream
 *
 * @return Number of deserialized bytes from stream
 */
size_t cbor_deserialize_array_indefinite(const cbor_stream_t *stream, size_t offset);

/**
 * @brief Serialize map of length @p map_length
 *
 * Basic usage:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.c}
 * cbor_serialize_map(&stream, 2); // map of length 2 follows
 * cbor_serialize_int(&stream, 1)); // write key 1
 * cbor_serialize_byte_string(&stream, "1")); // write value 1
 * cbor_serialize_int(&stream, 2)); // write key 2
 * cbor_serialize_byte_string(&stream, "2")); // write value 2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @param[out] stream  The destination stream for serializing the map
 * @param map_length Length of the map of items which follows
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_map(cbor_stream_t *stream, size_t map_length);

/**
 * @brief Deserialize map of items
 *
 * Basic usage:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ {.c}
 * size_t map_length;
 * // read out length of the map
 * size_t offset = cbor_deserialize_map(&stream, 0, &map_length);
 * int key1, key1;
 * char value1[8], value2[8];
 * // read key 1
 * offset += cbor_deserialize_int(&stream, offset, &key1);
 * // read value 1
 * offset += cbor_deserialize_byte_string(&stream, offset, value1, sizeof(value));
 * // read key 2
 * offset += cbor_deserialize_int(&stream, offset, &key2);
 * // read value 2
 * offset += cbor_deserialize_byte_string(&stream, offset, value2, sizeof(value));
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @param[in] stream     The stream to deserialize
 * @param[in] offset     The offset within the stream where to start deserializing
 * @param[in] map_length Where the array length is stored
 *
 * @return Number of deserialized bytes from @p stream
 */
size_t cbor_deserialize_map(const cbor_stream_t *stream, size_t offset,
                            size_t *map_length);

/**
 * @brief Serialize map of infite length
 *
 * @param[out] stream       The destination stream for serializing the map
 *
 * @return Number of bytes written to stream @p stream
 */
size_t cbor_serialize_map_indefinite(cbor_stream_t *stream);

/**
 * @brief Deserialize map of items
 *
 * @param[in] stream       The stream to deserialize
 * @param[in] offset       The offset within the stream
 *
 * @return Number of deserialized bytes from stream
 */
size_t cbor_deserialize_map_indefinite(const cbor_stream_t *stream, size_t offset);

/**
 * @brief Write a tag to give the next CBOR item additional semantics
 *
 * Also see https://tools.ietf.org/html/rfc7049#section-2.4 (Optional Tagging of Items)
 *
 * @param[in, out] stream Pointer to the cbor struct
 * @param[in]      tag    The tag to write
 *
 * @return Always 1
 */
size_t cbor_write_tag(cbor_stream_t *stream, unsigned char tag);

/**
 * @brief Whether we are at a tag symbol in stream @p stream at offset @p offset
 *
 * @param[in] stream Pointer to the cbor struct
 * @param[in] offset The offset within @p stream
 *
 * @return True in case there is a tag symbol at the current offset
 */
bool cbor_at_tag(const cbor_stream_t *stream, size_t offset);

/**
 * @brief Write a break symbol at the current offset in stream @p stream
 *
 * Used for marking the end of indefinite length CBOR items
 *
 * @param[in] stream  Pointer to the cbor struct
 *
 * @return Always 1
 */
size_t cbor_write_break(cbor_stream_t *stream);

/**
 * @brief Whether we are at a break symbol in stream @p stream at offset @p offset
 *
 * @param[in] stream  Pointer to the cbor struct
 * @param[in] offset  The offset within @p stream
 *
 * @return True in case the there is a break symbol at the current offset
 */
bool cbor_at_break(const cbor_stream_t *stream, size_t offset);

/**
 * @brief Whether we are at the end of the stream @p stream at offset @p offset
 *
 * Useful for abort conditions in loops while deserializing CBOR items
 *
 * @param[in] stream  Pointer to the cbor struct
 * @param[in] offset  The offset within @p stream
 *
 * @return True in case @p offset marks the end of the stream
 */
bool cbor_at_end(const cbor_stream_t *stream, size_t offset);

/**
 * @brief Print a CBOR Map structure starting at stream @p stream at offset @p offset
 *
 * Useful for abort conditions in loops while deserializing CBOR items
 *
 * @param[in] stream  Pointer to the cbor struct
 * @param[in] offset  The offset within @p stream
 * @param[in] maxstringprint Maximum characters to print for TEXT or BYTES values
 *
 * @param[out] output  Pointer to the output buffer
 * @param[out] output  Pointer to the end of the output buffer
 *
 * @return Size of last item parsed and printed
 */
size_t cbor_map_to_diag (cbor_stream_t *stream, size_t offset, int maxstringprint,
                         char *output, size_t *outputoffset, const size_t outputmax);


size_t cbor_decode_item (cbor_stream_t *stream, size_t offset, cbor_item_t *item);

size_t cbor_fetch_map_value (cbor_stream_t *stream, size_t offset,
                             cbor_item_t *value, const char *path[]);

size_t cbor_set_map_value (cbor_stream_t *stream, cbor_item_t *item, const char *path[]);

size_t cbor_print_item (cbor_item_t *item, int indent, char *prefix, char *suffix);


#ifdef __cplusplus
}
#endif

#endif /* CBOR_H */

/** @} */