/***************************************************************************
 * libmseed.h:
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
 * Copyright (C) 2017 Chad Trabant
 * IRIS Data Management Center
 ***************************************************************************/

#ifndef LIBMSEED_H
#define LIBMSEED_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define LIBMSEED_VERSION "3.0.0"
#define LIBMSEED_RELEASE "2017.XXXdev"

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

#define MINRECLEN 36       /* Minimum miniSEED record length */
#define MAXRECLEN 16777216 /* Maximum miniSEED record length, 16MB */

/* SEED data encoding types */
#define DE_ASCII       0
#define DE_INT16       1
#define DE_INT32       3
#define DE_FLOAT32     4
#define DE_FLOAT64     5
#define DE_STEIM1      10
#define DE_STEIM2      11
#define DE_GEOSCOPE24  12
#define DE_GEOSCOPE163 13
#define DE_GEOSCOPE164 14
#define DE_CDSN        16
#define DE_SRO         30
#define DE_DWWSSN      32

/* Library return and error code values, error values should always be negative */
#define MS_ENDOFFILE        1        /* End of file reached return value */
#define MS_NOERROR          0        /* No error */
#define MS_GENERROR        -1        /* Generic unspecified error */
#define MS_NOTSEED         -2        /* Data not SEED */
#define MS_WRONGLENGTH     -3        /* Length of data read was not correct */
#define MS_OUTOFRANGE      -4        /* SEED record length out of range */
#define MS_UNKNOWNFORMAT   -5        /* Unknown data encoding format */
#define MS_STBADCOMPFLAG   -6        /* Steim, invalid compression flag(s) */

/* Define the high precision time tick interval as 1/modulus seconds.
 * Default, native modulus defines tick interval as a nanosecond.
 * Compatibility moduls defines tick interval as millisecond. */
#define NSTMODULUS 1000000000
#define HPTMODULUS 1000000

/* Error code for routines that normally return a high precision time.
 * The time value corresponds to '1902-1-1 00:00:00.00000000'. */
#define NSTERROR -2145916800000000000LL
#define HPTERROR -2145916800000000LL

/* Macros to scale between Unix/POSIX epoch time & high precision times */
#define MS_EPOCH2NSTIME(X) X * (hptime_t) NSTMODULUS
#define MS_NSTIME2EPOCH(X) X / NSTMODULUS
#define MS_EPOCH2HPTIME(X) X * (hptime_t) HPTMODULUS
#define MS_HPTIME2EPOCH(X) X / HPTMODULUS

/* Macro to test default sample rate tolerance: abs(1-sr1/sr2) < 0.0001 */
#define MS_ISRATETOLERABLE(A,B) (ms_dabs (1.0 - (A / B)) < 0.0001)

/* Macro to test for sane year and day values, used primarily to
 * determine if byte order swapping is needed for miniSEED 2.x.
 *
 * Year : between 1900 and 2100
 * Day  : between 1 and 366
 *
 * This test is non-unique (non-deterministic) for days 1, 256 and 257
 * in the year 2056 because the swapped values are also within range.
 * If you are using this in 2056 to determine the byte order of miniSEED 2
 * you have my deepest sympathies.
 */
#define MS_ISVALIDYEARDAY(Y,D) (Y >= 1900 && Y <= 2100 && D >= 1 && D <= 366)

/* Macro to test a character for miniSEED 2.x data record/quality indicators */
#define MS_ISQUALITYINDICATOR(X) (X=='D' || X=='R' || X=='Q' || X=='M')

/* Macro to test memory for a miniSEED 3.x data record signature by checking
 * record header values at known byte offsets.
 *
 * Offset = Value
 *   0  = "M"
 *   1  = "S"
 *   2  = 3
 *   8  = valid hour (0-23)
 *   9  = valid minute (0-59)
 *   10 = valid second (0-60)
 *
 * Usage:
 *   MS3_ISVALIDHEADER ((char *)X)  X buffer must contain at least 27 bytes
 */
#define MS3_ISVALIDHEADER(X) (                                 \
    *(X) == 'M' && *(X + 1) == 'S' && *(X + 2) == 3 &&         \
    (uint8_t) (*(X + 8)) >= 0 && (uint8_t) (*(X + 8)) <= 23 && \
    (uint8_t) (*(X + 9)) >= 0 && (uint8_t) (*(X + 9)) <= 59 && \
    (uint8_t) (*(X + 10)) >= 0 && (uint8_t) (*(X + 10)) <= 60) )

/* Macro to test memory for a miniSEED 2.x data record signature by checking
 * SEED data record header values at known byte offsets.
 *
 * Offset = Value
 * [0-5]  = Digits, spaces or NULL, SEED sequence number
 *     6  = Data record quality indicator
 *     7  = Space or NULL [not valid SEED]
 *     24 = Start hour (0-23)
 *     25 = Start minute (0-59)
 *     26 = Start second (0-60)
 *
 * Usage:
 *   MS2_ISVALIDHEADER ((char *)X)  X buffer must contain at least 27 bytes
 */
#define MS2_ISVALIDHEADER(X) (                                         \
    (isdigit ((uint8_t) * (X)) || *(X) == ' ' || !*(X)) &&             \
    (isdigit ((uint8_t) * (X + 1)) || *(X + 1) == ' ' || !*(X + 1)) && \
    (isdigit ((uint8_t) * (X + 2)) || *(X + 2) == ' ' || !*(X + 2)) && \
    (isdigit ((uint8_t) * (X + 3)) || *(X + 3) == ' ' || !*(X + 3)) && \
    (isdigit ((uint8_t) * (X + 4)) || *(X + 4) == ' ' || !*(X + 4)) && \
    (isdigit ((uint8_t) * (X + 5)) || *(X + 5) == ' ' || !*(X + 5)) && \
    MS2_ISQUALITYINDICATOR (*(X + 6)) &&                               \
    (*(X + 7) == ' ' || *(X + 7) == '\0') &&                           \
    (uint8_t) (*(X + 24)) >= 0 && (uint8_t) (*(X + 24)) <= 23 &&       \
    (uint8_t) (*(X + 25)) >= 0 && (uint8_t) (*(X + 25)) <= 59 &&       \
    (uint8_t) (*(X + 26)) >= 0 && (uint8_t) (*(X + 26)) <= 60 )

/* A simple bitwise AND test to return 0 or 1 */
#define bit(x,y) (x&y)?1:0

/* Define libmseed time type */
typedef int64_t nstime_t;
typedef int64_t hptime_t;

/* A single byte flag type */
typedef int8_t flag;

typedef struct MS3Record_s {
  char           *record;            /* miniSEED record */
  int32_t         reclen;            /* Length of miniSEED record in bytes */

  /* Common header fields in accessible form */
  char            tsid[64];          /* Time series identifier as URN */
  uint8_t         formatversion;     /* Original source format major version */
  uint8_t         flags;             /* Record-level bit flags */
  nstime_t        starttime;         /* Record start time (first sample) */
  double          samprate;          /* Nominal sample rate (Hz) */
  uint8_t         encoding;          /* Data encoding format */
  uint8_t         pubversion;        /* Publication version */
  uint32_t        samplecnt;         /* Number of samples in record */
  uint32_t        crc;               /* CRC of entire record */
  uint16_t        extralength;       /* Length of extra headers in bytes */
  uint16_t        payloadlength;     /* Length of data payload in bytes */
  void           *extra;             /* Pointer to extra headers */

  /* Data sample fields */
  void           *datasamples;       /* Data samples, 'numsamples' of type 'sampletype'*/
  int64_t         numsamples;        /* Number of data samples in datasamples */
  char            sampletype;        /* Sample type code: a, i, f, d */
}
MS3Record;

/* Container for a continuous trace segment, linkable */
typedef struct MS3TraceSeg_s {
  nstime_t        starttime;         /* Time of first sample */
  nstime_t        endtime;           /* Time of last sample */
  double          samprate;          /* Nominal sample rate (Hz) */
  int64_t         samplecnt;         /* Number of samples in trace coverage */
  void           *datasamples;       /* Data samples, 'numsamples' of type 'sampletype'*/
  int64_t         numsamples;        /* Number of data samples in datasamples */
  char            sampletype;        /* Sample type code: a, i, f, d */
  void           *prvtptr;           /* Private pointer for general use, unused by libmseed */
  struct MS3TraceSeg_s *prev;        /* Pointer to previous segment */
  struct MS3TraceSeg_s *next;        /* Pointer to next segment */
}
MS3TraceSeg;

/* Container for a trace ID, linkable */
typedef struct MS3TraceID_s {
  char            tsid[64];          /* Time series identifier as URN */
  uint8_t         pubversion;        /* Publication version */
  nstime_t        earliest;          /* Time of earliest sample */
  nstime_t        latest;            /* Time of latest sample */
  void           *prvtptr;           /* Private pointer for general use, unused by libmseed */
  uint32_t        numsegments;       /* Number of segments for this ID */
  struct MS3TraceSeg_s *first;       /* Pointer to first of list of segments */
  struct MS3TraceSeg_s *last;        /* Pointer to last of list of segments */
  struct MS3TraceID_s *next;         /* Pointer to next trace */
}
MS3TraceID;

/* Container for a continuous trace segment, linkable */
typedef struct MS3TraceList_s {
  uint32_t             numtraces;    /* Number of traces in list */
  struct MS3TraceID_s *traces;       /* Pointer to list of traces */
  struct MS3TraceID_s *last;         /* Pointer to last used trace in list */
}
MS3TraceList;

/* Data selection structure time window definition containers */
typedef struct MS3SelectTime_s {
  nstime_t starttime;    /* Earliest data for matching channels */
  nstime_t endtime;      /* Latest data for matching channels */
  struct MS3SelectTime_s *next;
} MS3SelectTime;

/* Data selection structure definition containers */
typedef struct MS3Selections_s {
  char tsidpattern[100];    /* Matching (globbing) pattern for time series ID */
  struct MS3SelectTime_s *timewindows;
  struct MS3Selections_s *next;
  uint8_t pubversion;
} MS3Selections;


/* Global variables (defined in unpack.c) and macros to set/force
 * encoding and fallback encoding */
extern int unpackencodingformat;
extern int unpackencodingfallback;
#define MS_UNPACKENCODINGFORMAT(X) (unpackencodingformat = X);
#define MS_UNPACKENCODINGFALLBACK(X) (unpackencodingfallback = X);

/* Global variables (defined in unpack.c) and macros to set/force
 * unpack byte orders for miniSEED 2.x */
extern int8_t ms2_unpackheaderbyteorder;
extern int8_t ms2_unpackdatabyteorder;
#define MS2_UNPACKHEADERBYTEORDER(X) (ms2_unpackheaderbyteorder = X);
#define MS2_UNPACKDATABYTEORDER(X) (ms2_unpackdatabyteorder = X);

/* miniSEED record related functions */
extern int msr3_parse (char *record, uint64_t recbuflen, MS3Record **ppmsr,
                       int8_t dataflag, int8_t verbose);

extern int msr3_parse_selection (char *recbuf, uint64_t recbuflen, uint64_t *offset,
                                 MS3Record **ppmsr, MS3Selections *selections,
                                 int8_t dataflag, int8_t verbose);

extern int msr3_unpack (char *record, MS3Record **ppmsr,
                        int8_t dataflag, int8_t verbose);

extern int msr3_pack (MS3Record *msr,
                      void (*record_handler) (char *, int, void *),
                      void *handlerdata, int64_t *packedsamples,
                      int8_t flush, int8_t verbose);

extern int msr3_pack_header (MS3Record *msr, int8_t normalize, int8_t verbose);

extern int msr3_unpack_data (MS3Record *msr, int swapflag, int8_t verbose);

extern MS3Record* msr3_init (MS3Record *msr);
extern void       msr3_free (MS3Record **ppmsr);
extern MS3Record* msr3_duplicate (MS3Record *msr, int8_t datadup);
extern hptime_t   msr3_endtime (MS3Record *msr);
extern void       msr3_print (MS3Record *msr, int8_t details);
extern double     msr3_host_latency (MS3Record *msr);

extern int ms3_detect (const char *record, int recbuflen);
extern int ms3_parse_raw (char *record, int maxreclen, int8_t details);
extern int ms2_parse_raw (char *record, int maxreclen, int8_t details, int8_t swapflag);

/* MSTraceList related functions */
extern MS3TraceList* mstl3_init (MS3TraceList *mstl);
extern void          mstl3_free (MS3TraceList **ppmstl, int8_t freeprvtptr);
extern MS3TraceSeg*  mstl3_addmsr (MS3TraceList *mstl, MS3Record *msr, int8_t pubversion,
                                   int8_t autoheal, double timetol, double sampratetol);
extern int mstl3_convertsamples (MS3TraceSeg *seg, char type, int8_t truncate);
extern void mstl3_printtracelist (MS3TraceList *mstl, int8_t timeformat,
                                  int8_t details, int8_t gaps);
extern void mstl3_printsynclist (MS3TraceList *mstl, char *dccid, int8_t subsecond);
extern void mstl3_printgaplist (MS3TraceList *mstl, int8_t timeformat,
                                double *mingap, double *maxgap);

/* Reading miniSEED records from files */
typedef struct MS3FileParam_s
{
  FILE *fp;
  char filename[512];
  char *rawrec;
  int readlen;
  int readoffset;
  int64_t filepos;
  int64_t filesize;
  int64_t recordcount;
} MS3FileParam;

extern int ms3_readmsr (MS3Record **ppmsr, const char *msfile, int64_t *fpos, int8_t *last,
                        int8_t skipnotdata, int8_t dataflag, int8_t verbose);
extern int ms3_readmsr_r (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
                          int64_t *fpos, int8_t *last, int8_t skipnotdata, int8_t dataflag, int8_t verbose);

extern int ms3_readmsr_main (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
                             int64_t *fpos, int8_t *last, int8_t skipnotdata, int8_t dataflag,
                             MS3Selections *selections, int8_t verbose);

extern int ms3_readtracelist (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                              int8_t pubversion, int8_t skipnotdata, int8_t dataflag, int8_t verbose);
extern int ms3_readtracelist_timewin (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                                      nstime_t starttime, nstime_t endtime, int8_t pubversion, int8_t skipnotdata,
                                      int8_t dataflag, int8_t verbose);
extern int ms3_readtracelist_selection (MS3TraceList **ppmstl, const char *msfile, double timetol, double sampratetol,
                                        MS3Selections *selections, int8_t pubversion, int8_t skipnotdata,
                                        int8_t dataflag, int8_t verbose);
extern int msr3_writemseed (MS3Record *msr, const char *msfile, int8_t overwrite,
                            int maxreclen, int8_t encoding, int8_t verbose);
extern int mstl3_writemseed (MS3TraceList *mst, const char *msfile, int8_t overwrite,
                             int maxreclen, int8_t encoding, int8_t verbose);

/* General use functions */
extern int      ms_parsetsid (char *tsid, char *net, char *sta, char *loc, char *chan);
extern int      ms_strncpclean (char *dest, const char *source, int length);
extern int      ms_strncpcleantail (char *dest, const char *source, int length);
extern int      ms_strncpopen (char *dest, const char *source, int length);
extern int      ms_doy2md (int year, int jday, int *month, int *mday);
extern int      ms_md2doy (int year, int month, int mday, int *jday);
extern char*    ms_nstime2isotimestr (nstime_t nstime, char *isotimestr, int8_t subsecond);
extern char*    ms_nstime2mdtimestr (nstime_t nstime, char *mdtimestr, int8_t subsecond);
extern char*    ms_nstime2seedtimestr (nstime_t nstime, char *seedtimestr, int8_t subsecond);
extern nstime_t ms_time2nstime (int year, int day, int hour, int min, int sec, int usec);
extern nstime_t ms_seedtimestr2nstime (char *seedtimestr);
extern nstime_t ms_timestr2nstime (char *timestr);
extern int      ms_bigendianhost (void);
extern double   ms_dabs (double val);

/* Lookup functions */
extern uint8_t  ms_samplesize (const char sampletype);
extern char*    ms_encodingstr (const char encoding);
extern char *   ms_errorstr (int errorcode);

/* Logging facility */
#define MAX_LOG_MSG_LENGTH  200      /* Maximum length of log messages */

/* Logging parameters */
typedef struct MSLogParam_s
{
  void (*log_print)(char*);
  const char *logprefix;
  void (*diag_print)(char*);
  const char *errprefix;
} MSLogParam;

extern int ms_log (int level, ...);
extern int ms_log_l (MSLogParam *logp, int level, ...);
extern void ms_loginit (void (*log_print)(char*), const char *logprefix,
                        void (*diag_print)(char*), const char *errprefix);
extern MSLogParam *ms_loginit_l (MSLogParam *logp,
                                 void (*log_print)(char*), const char *logprefix,
                                 void (*diag_print)(char*), const char *errprefix);

/* Selection functions */
extern MS3Selections *ms3_matchselect (MS3Selections *selections, char *tsid,
                                       nstime_t starttime, nstime_t endtime,
                                       int pubversion, MS3SelectTime **ppselecttime);
extern MS3Selections *msr3_matchselect (MS3Selections *selections, MS3Record *msr,
                                        MS3SelectTime **ppselecttime);
extern int ms3_addselect (MS3Selections **ppselections, char *tsidpattern,
                          nstime_t starttime, nstime_t endtime, uint8_t pubversion);
extern int ms3_addselect_comp (MS3Selections **ppselections,
                               char *net, char* sta, char *loc, char *chan,
                               nstime_t starttime, nstime_t endtime, uint8_t pubversion);
extern int ms3_readselectionsfile (MS3Selections **ppselections, char *filename);
extern void ms3_freeselections (MS3Selections *selections);
extern void ms3_printselections (MS3Selections *selections);

/* Leap second declarations, implementation in gentutils.c */
typedef struct LeapSecond_s
{
  nstime_t leapsecond;
  int32_t  TAIdelta;
  struct LeapSecond_s *next;
} LeapSecond;

extern LeapSecond *leapsecondlist;
extern int ms_readleapseconds (char *envvarname);
extern int ms_readleapsecondfile (char *filename);

/* Generic byte swapping routines */
extern void ms3_gswap2 ( void *data2 );
extern void ms3_gswap3 ( void *data3 );
extern void ms3_gswap4 ( void *data4 );
extern void ms3_gswap8 ( void *data8 );

/* Generic byte swapping routines for memory aligned quantities */
extern void ms3_gswap2a ( void *data2 );
extern void ms3_gswap4a ( void *data4 );
extern void ms3_gswap8a ( void *data8 );

/* Platform portable functions */
extern int64_t lmp_ftell64 (FILE *stream);
extern int lmp_fseek64 (FILE *stream, int64_t offset, int whence);

#ifdef __cplusplus
}
#endif

#endif /* LIBMSEED_H */
