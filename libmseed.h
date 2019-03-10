/**********************************************************************/ /**
 * @file libmseed.h:
 *
 * Interface declarations for the miniSEED library (libmseed).
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License (GNU-LGPL) for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2019:
 * @author Chad Trabant, IRIS Data Management Center
 * @copyright GNU Public License
 ***************************************************************************/

#ifndef LIBMSEED_H
#define LIBMSEED_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define LIBMSEED_VERSION "3.0.0"        //!< Library version
#define LIBMSEED_RELEASE "2019.XXXdev"  //!< Library release date

/* C99 standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
  #define LMP_WIN 1
#endif

/* Set platform specific features, Windows versus everything else */
#if defined(LMP_WIN)
  #include <windows.h>
  #include <sys/types.h>

  /* For MSVC 2012 and earlier define standard int types, otherwise use inttypes.h */
  #if defined(_MSC_VER) && _MSC_VER <= 1700
    typedef signed char int8_t;
    typedef unsigned char uint8_t;
    typedef signed short int int16_t;
    typedef unsigned short int uint16_t;
    typedef signed int int32_t;
    typedef unsigned int uint32_t;
    typedef signed __int64 int64_t;
    typedef unsigned __int64 uint64_t;
  #else
    #include <inttypes.h>
  #endif

  /* For MSVC define PRId64 and SCNd64 if needed */
  #if defined(_MSC_VER)
    #if !defined(PRId64)
      #define PRId64 "I64d"
    #endif
    #if !defined(SCNd64)
      #define SCNd64 "I64d"
    #endif

    #define snprintf _snprintf
    #define vsnprintf _vsnprintf
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
    #define strtoull _strtoui64
    #define strdup _strdup
    #define fileno _fileno
  #endif

  /* Extras needed for MinGW */
  #if defined(__MINGW32__) || defined(__MINGW64__)
    #include <fcntl.h>

    #define fstat _fstat
    #define stat _stat
  #endif
#else
  /* All other platforms */
  #include <inttypes.h>
#endif

#define MINRECLEN 40       //!< Minimum miniSEED record length supported
#define MAXRECLEN 131172   //!< Maximum miniSEED record length supported

#define LM_SIDLEN 64       //!< Length of source ID string

/** @defgroup encoding-values Data encodings
    @brief Data encoding type defines
    @{ */
#define DE_ASCII       0            //!< ASCII (text) encoding
#define DE_INT16       1            //!< 16-bit integer
#define DE_INT32       3            //!< 32-bit integer
#define DE_FLOAT32     4            //!< 32-bit float (IEEE)
#define DE_FLOAT64     5            //!< 64-bit float (IEEE)
#define DE_STEIM1      10           //!< Steim-1 compressed integers
#define DE_STEIM2      11           //!< Steim-2 compressed integers
#define DE_GEOSCOPE24  12           //!< [Legacy] GEOSCOPE 24-bit integer
#define DE_GEOSCOPE163 13           //!< [Legacy] GEOSCOPE 16-bit gain ranged, 3-bit exponent
#define DE_GEOSCOPE164 14           //!< [Legacy] GEOSCOPE 16-bit gain ranged, 4-bit exponent
#define DE_CDSN        16           //!< [Legacy] CDSN 16-bit gain ranged
#define DE_SRO         30           //!< [Legacy] SRO 16-bit gain ranged
#define DE_DWWSSN      32           //!< [Legacy] DWWSSN 16-bit gain ranged
/** @} */

/** @defgroup return-values Return codes
    @brief Common error codes returned by library functions.  Error values will always be negative.

    \sa ms_errorstr()
    @{ */
#define MS_ENDOFFILE        1        //!< End of file reached return value
#define MS_NOERROR          0        //!< No error
#define MS_GENERROR        -1        //!< Generic unspecified error
#define MS_NOTSEED         -2        //!< Data not SEED
#define MS_WRONGLENGTH     -3        //!< Length of data read was not correct
#define MS_OUTOFRANGE      -4        //!< SEED record length out of range
#define MS_UNKNOWNFORMAT   -5        //!< Unknown data encoding format
#define MS_STBADCOMPFLAG   -6        //!< Steim, invalid compression flag(s)
#define MS_INVALIDCRC      -7        //!< Invalid CRC
/** @} */

/** @def MS_ISRATETOLERABLE
    @brief Macro to test default sample rate tolerance: abs(1-sr1/sr2) < 0.0001 */
#define MS_ISRATETOLERABLE(A,B) (ms_dabs (1.0 - (A / B)) < 0.0001)

/** @def MS2_ISDATAINDICATOR
    @brief Macro to test a character for miniSEED 2.x data record/quality indicators */
#define MS2_ISDATAINDICATOR(X) (X=='D' || X=='R' || X=='Q' || X=='M')

/** @def MS3_ISVALIDHEADER
 * @hideinitializer
 * Macro to test a buffer for a miniSEED 3.x data record signature by checking
 * header values at known byte offsets:
 * - 0  = "M"
 * - 1  = "S"
 * - 2  = 3
 * - 12 = valid hour (0-23)
 * - 13 = valid minute (0-59)
 * - 14 = valid second (0-60)
 *
 * Usage, X buffer must contain at least 15 bytes:
 * @code
 *   MS3_ISVALIDHEADER ((char *)X)
 * @endcode
 */
#define MS3_ISVALIDHEADER(X) (                                   \
    *(X) == 'M' && *(X + 1) == 'S' && *(X + 2) == 3 &&           \
    (uint8_t) (*(X + 12)) >= 0 && (uint8_t) (*(X + 12)) <= 23 && \
    (uint8_t) (*(X + 13)) >= 0 && (uint8_t) (*(X + 13)) <= 59 && \
    (uint8_t) (*(X + 14)) >= 0 && (uint8_t) (*(X + 14)) <= 60)

/** @def MS2_ISVALIDHEADER
 * @hideinitializer
 * Macro to test a buffer for a miniSEED 2.x data record signature by checking
 * header values at known byte offsets:
 * - [0-5]  = Digits, spaces or NULL, SEED sequence number
 * - 6  = Data record quality indicator
 * - 7  = Space or NULL [not valid SEED]
 * - 24 = Start hour (0-23)
 * - 25 = Start minute (0-59)
 * - 26 = Start second (0-60)
 *
 * Usage, X buffer must contain at least 27 bytes:
 * @code
 *   MS2_ISVALIDHEADER ((char *)X)
 * @endcode
 */
#define MS2_ISVALIDHEADER(X) (                                         \
    (isdigit ((uint8_t) * (X)) || *(X) == ' ' || !*(X)) &&             \
    (isdigit ((uint8_t) * (X + 1)) || *(X + 1) == ' ' || !*(X + 1)) && \
    (isdigit ((uint8_t) * (X + 2)) || *(X + 2) == ' ' || !*(X + 2)) && \
    (isdigit ((uint8_t) * (X + 3)) || *(X + 3) == ' ' || !*(X + 3)) && \
    (isdigit ((uint8_t) * (X + 4)) || *(X + 4) == ' ' || !*(X + 4)) && \
    (isdigit ((uint8_t) * (X + 5)) || *(X + 5) == ' ' || !*(X + 5)) && \
    MS2_ISDATAINDICATOR (*(X + 6)) &&                                  \
    (*(X + 7) == ' ' || *(X + 7) == '\0') &&                           \
    (uint8_t) (*(X + 24)) >= 0 && (uint8_t) (*(X + 24)) <= 23 &&       \
    (uint8_t) (*(X + 25)) >= 0 && (uint8_t) (*(X + 25)) <= 59 &&       \
    (uint8_t) (*(X + 26)) >= 0 && (uint8_t) (*(X + 26)) <= 60)

/** A simple bitwise AND test to return 0 or 1 */
#define bit(x,y) (x&y)?1:0

/** libmseed time type, integer nanoseconds since the Unix/POSIX epoch (00:00:00 Thursday, 1 January 1970) */
typedef int64_t nstime_t;

/** @page sample-types
    @brief Data sample types used by the library.

    Sample types are represented using a single character as follows:
    - \c 'a' - Text (ASCII) data samples
    - \c 'i' - 32-bit integer data samples
    - \c 'f' - 32-bit float (IEEE) data samples
    - \c 'd' - 64-bit float (IEEE) data samples
*/

/** @defgroup control-flags Control flags
    @brief Parsing and packing control flags
    @{ */
#define MSF_UNPACKDATA  0x0001  //!< [Parsing] unpack data samples
#define MSF_SKIPNOTDATA 0x0002  //!< [Parsing] skip input that cannot be identified as miniSEED
#define MSF_VALIDATECRC 0x0004  //!< [Parsing] validate CRC (if version 3)
#define MSF_SEQUENCE    0x0008  //!< [Packing] UNSUPPORTED: Maintain a record-level sequence number
#define MSF_FLUSHDATA   0x0010  //!< [Packing] pack all available data even if final record would not be filled
#define MSF_ATENDOFFILE 0x0020  //!< [Parsing] reading routine is at the end of the file
/** @} */

/** @defgroup byte-swap-flags Byte swap flags
    @brief Flags indicating whether the header or payload needed byte swapping
    @{ */
#define MSSWAP_HEADER   0x01    //!< Header needed byte swapping
#define MSSWAP_PAYLOAD  0x02    //!< Data payload needed byte swapping
/** @} */

/** @defgroup miniseed-record miniSEED record handling
    @brief Definitions and functions related to individual miniSEED records
    @{ */

/** miniSEED record structure */
typedef struct MS3Record {
  char           *record;            //!< Raw miniSEED record, if available
  int32_t         reclen;            //!< Length of miniSEED record in bytes
  uint8_t         swapflag;          //!< Byte swap indicator, see @ref byte-swap-flags

  /* Common header fields in accessible form */
  char            sid[LM_SIDLEN];    //!< Source identifier as URN, max length @ref LM_SIDLEN
  uint8_t         formatversion;     //!< Format major version
  uint8_t         flags;             //!< Record-level bit flags
  nstime_t        starttime;         //!< Record start time (first sample)
  double          samprate;          //!< Nominal sample rate (Hz)
  int8_t          encoding;          //!< Data encoding format
  uint8_t         pubversion;        //!< Publication version
  int64_t         samplecnt;         //!< Number of samples in record
  uint32_t        crc;               //!< CRC of entire record
  uint16_t        extralength;       //!< Length of extra headers in bytes
  uint16_t        datalength;        //!< Length of data payload in bytes
  char           *extra;             //!< Pointer to extra headers

  /* Data sample fields */
  void           *datasamples;       //!< Data samples, \a numsamples of type \a sampletype
  int64_t         numsamples;        //!< Number of data samples in datasamples
  char            sampletype;        //!< Sample type code: a, i, f, d @ref sample-types
} MS3Record;

extern int msr3_parse (char *record, uint64_t recbuflen, MS3Record **ppmsr,
                       uint32_t flags, int8_t verbose);

extern int msr3_pack (MS3Record *msr,
                      void (*record_handler) (char *, int, void *),
                      void *handlerdata, int64_t *packedsamples,
                      uint32_t flags, int8_t verbose);

extern int msr3_repack_mseed3 (MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose);

extern int msr3_pack_header3 (MS3Record *msr, char *record, uint32_t recbuflen, int8_t verbose);

extern int msr3_unpack_data (MS3Record *msr, int8_t verbose);

extern int msr3_data_bounds (MS3Record *msr, uint32_t *dataoffset, uint16_t *datasize);

extern MS3Record* msr3_init (MS3Record *msr);
extern void       msr3_free (MS3Record **ppmsr);
extern MS3Record* msr3_duplicate (MS3Record *msr, int8_t datadup);
extern nstime_t   msr3_endtime (MS3Record *msr);
extern void       msr3_print (MS3Record *msr, int8_t details);
extern double     msr3_host_latency (MS3Record *msr);

extern int ms3_detect (const char *record, int recbuflen, uint8_t *formatversion);
extern int ms_parse_raw3 (char *record, int maxreclen, int8_t details);
extern int ms_parse_raw2 (char *record, int maxreclen, int8_t details, int8_t swapflag);
/** @} */

/** @defgroup tracelist Trace lists
    @brief A container for continuous data

    Trace lists are a container to organize continuous segments of
    data.  By combining miniSEED data records into trace lists, the
    time series is reconstructed and ready for processing or
    summarization.

    A trace list container starts with an ::MS3TraceList, which
    contains one or more ::MS3TraceID entries, which each contain one
    or more ::MS3TraceSeg entries.  The ::MS3TraceID and ::MS3TraceSeg
    entries are traversed as a simple linked list.

    The overall structure is illustrated as:
      - MS3TraceList
        - MS3TraceID
          - MS3TraceSeg
          - MS3TraceSeg
          - ...
        - MS3TraceID
          - MS3TraceSeg
          - MS3TraceSeg
          - ...
        - ...
    @{ */

/** Container for a continuous trace segment, linkable */
typedef struct MS3TraceSeg {
  nstime_t        starttime;         //!< Time of first sample
  nstime_t        endtime;           //!< Time of last sample
  double          samprate;          //!< Nominal sample rate (Hz)
  int64_t         samplecnt;         //!< Number of samples in trace coverage
  void           *datasamples;       //!< Data samples, \a numsamples of type \a sampletype
  int64_t         numsamples;        //!< Number of data samples in datasamples
  char            sampletype;        //!< Sample type code, see @ref sample-types
  void           *prvtptr;           //!< Private pointer for general use, unused by library
  struct MS3TraceSeg *prev;          //!< Pointer to previous segment
  struct MS3TraceSeg *next;          //!< Pointer to next segment, NULL if the last
} MS3TraceSeg;

/** Container for a trace ID, linkable */
typedef struct MS3TraceID {
  char            sid[LM_SIDLEN];    //!< Source identifier as URN, max length @ref LM_SIDLEN
  uint8_t         pubversion;        //!< Publication version
  nstime_t        earliest;          //!< Time of earliest sample
  nstime_t        latest;            //!< Time of latest sample
  void           *prvtptr;           //!< Private pointer for general use, unused by library
  uint32_t        numsegments;       //!< Number of segments for this ID
  struct MS3TraceSeg *first;         //!< Pointer to first of list of segments
  struct MS3TraceSeg *last;          //!< Pointer to last of list of segments
  struct MS3TraceID *next;           //!< Pointer to next trace ID, NULL if the last
} MS3TraceID;

/** Container for a continuous trace segment, linkable */
typedef struct MS3TraceList {
  uint32_t           numtraces;      //!< Number of traces in list
  struct MS3TraceID *traces;         //!< Pointer to list of traces
  struct MS3TraceID *last;           //!< Pointer to last modified trace in list
} MS3TraceList;

extern MS3TraceList* mstl3_init (MS3TraceList *mstl);
extern void          mstl3_free (MS3TraceList **ppmstl, int8_t freeprvtptr);
extern MS3TraceSeg*  mstl3_addmsr (MS3TraceList *mstl, MS3Record *msr, int8_t splitversion,
                                   int8_t autoheal, double timetol, double sampratetol);
extern int64_t       mstl3_readbuffer (MS3TraceList **ppmstl, char *buffer, uint64_t bufferlength,
                                       double timetol, double sampratetol, int8_t splitversion,
                                       uint32_t flags, int8_t verbose);
extern int mstl3_convertsamples (MS3TraceSeg *seg, char type, int8_t truncate);
extern int mstl3_pack (MS3TraceList *mstl, void (*record_handler) (char *, int, void *),
                       void *handlerdata, int reclen, int8_t encoding,
                       int64_t *packedsamples, uint32_t flags, int8_t verbose,
                       char *extra);
extern void mstl3_printtracelist (MS3TraceList *mstl, int8_t timeformat,
                                  int8_t details, int8_t gaps);
extern void mstl3_printsynclist (MS3TraceList *mstl, char *dccid, int8_t subsecond);
extern void mstl3_printgaplist (MS3TraceList *mstl, int8_t timeformat,
                                double *mingap, double *maxgap);
/** @} */

/** @defgroup extra-headers Extra headers
    @brief Structures and funtions to support extra headers

    Extra headers are stored as JSON within a data record header using
    an anonymous, root object as a container for all extra headers.
    For a full description consult the format specification.

    The library functions supporting extra headers allow specific
    header identification using dot notation.  In this notation each
    path element is an object until the final element which is a
    key-value pair.

    For example, a \a path specified as:
    \code
    "objectA.objectB.header"
    \endcode

    would correspond to the single JSON value in:
    \code
    {
       "objectA": {
         "objectB": {
           "header":VALUE
          }
       }
    }
    \endcode
    @{ */

/**
 * @brief A container for event detection parameters for use in extra headers
 *
 * Actual values are optional, with special values indicating an unset
 * state.
 *
 * @see mseh_add_event_detection
 */
typedef struct MSEHEventDetection
{
  char type[30]; /**< Detector type (e.g. "MURDOCK"), zero length = not included */
  char detector[30]; /**< Detector name, zero length = not included  */
  double signalamplitude; /**< SignalAmplitude, 0.0 = not included */
  double signalperiod; /**< Signal period, 0.0 = not included */
  double backgroundestimate; /**< Background estimate, 0.0 = not included */
  char detectionwave[30]; /**< Detection wave (e.g. "DILATATION"), zero length = not included */
  char units[30]; /**< Units of amplitude and background estimate (e.g. "COUNTS"), zero length = not included */
  nstime_t onsettime; /**< Onset time, NSTERROR = not included */
  uint8_t medsnr[6]; /**< Signal to noise ratio for Murdock event detection, all zeros = not included */
  int medlookback; /**< Murdock event detection lookback value, -1 = not included */
  int medpickalgorithm; /**< Murdock event detection pick algoritm, -1 = not included */
  struct MSEHEventDetection *next; /**< Pointer to next detection, zero length if none */
} MSEHEventDetection;

/**
 * @brief A container for calibration parameters for use in extra headers
 *
 * Actual values are optional, with special values indicating an unset
 * state.
 *
 * @see mseh_add_calibration
 */
typedef struct MSEHCalibration
{
  char type[30]; /**< Calibration type  (e.g. "STEP", "SINE", "PSEUDORANDOM"), zero length = not included */
  nstime_t begintime; /**< Begin time, NSTERROR = not included */
  nstime_t endtime; /**< End time, NSTERROR = not included */
  int steps; /**< Number of step calibrations, -1 = not included */
  int firstpulsepositive; /**< Boolean, step cal. first pulse, -1 = not included */
  int alternatesign; /**< Boolean, step cal. alt. sign, -1 = not included */
  char trigger[30]; /**< Trigger, e.g. AUTOMATIC or MANUAL, zero length = not included */
  int continued; /**< Boolean, continued from prev. record, -1 = not included */
  double amplitude; /**< Amp. of calibration signal, 0.0 = not included */
  char inputunits[30]; /**< Units of input (e.g. volts, amps), zero length = not included */
  char amplituderange[30]; /**< E.g PEAKTOPTEAK, ZEROTOPEAK, RMS, RANDOM, zero length = not included */
  double duration; /**< Duration in seconds, 0.0 = not included */
  double sineperiod; /**< Period of sine, 0.0 = not included */
  double stepbetween; /**< Interval bewteen steps, 0.0 = not included */
  char inputchannel[30]; /**< Channel of input, zero length = not included */
  double refamplitude; /**< Reference amplitude, 0.0 = not included */
  char coupling[30]; /**< Coupling, e.g. Resistive, Capacitive, zero length = not included */
  char rolloff[30]; /**< Rolloff of filters, zero length = not included */
  char noise[30]; /**< Noise for PR cals, e.g. White or Red, zero length = not included */
  struct MSEHCalibration *next; /**< Pointer to next detection, zero length if none */
} MSEHCalibration;

/**
 * @brief A container for timing exception parameters for use in extra headers
 *
 * Actual values are optional, with special values indicating an unset
 * state.
 *
 * @see mseh_add_timing_exception
 */
typedef struct MSEHTimingException
{
  float vcocorrection; /**< VCO correction, from 0 to 100%, <0 = not included */
  nstime_t time; /**< Time of exception, NSTERROR = not included */
  int usec; /**< [DEPRECATED] microsecond time offset, 0 = not included */
  int receptionquality; /**< Reception quality, 0 to 100% clock accurracy, <0 = not included */
  uint32_t count; /**< The count thereof, 0 = not included */
  char type[16]; /**< E.g. "MISSING" or "UNEXPECTED", zero length = not included */
  char clockstatus[128]; /**< Description of clock-specific parameters, zero length = not included */
} MSEHTimingException;

/**
 * @brief A container for recenter parameters for use in extra headers
 *
 * Actual values are optional, with special values indicating an unset
 * state.
 *
 * @see mseh_add_recenter
 */
typedef struct MSEHRecenter
{
  char type[30]; /**< Recenter type  (e.g. "MASS", "GIMBAL"), zero length = not included */
  nstime_t begintime; /**< Begin time, NSTERROR = not included */
  nstime_t endtime; /**< Estimated end time, NSTERROR = not included */
  char trigger[30]; /**< Trigger, e.g. AUTOMATIC or MANUAL, zero length = not included */
} MSEHRecenter;

/** @def mseh_get
    @brief A simple wrapper to access any type of extra header */
#define mseh_get(msr, path, valueptr, type, maxlength) \
  mseh_get_path (msr, path, valueptr, type, maxlength)

/** @def mseh_get_number
    @brief A simple wrapper to access a number type extra header */
#define mseh_get_number(msr, path, valueptr)    \
  mseh_get_path (msr, path, valueptr, 'n', 0)

/** @def mseh_get_string
    @brief A simple wrapper to access a string type extra header */
#define mseh_get_string(msr, path, buffer, maxlength)   \
  mseh_get_path (msr, path, buffer, 's', maxlength)

/** @def mseh_get_boolean
    @brief A simple wrapper to access a boolean type extra header */
#define mseh_get_boolean(msr, path, valueptr)   \
  mseh_get_path (msr, path, valueptr, 'b', 0)

/** @def mseh_exists
    @brief A simple wrapper to test existence of an extra header */
#define mseh_exists(msr, path)                  \
  (!mseh_get_path (msr, path, NULL, 0, 0))

extern int mseh_get_path (MS3Record *msr, const char *path,
                          void *value, char type, size_t maxlength);

/** @def mseh_set
    @brief A simple wrapper to set any type of extra header */
#define mseh_set(msr, path, valueptr, type) \
  mseh_set_path (msr, path, valueptr, type)

/** @def mseh_set_number
    @brief A simple wrapper to set a number type extra header */
#define mseh_set_number(msr, path, valueptr) \
  mseh_set_path (msr, path, valueptr, 'n')

/** @def mseh_set_string
    @brief A simple wrapper to set a string type extra header */
#define mseh_set_string(msr, path, valueptr) \
  mseh_set_path (msr, path, valueptr, 's')

/** @def mseh_set_boolean
    @brief A simple wrapper to set a boolean type extra header */
#define mseh_set_boolean(msr, path, valueptr)   \
  mseh_set_path (msr, path, valueptr, 'b')

extern int mseh_set_path (MS3Record *msr, const char *path,
                          void *value, char type);

extern int mseh_add_event_detection (MS3Record *msr, const char *path,
                                     MSEHEventDetection *eventdetection);

extern int mseh_add_calibration (MS3Record *msr, const char *path,
                                 MSEHCalibration *calibration);

extern int mseh_add_timing_exception (MS3Record *msr, const char *path,
                                      MSEHTimingException *exception);

extern int mseh_add_recenter (MS3Record *msr, const char *path,
                              MSEHRecenter *recenter);

extern int mseh_print (MS3Record *msr, int indent);
/** @} */

/** @defgroup selection-functions Data selection functions
    @brief Data selections to be used as filters

    Selections are the identification of data, by source identifier
    and time ranges, that are desired.  Capability is included to read
    selections from files and to match data against a selection list.

    The ms3_readmsr_selection() and ms3_readtracelist_selection()
    routines accept ::MS3Selections and allow selective (and
    efficient) reading of data from files.
    @{ */
/** Data selection structure time window definition containers */
typedef struct MS3SelectTime {
  nstime_t starttime;                //!< Earliest data for matching channels
  nstime_t endtime;                  //!< Latest data for matching channels
  struct MS3SelectTime *next;        //!< Pointer to next selection time, NULL if the last
} MS3SelectTime;

/** Data selection structure definition containers */
typedef struct MS3Selections {
  char sidpattern[100];              //!< Matching (globbing) pattern for source ID
  struct MS3SelectTime *timewindows; //!< Pointer to time window list for this source ID
  struct MS3Selections *next;        //!< Pointer to next selection, NULL if the last
  uint8_t pubversion;                //!< Selected publication version, use 0 for any
} MS3Selections;

extern MS3Selections *ms3_matchselect (MS3Selections *selections, char *sid,
                                       nstime_t starttime, nstime_t endtime,
                                       int pubversion, MS3SelectTime **ppselecttime);
extern MS3Selections *msr3_matchselect (MS3Selections *selections, MS3Record *msr,
                                        MS3SelectTime **ppselecttime);
extern int ms3_addselect (MS3Selections **ppselections, char *sidpattern,
                          nstime_t starttime, nstime_t endtime, uint8_t pubversion);
extern int ms3_addselect_comp (MS3Selections **ppselections,
                               char *network, char* station, char *location, char *channel,
                               nstime_t starttime, nstime_t endtime, uint8_t pubversion);
extern int ms3_readselectionsfile (MS3Selections **ppselections, char *filename);
extern void ms3_freeselections (MS3Selections *selections);
extern void ms3_printselections (MS3Selections *selections);
/** @} */

/** @defgroup io-functions Reading and writing functions
    @brief miniSEED reading and writing interfaces
    @{ */

/** @brief State container for reading miniSEED records from files.
    __Callers should not modify these values directly and generally
    should not need to access them.__ */
typedef struct MS3FileParam
{
  FILE *fp;            //!< File handle
  char filename[512];  //!< File name
  char *readbuffer;    //!< Read buffer
  int readlength;      //!< Length of data in read buffer
  int readoffset;      //!< Read offset in read buffer
  int64_t filepos;     //!< File position corresponding to start of buffer
  int64_t filesize;    //!< File size
  int64_t recordcount; //!< Count of records read from this file
} MS3FileParam;

extern int ms3_readmsr (MS3Record **ppmsr, const char *msfile, int64_t *fpos, int8_t *last,
                        uint32_t flags, int8_t verbose);
extern int ms3_readmsr_r (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
                          int64_t *fpos, int8_t *last, uint32_t flags, int8_t verbose);
extern int ms3_readmsr_selection (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
                                  int64_t *fpos, int8_t *last, uint32_t flags,
                                  MS3Selections *selections, int8_t verbose);
extern int ms3_readtracelist (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                              int8_t splitversion, uint32_t flags, int8_t verbose);
extern int ms3_readtracelist_timewin (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                                      nstime_t starttime, nstime_t endtime, int8_t splitversion, uint32_t flags,
                                      int8_t verbose);
extern int ms3_readtracelist_selection (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                                        MS3Selections *selections, int8_t splitversion, uint32_t flags, int8_t verbose);
extern int msr3_writemseed (MS3Record *msr, const char *msfile, int8_t overwrite,
                            uint32_t flags, int8_t verbose);
extern int mstl3_writemseed (MS3TraceList *mst, const char *msfile, int8_t overwrite,
                             int maxreclen, int8_t encoding, uint32_t flags, int8_t verbose);
/** @} */

/** @defgroup string-functions SID and string functions
    @brief Source identifier (SID) and string manipulation functions
    @{ */
extern int      ms_sid2nslc (char *sid, char *net, char *sta, char *loc, char *chan);
extern int      ms_nslc2sid (char *sid, int sidlen, uint16_t flags,
                              char *net, char *sta, char *loc, char *chan);
extern int      ms_strncpclean (char *dest, const char *source, int length);
extern int      ms_strncpcleantail (char *dest, const char *source, int length);
extern int      ms_strncpopen (char *dest, const char *source, int length);
/** @} */

/** @defgroup time-related Time definitions and functions

    @brief Definitions and functions for related to library time values

    @{ */

/** @def NSTMODULUS
    @brief Define the high precision time tick interval as 1/modulus seconds
    corresponding to **nanoseconds**. **/
#define NSTMODULUS 1000000000

/** @def NSTERROR
    @brief Error code for routines that normally return a high precision time.
    The time value corresponds to '1902-1-1 00:00:00.00000000'. **/
#define NSTERROR -2145916800000000000LL

/** @def MS_EPOCH2NSTIME
    @brief macro to convert Unix/POSIX epoch time to high precision epoch time */
#define MS_EPOCH2NSTIME(X) X * (nstime_t) NSTMODULUS
/** @def MS_NSTIME2EPOCH
    @brief Macro to convert high precision epoch time to Unix/POSIX epoch time */
#define MS_NSTIME2EPOCH(X) X / NSTMODULUS

extern int      ms_nstime2time (nstime_t nstime, uint16_t *year, uint16_t *yday,
                                uint8_t *hour, uint8_t *min, uint8_t *sec, uint32_t *nsec);
extern char*    ms_nstime2timestr (nstime_t nstime, char *timestr, int8_t format, int8_t subsecond);
extern nstime_t ms_time2nstime (int year, int yday, int hour, int min, int sec, uint32_t nsec);
extern nstime_t ms_timestr2nstime (char *timestr);
extern nstime_t ms_seedtimestr2nstime (char *seedtimestr);
extern int      ms_doy2md (int year, int yday, int *month, int *mday);
extern int      ms_md2doy (int year, int month, int mday, int *yday);

/** \def ms_nstime2isotimestr (nstime, isotimestr, subseconds)
    @brief Legacy-like wrapper for ISO T-separated time strings */
#define ms_nstime2isotimestr(nstime, isotimestr, subseconds) (ms_nstime2timestr(nstime,isotimestr,0,subseconds))

/** \def ms_nstime2mdtimestr (nstime, isotimestr, subseconds)
    @brief Legacy-like wrapper for ISO space-separated time strings */
#define ms_nstime2mdtimestr(nstime, mdtimestr, subseconds) (ms_nstime2timestr(nstime,mdtimestr,1,subseconds))

/** \def ms_nstime2seedtimestr (nstime, isotimestr, subseconds)
    @brief Legacy-like wrapper for SEED-style time strings */
#define ms_nstime2seedtimestr(nstime, seedtimestr, subseconds) (ms_nstime2timestr(nstime,seedtimestr,2,subseconds))

/** @} */

/**********************************************************************/ /**
 * @brief Runtime test for host endianess
 * @returns 0 if the host is little endian, otherwise 1.
 ***************************************************************************/
static inline int
ms_bigendianhost (void)
{
  const uint16_t endian = 256;
  return *(const uint8_t *)&endian;
}

/**********************************************************************/ /**
 * @brief Calculate sample rate in Hertz for a given ::MS3Record
 * @returns Return sample rate in Hertz (samples per second)
 ***************************************************************************/
static inline double
msr_sampratehz (MS3Record *msr)
{
  if (msr->samprate < 0.0)
    return (-1.0 / msr->samprate);
  else
    return msr->samprate;
}

/** @defgroup logging-functions Logging functions
    @brief Central logging functions for the library and calling programs

    This central logging facility is used for all logging performed by
    the library.

    The logging can be configured to send messages to arbitrary
    functions, referred to as \c log_print() and \c diag_print().
    This allows output to be re-directed to other logging systems if
    needed.

    It is also possible to assign prefixes to log messages for
    identification, referred to as \c logprefix and \c errprefix.

    @anchor logging-levels
    Three message levels are recognized:
    - 0 : Normal log messages, printed using \c log_print() with \c logprefix
    - 1  : Diagnostic messages, printed using \c diag_print() with \c logprefix
    - 2+ : Error messages, printed using \c diag_print() with \c errprefix

    It is the task of the \c ms_log() and \c ms_log_l() functions to
    format a message using printf conventions and pass the formatted
    string to the appropriate printing function.

    @{ */

/** Maximum length of log messages in bytes */
#define MAX_LOG_MSG_LENGTH  200

/** @brief Logging parameters.
    __Callers should not modify these values directly and generally
    should not need to access them.__

    \sa ms_loginit() */
typedef struct MSLogParam
{
  void (*log_print)(char*);  //!< Function to call for regular messages
  const char *logprefix;     //!< Message prefix for regular and diagnostic messages
  void (*diag_print)(char*); //!< Function to call for diagnostic and error messages
  const char *errprefix;     //!< Message prefix for error messages
} MSLogParam;

extern int ms_log (int level, ...);
extern int ms_log_l (MSLogParam *logp, int level, ...);
extern void ms_loginit (void (*log_print)(char*), const char *logprefix,
                        void (*diag_print)(char*), const char *errprefix);
extern MSLogParam *ms_loginit_l (MSLogParam *logp,
                                 void (*log_print)(char*), const char *logprefix,
                                 void (*diag_print)(char*), const char *errprefix);
/** @} */

/** @defgroup leapsecond Leap second handling
    @{ */
/** Leap second list container */
typedef struct LeapSecond
{
  nstime_t leapsecond;       //!< Time of leap second as epoch since 1 January 1900
  int32_t  TAIdelta;         //!< TAI-UTC difference in seconds
  struct LeapSecond *next;   //!< Pointer to next entry, NULL if the last
} LeapSecond;

/** Global leap second list */
extern LeapSecond *leapsecondlist;
extern int ms_readleapseconds (const char *envvarname);
extern int ms_readleapsecondfile (const char *filename);
/** @} */

/** @defgroup utility-functions General utility functions
    @brief General utilities
    @{ */

extern uint8_t  ms_samplesize (const char sampletype);
extern const char* ms_encodingstr (const uint8_t encoding);
extern const char* ms_errorstr (int errorcode);

/** Return CRC32C value of supplied buffer, with optional starting CRC32C value */
extern uint32_t ms_crc32c (const uint8_t* input, int length, uint32_t previousCRC32C);

/** Return the absolute value of the specified double */
extern double ms_dabs (double val);

/** Portable version of POSIX ftello() to get file position in large files */
extern int64_t lmp_ftell64 (FILE *stream);
/** Portable version of POSIX fseeko() to set position in large files */
extern int lmp_fseek64 (FILE *stream, int64_t offset, int whence);

/** In-place byte swapping of 2 byte quantity */
extern void ms_gswap2 ( void *data2 );
/** In-place byte swapping of 4 byte quantity */
extern void ms_gswap4 ( void *data4 );
/** In-place byte swapping of 8 byte quantity */
extern void ms_gswap8 ( void *data8 );

/** In-place byte swapping of 2 byte, memory-aligned, quantity */
extern void ms_gswap2a ( void *data2 );
/** In-place byte swapping of 4 byte, memory-aligned, quantity */
extern void ms_gswap4a ( void *data4 );
/** In-place byte swapping of 8 byte, memory-aligned, quantity */
extern void ms_gswap8a ( void *data8 );

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LIBMSEED_H */
