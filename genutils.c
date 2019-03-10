/**********************************************************************/ /**
 * @file genutils.c
 *
 * Generic utility routines
 *
 * @author Chad Trabant, IRIS Data Management Center
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gmtime64.h"
#include "libmseed.h"

static nstime_t ms_time2nstime_int (int year, int day, int hour,
                                    int min, int sec, uint32_t nsec);

/* A constant number of seconds between the NTP and Posix/Unix time epoch */
#define NTPPOSIXEPOCHDELTA 2208988800LL

/* Global variable to hold a leap second list */
LeapSecond *leapsecondlist = NULL;

/* Days in each month, for non-leap and leap years */
static const int monthdays[]      = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static const int monthdays_leap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/** @def LEAPYEAR
    @brief Determine if year is a leap year **/
#define LEAPYEAR(year) (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))

/** @def VALIDYEAR
    @brief Check that a year is in a valid range **/
#define VALIDYEAR(year) (year >= 1800 && year <= 5000)

/** @def VALIDMONTH
    @brief Check that a month is in a valid range **/
#define VALIDMONTH(month) (month >= 1 && month <= 12)

/** @def VALIDMONTHDAY
    @brief Check that a day-of-month is in a valid range **/
#define VALIDMONTHDAY(year, month, mday) (mday >= 0 && mday <= (LEAPYEAR (year) ? monthdays_leap[month - 1] : monthdays[month - 1]))

/** @def VALIDYEARDAY
    @brief Check that a day-of-year is in a valid range **/
#define VALIDYEARDAY(year, yday) (yday >= 1 && yday <= (365 + (LEAPYEAR (year) ? 1 : 0)))

/** @def VALIDHOUR
    @brief Check that an hour is in a valid range **/
#define VALIDHOUR(hour) (hour >= 0 && hour <= 23)

/** @def VALIDMIN
    @brief Check that a minute is in a valid range **/
#define VALIDMIN(min) (min >= 0 && min <= 59)

/** @def VALIDSEC
    @brief Check that a second is in a valid range **/
#define VALIDSEC(sec) (sec >= 0 && sec <= 60)

/** @def VALIDNANOSEC
    @brief Check that a nanosecond is in a valid range **/
#define VALIDNANOSEC(nanosec) (nanosec >= 0 && nanosec <= 999999999)

/**********************************************************************/ /**
 * @brief Parse network, station, location and channel from a source ID URI
 *
 * Parse a source identifier into separate components, expecting:
 *  \c "XFDSN:NET_STA_LOC_CHAN", where \c CHAN="BAND_SOURCE_POSITION"
 * or
 *  \c "FDSN:NET_STA_LOC_CHAN" for a simple combination of SEED 2.4 idenitifers.
 *
 * Identifiers may contain additional namespace identifiers, e.g.:
 *  \c "XFDSN:AGENCY:NET_STA_LOC_CHAN"
 *
 * Memory for each component must already be allocated.  If a specific
 * component is not desired set the appropriate argument to NULL.
 *
 * @param[in] sid Source identifier
 * @param[out] net Network code
 * @param[out] sta Station code
 * @param[out] loc Location code
 * @param[out] chan Channel code
 *
 * @retval 0 on success
 * @retval -1 on error
 ***************************************************************************/
int
ms_sid2nslc (char *sid, char *net, char *sta, char *loc, char *chan)
{
  char *id;
  char *ptr, *top, *next;
  int sepcnt = 0;

  if (!sid)
    return -1;

  /* Handle the XFDSN: and FDSN: namespace identifiers */
  if (!strncmp (sid, "XFDSN:", 6) ||
      !strncmp (sid, "FDSN:", 5))
  {
    /* Advance sid pointer to last ':', skipping all namespace identifiers */
    sid = strrchr (sid, ':') + 1;

    /* Verify 3 or 5 delimiters */
    id = sid;
    while ((id = strchr (id, '_')))
    {
      id++;
      sepcnt++;
    }
    if (sepcnt != 3 && sepcnt != 5)
    {
      ms_log (2, "%s(): Incorrect number of identifier delimiters (%d): %s\n",
              __func__, sepcnt, sid);
      return -1;
    }

    if (!(id = strdup (sid)))
    {
      ms_log (2, "%s(): Error duplicating identifier\n", __func__);
      return -1;
    }

    /* Network */
    top = id;
    if ((ptr = strchr (top, '_')))
    {
      next = ptr + 1;
      *ptr = '\0';

      if (net)
        strcpy (net, top);

      top = next;
    }
    /* Station */
    if ((ptr = strchr (top, '_')))
    {
      next = ptr + 1;
      *ptr = '\0';

      if (sta)
        strcpy (sta, top);

      top = next;
    }
    /* Location (potentially empty) */
    if ((ptr = strchr (top, '_')))
    {
      next = ptr + 1;
      *ptr = '\0';

      if (loc)
        strcpy (loc, top);

      top = next;
    }
    /* Channel */
    if (*top && chan)
    {
      strcpy (chan, top);
    }

    /* Free duplicated ID */
    if (id)
      free (id);
  }
  else
  {
    ms_log (2, "%s(): Unrecognized identifier: %s\n", __func__, sid);
    return -1;
  }

  return 0;
} /* End of ms_sid2nslc() */

/**********************************************************************/ /**
 * @brief Convert network, station, location and channel to a source ID URI
 *
 * Create a source identifier from individual network,
 * station, location and channel codes with the form:
 *  \c XFDSN:NET_STA_LOC_CHAN, where \c CHAN="BAND_SOURCE_POSITION"
 *
 * Memory for the source identifier must already be allocated.  If a
 * specific component is NULL it will be empty in the resulting
 * identifier.
 *
 * @param[out] sid Destination string for source identifier
 * @param sidlen Maximum length of \a sid
 * @param flags Currently unused, set to 0
 * @param[in] net Network code
 * @param[in] sta Station code
 * @param[in] loc Location code
 * @param[in] chan Channel code
 *
 * @returns length of source identifier
 * @retval -1 on error
 ***************************************************************************/
int
ms_nslc2sid (char *sid, int sidlen, uint16_t flags,
             char *net, char *sta, char *loc, char *chan)
{
  char *sptr = sid;
  int needed = 0;

  if (!sid)
    return -1;

  if (sidlen < 13)
    return -1;

  *sptr++ = 'X';
  *sptr++ = 'F';
  *sptr++ = 'D';
  *sptr++ = 'S';
  *sptr++ = 'N';
  *sptr++ = ':';
  needed  = 6;

  if (net)
  {
    while (*net)
    {
      if ((sptr - sid) < sidlen)
        *sptr++ = *net;

      net++;
      needed++;
    }
  }

  if ((sptr - sid) < sidlen)
    *sptr++ = '_';
  needed++;

  if (sta)
  {
    while (*sta)
    {
      if ((sptr - sid) < sidlen)
        *sptr++ = *sta;

      sta++;
      needed++;
    }
  }

  if ((sptr - sid) < sidlen)
    *sptr++ = '_';
  needed++;

  if (loc)
  {
    while (*loc)
    {
      if ((sptr - sid) < sidlen)
        *sptr++ = *loc;

      loc++;
      needed++;
    }
  }

  if ((sptr - sid) < sidlen)
    *sptr++ = '_';
  needed++;

  if (chan)
  {
    while (*chan)
    {
      if ((sptr - sid) < sidlen)
        *sptr++ = *chan;

      chan++;
      needed++;
    }
  }

  if ((sptr - sid) < sidlen)
    *sptr = '\0';
  else
    *--sptr = '\0';

  if (needed >= sidlen)
    return -1;

  return (sptr - sid);
} /* End of ms_nslc2sid() */

/**********************************************************************/ /**
 * @brief Copy string, removing spaces, always terminated
 *
 * Copy up to \a length characters from \a source to \a dest while
 * removing all spaces.  The result is left justified and always null
 * terminated.
 *
 * The destination string must have enough room needed for the
 * non-space characters within \a length and the null terminator, a
 * maximum of \a length + 1.
 *
 * @param[out] dest Destination for terminated string
 * @param[in] source Source string
 * @param[in] length Length of characters for destination string in bytes
 *
 * @returns the number of characters (not including the null terminator) in
 * the destination string
 ***************************************************************************/
int
ms_strncpclean (char *dest, const char *source, int length)
{
  int sidx, didx;

  if (!dest)
    return 0;

  if (!source)
  {
    *dest = '\0';
    return 0;
  }

  for (sidx = 0, didx = 0; sidx < length; sidx++)
  {
    if (*(source + sidx) == '\0')
    {
      break;
    }

    if (*(source + sidx) != ' ')
    {
      *(dest + didx) = *(source + sidx);
      didx++;
    }
  }

  *(dest + didx) = '\0';

  return didx;
} /* End of ms_strncpclean() */

/**********************************************************************/ /**
 * @brief Copy string, removing trailing spaces, always terminated
 *
 * Copy up to \a length characters from \a source to \a dest without any
 * trailing spaces.  The result is left justified and always null
 * terminated.
 *
 * The destination string must have enough room needed for the
 * characters within \a length and the null terminator, a maximum of
 * \a length + 1.
 *
 * @param[out] dest Destination for terminated string
 * @param[in] source Source string
 * @param[in] length Length of characters for destination string in bytes
 *
 * @returns The number of characters (not including the null terminator) in
 * the destination string
 ***************************************************************************/
int
ms_strncpcleantail (char *dest, const char *source, int length)
{
  int idx, pretail;

  if (!dest)
    return 0;

  if (!source)
  {
    *dest = '\0';
    return 0;
  }

  *(dest + length) = '\0';

  pretail = 0;
  for (idx = length - 1; idx >= 0; idx--)
  {
    if (!pretail && *(source + idx) == ' ')
    {
      *(dest + idx) = '\0';
    }
    else
    {
      pretail++;
      *(dest + idx) = *(source + idx);
    }
  }

  return pretail;
} /* End of ms_strncpcleantail() */

/**********************************************************************/ /**
 * @brief Copy fixed number of characters into unterminated string
 *
 * Copy \a length characters from \a source to \a dest, padding the right
 * side with spaces and leave open-ended, aka un-terminated.  The
 * result is left justified and \e never null terminated.
 *
 * The destination string must have enough room for \a length characters.
 *
 * @param[out] dest Destination for unterminated string
 * @param[in] source Source string
 * @param[in] length Length of characters for destination string in bytes
 *
 * @returns the number of characters copied from the source string
 ***************************************************************************/
int
ms_strncpopen (char *dest, const char *source, int length)
{
  int didx;
  int dcnt = 0;
  int term = 0;

  if (!dest)
    return 0;

  if (!source)
  {
    for (didx = 0; didx < length; didx++)
    {
      *(dest + didx) = ' ';
    }

    return 0;
  }

  for (didx = 0; didx < length; didx++)
  {
    if (!term)
      if (*(source + didx) == '\0')
        term = 1;

    if (!term)
    {
      *(dest + didx) = *(source + didx);
      dcnt++;
    }
    else
    {
      *(dest + didx) = ' ';
    }
  }

  return dcnt;
} /* End of ms_strncpopen() */

/**********************************************************************/ /**
 * @brief Compute the month and day-of-month from a year and day-of-year
 *
 * @param[in] year Year in range 1800-5000
 * @param[in] yday Day of year in range 1-365 or 366 for leap year
 * @param[out] month Month in range 1-12
 * @param[out] mday Day of month in range 1-31
 *
 * @retval 0 on success
 * @retval -1 on error
 ***************************************************************************/
int
ms_doy2md (int year, int yday, int *month, int *mday)
{
  int idx;
  const int(*days)[];

  if (!VALIDYEAR (year))
  {
    ms_log (2, "%s(): year (%d) is out of range\n", __func__, year);
    return -1;
  }

  if (!VALIDYEARDAY (year, yday))
  {
    ms_log (2, "%s(): day-of-year (%d) is out of range for year %d\n", __func__, yday, year);
    return -1;
  }

  days = (LEAPYEAR (year)) ? &monthdays_leap : &monthdays;

  for (idx = 0; idx < 12; idx++)
  {
    yday -= (*days)[idx];

    if (yday <= 0)
    {
      *month = idx + 1;
      *mday  = (*days)[idx] + yday;
      break;
    }
  }

  return 0;
} /* End of ms_doy2md() */

/**********************************************************************/ /**
 * @brief Compute the day-of-year from a year, month and day-of-month
 *
 * @param[in] year Year in range 1800-5000
 * @param[in] month Month in range 1-12
 * @param[in] mday Day of month in range 1-31 (or appropriate last day)
 * @param[out] yday Day of year in range 1-366 or 366 for leap year
 *
 * @retval 0 on success
 * @retval -1 on error
 ***************************************************************************/
int
ms_md2doy (int year, int month, int mday, int *yday)
{
  int idx;
  const int(*days)[];

  if (!VALIDYEAR (year))
  {
    ms_log (2, "%s(): year (%d) is out of range\n", __func__, year);
    return -1;
  }
  if (!VALIDMONTH (month))
  {
    ms_log (2, "%s(): month (%d) is out of range\n", __func__, month);
    return -1;
  }
  if (!VALIDMONTHDAY (year, month, mday))
  {
    ms_log (2, "%s(): day-of-month (%d) is out of range for year %d and month %d\n", __func__, mday, year, month);
    return -1;
  }

  days = (LEAPYEAR (year)) ? &monthdays_leap : &monthdays;

  *yday = 0;
  month--;

  for (idx = 0; idx < 12; idx++)
  {
    if (idx == month)
    {
      *yday += mday;
      break;
    }

    *yday += (*days)[idx];
  }

  return 0;
} /* End of ms_md2doy() */

/**********************************************************************/ /**
 * @brief Convert an ::nstime_t to individual date-time components
 *
 * @param[in] nstime Time value to convert
 * @param[out] year Year with century, like 2018
 * @param[out] day Day of year, 1 - 366
 * @param[out] hour Hour, 0 - 23
 * @param[out] min Minute, 0 - 59
 * @param[out] sec Second, 0 - 60, where 60 is a leap second
 * @param[out] nsec Nanoseconds, 0 - 999999999
 *
 * @retval 0 on success
 * @retval -1 on error
 ***************************************************************************/
int
ms_nstime2time (nstime_t nstime, uint16_t *year, uint16_t *day,
                uint8_t *hour, uint8_t *min, uint8_t *sec, uint32_t *nsec)
{
  struct tm tms;
  int64_t isec;
  int ifract;

  /* Reduce to Unix/POSIX epoch time and fractional seconds */
  isec   = MS_NSTIME2EPOCH (nstime);
  ifract = (int)(nstime - (isec * NSTMODULUS));

  /* Adjust for negative epoch times */
  if (nstime < 0 && ifract != 0)
  {
    isec -= 1;
    ifract = NSTMODULUS - (-ifract);
  }

  if (!(ms_gmtime64_r (&isec, &tms)))
    return -1;

  if (year)
    *year = tms.tm_year + 1900;

  if (day)
    *day = tms.tm_yday + 1;

  if (hour)
    *hour = tms.tm_hour;

  if (min)
    *min = tms.tm_min;

  if (sec)
    *sec = tms.tm_sec;

  if (nsec)
    *nsec = ifract;

  return 0;
} /* End of ms_nstime2time() */

/**********************************************************************/ /**
 * @brief Convert an ::nstime_t to a time string
 *
 * Create a time string representation of a high precision epoch time
 * in ISO 8601 and SEED formats.
 *
 * The provided \a timestr buffer must have enough room for the
 * resulting time string, a maximum of 30 characters.
 *
 * The \a subseconds flag controls whether the subsecond portion of
 * the time is included or not.  If subseconds are included,
 * nanosecond resolution is only included when submicroseconds are
 * non-zero.
 *
 * @param[in] nstime Time value to convert
 * @param[out] timestr Buffer for ISO time string
 * @param format Flag controlling the time string format:
 * @parblock
 * - \c 0 - Month-day format, T separator: \c 'YYYY-MM-DDThh:mm:ss[.ssssss[sss]]'
 * - \c 1 - Month-day format, space separator: \c 'YYYY-MM-DD hh:mm:ss[.ssssss[sss]]'
 * - \c 2 - SEED day-of-year format: \c 'YYYY,DDD,hh:mm:ss[.ssssss[sss]]'
 * @endparblock
 * @param subseconds Flag controlling inclusion of subseconds:
 * @parblock
 * - \c 2 - include subseconds to nanoseconds
 * - \c 1 - include subseconds to microseconds at least, nanoseconds if existing
 * - \c 0 - do not include subseconds
 * - \c -1 - include subseconds if non-zero, same logic as for a value of \c 1
 * @endparblock
 *
 * @returns Pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms_nstime2timestr (nstime_t nstime, char *timestr, int8_t format, int8_t subseconds)
{
  struct tm tms;
  int64_t isec;
  int nanosec;
  int microsec;
  int submicro;
  int printed  = 0;
  int expected = 0;

  if (timestr == NULL)
    return NULL;

  /* Reduce to Unix/POSIX epoch time and fractional nanoseconds */
  isec    = MS_NSTIME2EPOCH (nstime);
  nanosec = (int)(nstime - (isec * NSTMODULUS));

  /* Adjust for negative epoch times */
  if (nstime < 0 && nanosec != 0)
  {
    isec -= 1;
    nanosec = NSTMODULUS - (-nanosec);
  }

  /* Determine microsecond and sub-microsecond values */
  microsec = nanosec / 1000;
  submicro = nanosec - (microsec * 1000);

  if (!(ms_gmtime64_r (&isec, &tms)))
    return NULL;

  if (subseconds == 1 || subseconds == 2 || (subseconds == -1 && nanosec))
  {
    /* Print nanoseconds if requested or sub-microseconds are non-zero */
    if (submicro || subseconds == 2)
    {
      switch (format)
      {
      case 0:
      case 1:
        expected = 29;
        printed  = snprintf (timestr, 30, "%4d-%02d-%02d%c%02d:%02d:%02d.%09d",
                            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                            (format == 0) ? 'T' : ' ',
                            tms.tm_hour, tms.tm_min, tms.tm_sec, nanosec);
        break;
      case 2:
        expected = 27;
        printed  = snprintf (timestr, 28, "%4d,%03d,%02d:%02d:%02d.%09d",
                            tms.tm_year + 1900, tms.tm_yday + 1,
                            tms.tm_hour, tms.tm_min, tms.tm_sec, nanosec);
        break;
      }
    }
    /* Otherwise, print microseconds */
    else
    {
      switch (format)
      {
      case 0:
      case 1:
        expected = 26;
        printed  = snprintf (timestr, 27, "%4d-%02d-%02d%c%02d:%02d:%02d.%06d",
                            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                            (format == 0) ? 'T' : ' ',
                            tms.tm_hour, tms.tm_min, tms.tm_sec, microsec);
        break;
      case 2:
        expected = 24;
        printed  = snprintf (timestr, 25, "%4d,%03d,%02d:%02d:%02d.%06d",
                            tms.tm_year + 1900, tms.tm_yday + 1,
                            tms.tm_hour, tms.tm_min, tms.tm_sec, microsec);
        break;
      }
    }
  }
  /* Otherwise, print no subseconds */
  else
  {
    switch (format)
    {
    case 0:
    case 1:
      expected = 19;
      printed  = snprintf (timestr, 20, "%4d-%02d-%02d%c%02d:%02d:%02d",
                          tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                          (format == 0) ? 'T' : ' ',
                          tms.tm_hour, tms.tm_min, tms.tm_sec);
      break;
    case 2:
      expected = 17;
      printed  = snprintf (timestr, 18, "%4d,%03d,%02d:%02d:%02d",
                          tms.tm_year + 1900, tms.tm_yday + 1,
                          tms.tm_hour, tms.tm_min, tms.tm_sec);
      break;
    }
  }

  if (expected == 0 || printed != expected)
    return NULL;
  else
    return timestr;
} /* End of ms_nstime2timestr() */

/***************************************************************************
 * INTERNAL Convert specified date-time values to a high precision epoch time.
 *
 * This is an internal version which does no range checking, it is
 * assumed that checking the range for each value has already been
 * done.
 *
 * Returns epoch time on success and ::NSTERROR on error.
 ***************************************************************************/
static nstime_t
ms_time2nstime_int (int year, int yday, int hour, int min, int sec, uint32_t nsec)
{
  nstime_t nstime;
  int shortyear;
  int a4, a100, a400;
  int intervening_leap_days;
  int days;

  shortyear = year - 1900;

  a4                    = (shortyear >> 2) + 475 - !(shortyear & 3);
  a100                  = a4 / 25 - (a4 % 25 < 0);
  a400                  = a100 >> 2;
  intervening_leap_days = (a4 - 492) - (a100 - 19) + (a400 - 4);

  days = (365 * (shortyear - 70) + intervening_leap_days + (yday - 1));

  nstime = (nstime_t) (60 * (60 * ((nstime_t)24 * days + hour) + min) + sec) * NSTMODULUS + nsec;

  return nstime;
} /* End of ms_time2nstime_int() */

/**********************************************************************/ /**
 * @brief Convert specified date-time values to a high precision epoch time.
 *
 * Each value must be within these expected ranges:
 * - year : 1800 - 5000
 * - yday : 1 - 366
 * - hour : 0 - 23
 * - min  : 0 - 59
 * - sec  : 0 - 60
 * - nsec : 0 - 999999999
 *
 * @returns epoch time on success and ::NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_time2nstime (int year, int yday, int hour, int min, int sec, uint32_t nsec)
{
  if (!VALIDYEAR (year))
  {
    ms_log (2, "%s(): year (%d) is out of range\n", __func__, year);
    return NSTERROR;
  }

  if (!VALIDYEARDAY (year, yday))
  {
    ms_log (2, "%s(): day-of-year (%d) is out of range for year %d\n", __func__, yday, year);
    return NSTERROR;
  }

  if (!VALIDHOUR (hour))
  {
    ms_log (2, "%s(): hour (%d) is out of range\n", __func__, hour);
    return NSTERROR;
  }

  if (!VALIDMIN (min))
  {
    ms_log (2, "%s(): minute (%d) is out of range\n", __func__, min);
    return NSTERROR;
  }

  if (!VALIDSEC (sec))
  {
    ms_log (2, "%s(): second (%d) is out of range\n", __func__, sec);
    return NSTERROR;
  }

  if (!VALIDNANOSEC (nsec))
  {
    ms_log (2, "%s(): nanosecond (%d) is out of range\n", __func__, nsec);
    return NSTERROR;
  }

  return ms_time2nstime_int (year, yday, hour, min, sec, nsec);
} /* End of ms_time2nstime() */

/**********************************************************************/ /**
 * @brief Convert a time string to a high precision epoch time.
 *
 * The time format expected is "YYYY[/MM/DD HH:MM:SS.FFFFFFFFF]", the
 * delimiter can be a dash [-], comma[,], slash [/], colon [:], or
 * period [.].  Additionally a 'T' or space may be used between the
 * date and time fields.  The fractional seconds ("FFFFFFFFF") must
 * begin with a period [.] if present.
 *
 * The time string can be "short" in which case the omitted values are
 * assumed to be zero (with the exception of month and day which are
 * assumed to be 1): "YYYY/MM/DD" assumes HH, MM, SS and FFFF are 0.
 * The year is required, otherwise there wouldn't be much for a date.
 *
 * @returns epoch time on success and ::NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_timestr2nstime (char *timestr)
{
  int fields;
  int year    = 0;
  int mon     = 1;
  int mday    = 1;
  int yday    = 1;
  int hour    = 0;
  int min     = 0;
  int sec     = 0;
  double fsec = 0.0;
  int nsec    = 0;

  fields = sscanf (timestr, "%d%*[-,/:.]%d%*[-,/:.]%d%*[-,/:.Tt ]%d%*[-,/:.]%d%*[-,/:.]%d%lf",
                   &year, &mon, &mday, &hour, &min, &sec, &fsec);

  /* Convert fractional seconds to nanoseconds */
  if (fsec != 0.0)
  {
    nsec = (int)(fsec * 1000000000.0 + 0.5);
  }

  if (fields < 1)
  {
    ms_log (2, "%s(): Cannot parse time string: %s\n", __func__, timestr);
    return NSTERROR;
  }

  if (!VALIDYEAR (year))
  {
    ms_log (2, "%s(): year (%d) is out of range\n", __func__, year);
    return NSTERROR;
  }

  if (!VALIDMONTH (mon))
  {
    ms_log (2, "%s(): month (%d) is out of range\n", __func__, mon);
    return NSTERROR;
  }

  if (!VALIDMONTHDAY (year, mon, mday))
  {
    ms_log (2, "%s(): day-of-month (%d) is out of range for year %d and month %d\n", __func__, mday, year, mon);
    return NSTERROR;
  }

  if (!VALIDHOUR (hour))
  {
    ms_log (2, "%s(): hour (%d) is out of range\n", __func__, hour);
    return NSTERROR;
  }

  if (!VALIDMIN (min))
  {
    ms_log (2, "%s(): minute (%d) is out of range\n", __func__, min);
    return NSTERROR;
  }

  if (!VALIDSEC (sec))
  {
    ms_log (2, "%s(): second (%d) is out of range\n", __func__, sec);
    return NSTERROR;
  }

  if (!VALIDNANOSEC (nsec))
  {
    ms_log (2, "%s(): fractional second (%d) is out of range\n", __func__, nsec);
    return NSTERROR;
  }

  /* Convert month and day-of-month to day-of-year */
  if (ms_md2doy (year, mon, mday, &yday))
  {
    return NSTERROR;
  }

  return ms_time2nstime_int (year, yday, hour, min, sec, nsec);
} /* End of ms_timestr2nstime() */

/**********************************************************************/ /**
 * @brief Convert a SEED time string (day-of-year style) to a high precision
 * epoch time.
 *
 * The time format expected is "YYYY[,DDD,HH,MM,SS.FFFFFFFFF]", the
 * delimiter can be a dash [-], comma [,], colon [:] or period [.].
 * Additionally a [T] or space may be used to seprate the day and hour
 * fields.  The fractional seconds ("FFFFFFFFF") must begin with a
 * period [.] if present.
 *
 * The time string can be "short" in which case the omitted values are
 * assumed to be zero (with the exception of DDD which is assumed to
 * be 1): "YYYY,DDD,HH" assumes MM, SS and FFFFFFFF are 0.  The year
 * is required, otherwise there wouldn't be much for a date.
 *
 * @returns epoch time on success and ::NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_seedtimestr2nstime (char *seedtimestr)
{
  int fields;
  int year    = 0;
  int yday    = 1;
  int hour    = 0;
  int min     = 0;
  int sec     = 0;
  double fsec = 0.0;
  int nsec    = 0;

  fields = sscanf (seedtimestr, "%d%*[-,:.]%d%*[-,:.Tt ]%d%*[-,:.]%d%*[-,:.]%d%lf",
                   &year, &yday, &hour, &min, &sec, &fsec);

  /* Convert fractional seconds to nanoseconds */
  if (fsec != 0.0)
  {
    nsec = (int)(fsec * 1000000000.0 + 0.5);
  }

  if (fields < 1)
  {
    ms_log (2, "%s(): Cannot parse time string: %s\n", __func__, seedtimestr);
    return NSTERROR;
  }

  if (!VALIDYEAR (year))
  {
    ms_log (2, "%s(): year (%d) is out of range\n", __func__, year);
    return NSTERROR;
  }

  if (!VALIDYEARDAY (year, yday))
  {
    ms_log (2, "%s(): day-of-year (%d) is out of range for year %d\n", __func__, yday, year);
    return NSTERROR;
  }

  if (!VALIDHOUR (hour))
  {
    ms_log (2, "%s(): hour (%d) is out of range\n", __func__, hour);
    return NSTERROR;
  }

  if (!VALIDMIN (min))
  {
    ms_log (2, "%s(): minute (%d) is out of range\n", __func__, min);
    return NSTERROR;
  }

  if (!VALIDSEC (sec))
  {
    ms_log (2, "%s(): second (%d) is out of range\n", __func__, sec);
    return NSTERROR;
  }

  if (!VALIDNANOSEC (nsec))
  {
    ms_log (2, "%s(): fractional second (%d) is out of range\n", __func__, nsec);
    return NSTERROR;
  }

  return ms_time2nstime_int (year, yday, hour, min, sec, nsec);
} /* End of ms_seedtimestr2nstime() */

/**********************************************************************/ /**
 * @brief Determine the absolute value of an input double
 *
 * Actually just test if the input double is positive multiplying by
 * -1.0 if not and return it.
 *
 * @returns the positive value of input double.
 ***************************************************************************/
double
ms_dabs (double val)
{
  if (val < 0.0)
    val *= -1.0;
  return val;
} /* End of ms_dabs() */

/**********************************************************************/ /**
 * @brief Read leap second file specified by an environment variable
 *
 * Leap seconds are loaded into the library's global leapsecond list.
 *
 * @returns positive number of leap seconds read
 * @retval -1 on file read error
 * @retval -2 when the environment variable is not set
 ***************************************************************************/
int
ms_readleapseconds (const char *envvarname)
{
  char *filename;

  if ((filename = getenv (envvarname)))
  {
    return ms_readleapsecondfile (filename);
  }

  return -2;
} /* End of ms_readleapseconds() */

/**********************************************************************/ /**
 * @brief Read leap second from the specified file
 *
 * Leap seconds are loaded into the library's global leapsecond list.
 *
 * The file is expected to be standard IETF leap second list format.
 * The list is usually available from:
 * https://www.ietf.org/timezones/data/leap-seconds.list
 *
 * @returns positive number of leap seconds read on success
 * @retval -1 on error
 ***************************************************************************/
int
ms_readleapsecondfile (const char *filename)
{
  FILE *fp           = NULL;
  LeapSecond *ls     = NULL;
  LeapSecond *lastls = NULL;
  int64_t expires;
  char readline[200];
  char *cp;
  int64_t leapsecond;
  int TAIdelta;
  int fields;
  int count = 0;

  if (!filename)
    return -1;

  if (!(fp = fopen (filename, "rb")))
  {
    ms_log (2, "Cannot open leap second file %s: %s\n", filename, strerror (errno));
    return -1;
  }

  /* Free existing leapsecondlist */
  while (leapsecondlist != NULL)
  {
    LeapSecond *next = leapsecondlist->next;
    free (leapsecondlist);
    leapsecondlist = next;
  }

  while (fgets (readline, sizeof (readline) - 1, fp))
  {
    /* Guarantee termination */
    readline[sizeof (readline) - 1] = '\0';

    /* Terminate string at first newline character if any */
    if ((cp = strchr (readline, '\n')))
      *cp = '\0';

    /* Skip empty lines */
    if (!strlen (readline))
      continue;

    /* Check for and parse expiration date */
    if (!strncmp (readline, "#@", 2))
    {
      expires = 0;
      fields  = sscanf (readline, "#@ %" SCNd64, &expires);

      if (fields == 1)
      {
        /* Convert expires to Unix epoch */
        expires = expires - NTPPOSIXEPOCHDELTA;

        /* Compare expire time to current time */
        if (time (NULL) > expires)
        {
          char timestr[100];
          ms_nstime2timestr (MS_EPOCH2NSTIME (expires), timestr, 0, 0);
          ms_log (1, "Warning: leap second file (%s) has expired as of %s\n",
                  filename, timestr);
        }
      }

      continue;
    }

    /* Skip comment lines */
    if (*readline == '#')
      continue;

    fields = sscanf (readline, "%" SCNd64 " %d ", &leapsecond, &TAIdelta);

    if (fields == 2)
    {
      if ((ls = (LeapSecond *)malloc (sizeof (LeapSecond))) == NULL)
      {
        ms_log (2, "Cannot allocate LeapSecond, out of memory?\n");
        return -1;
      }

      /* Convert NTP epoch time to Unix epoch time and then to nttime_t */
      ls->leapsecond = MS_EPOCH2NSTIME ((leapsecond - NTPPOSIXEPOCHDELTA));
      ls->TAIdelta   = TAIdelta;
      ls->next       = NULL;
      count++;

      /* Add leap second to global list */
      if (!leapsecondlist)
      {
        leapsecondlist = ls;
        lastls         = ls;
      }
      else
      {
        lastls->next = ls;
        lastls       = ls;
      }
    }
    else
    {
      ms_log (1, "Unrecognized leap second file line: '%s'\n", readline);
    }
  }

  if (ferror (fp))
  {
    ms_log (2, "Error reading leap second file (%s): %s\n", filename, strerror (errno));
  }

  fclose (fp);

  return count;
} /* End of ms_readleapsecondfile() */
