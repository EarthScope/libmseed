/***************************************************************************
 * genutils.c
 *
 * Generic utility routines
 *
 * Written by Chad Trabant
 * IRIS Data Management Center
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"
#include "gmtime64.h"

static nstime_t ms_time2nstime_int (int year, int day, int hour,
                                    int min, int sec, uint32_t nsec);

/* A constant number of seconds between the NTP and Posix/Unix time epoch */
#define NTPPOSIXEPOCHDELTA 2208988800LL

/* Global variable to hold a leap second list */
LeapSecond *leapsecondlist = NULL;

/***************************************************************************
 * ms_sid2nslc:
 *
 * Parse a source identifier into separate components, expecting:
 *  "XFDSN:NET_STA_LOC_CHAN", where CHAN="BAND_SOURCE_POSITION"
 * or
 *  "FDSN:NET_STA_LOC_CHAN" for a simple combination of SEED 2.4 idenitifers.
 *
 * Identifiers may contain additional namespace identifiers, e.g.:
 *  "XFDSN:AGENCY:NET_STA_LOC_CHAN"
 *
 * Memory for each component must already be allocated.  If a specific
 * component is not desired set the appropriate argument to NULL.
 *
 * Returns 0 on success and -1 on error.
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
      ms_log (2, "ms_sid2nslc(): Incorrect number of identifier delimiters (%d): %s\n",
              sepcnt, sid);
      return -1;
    }

    if (!(id = strdup (sid)))
    {
      ms_log (2, "ms_sid2nslc(): Error duplicating identifier\n");
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
    ms_log (2, "ms_sid2nslc(): Unrecognized identifier: %s\n", sid);
    return -1;
  }

  return 0;
} /* End of ms_sid2nslc() */

/***************************************************************************
 * ms_nslc2sid:
 *
 * Create a source identifier from individual network,
 * station, location and channel codes with the form:
 *  "XFDSN:NET_STA_LOC_CHAN", where CHAN="BAND_SOURCE_POSITION"
 *
 * Memory for the source identifier must already be allocated.  If a
 * specific component is NULL it will be empty in the resulting
 * identifier.
 *
 * The flags argument is not yet used and should be set to 0.
 *
 * Returns length of identifier on success and -1 on error.
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
  needed = 6;

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


/***************************************************************************
 * ms_strncpclean:
 *
 * Copy up to 'length' characters from 'source' to 'dest' while
 * removing all spaces.  The result is left justified and always null
 * terminated.  The destination string must have enough room needed
 * for the non-space characters within 'length' and the null
 * terminator, a maximum of 'length + 1'.
 *
 * Returns the number of characters (not including the null terminator) in
 * the destination string.
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

/***************************************************************************
 * ms_strncpcleantail:
 *
 * Copy up to 'length' characters from 'source' to 'dest' without any
 * trailing spaces.  The result is left justified and always null
 * terminated.  The destination string must have enough room needed
 * for the characters within 'length' and the null terminator, a
 * maximum of 'length + 1'.
 *
 * Returns the number of characters (not including the null terminator) in
 * the destination string.
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

/***************************************************************************
 * ms_strncpopen:
 *
 * Copy 'length' characters from 'source' to 'dest', padding the right
 * side with spaces and leave open-ended.  The result is left
 * justified and *never* null terminated (the open-ended part).  The
 * destination string must have enough room for 'length' characters.
 *
 * Returns the number of characters copied from the source string.
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

/***************************************************************************
 * ms_doy2md:
 *
 * Compute the month and day-of-month from a year and day-of-year.
 *
 * Year is expected to be in the range 1800-5000, jday is expected to
 * be in the range 1-366, month will be in the range 1-12 and mday
 * will be in the range 1-31.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
int
ms_doy2md (int year, int jday, int *month, int *mday)
{
  int idx;
  int leap;
  int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  /* Sanity check for the supplied year */
  if (year < 1800 || year > 5000)
  {
    ms_log (2, "ms_doy2md(): year (%d) is out of range\n", year);
    return -1;
  }

  /* Test for leap year */
  leap = (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) ? 1 : 0;

  /* Add a day to February if leap year */
  if (leap)
    days[1]++;

  if (jday > 365 + leap || jday <= 0)
  {
    ms_log (2, "ms_doy2md(): day-of-year (%d) is out of range\n", jday);
    return -1;
  }

  for (idx = 0; idx < 12; idx++)
  {
    jday -= days[idx];

    if (jday <= 0)
    {
      *month = idx + 1;
      *mday = days[idx] + jday;
      break;
    }
  }

  return 0;
} /* End of ms_doy2md() */

/***************************************************************************
 * ms_md2doy:
 *
 * Compute the day-of-year from a year, month and day-of-month.
 *
 * Year is expected to be in the range 1800-5000, month is expected to
 * be in the range 1-12, mday is expected to be in the range 1-31 and
 * jday will be in the range 1-366.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
int
ms_md2doy (int year, int month, int mday, int *jday)
{
  int idx;
  int leap;
  int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  /* Sanity check for the supplied parameters */
  if (year < 1800 || year > 5000)
  {
    ms_log (2, "ms_md2doy(): year (%d) is out of range\n", year);
    return -1;
  }
  if (month < 1 || month > 12)
  {
    ms_log (2, "ms_md2doy(): month (%d) is out of range\n", month);
    return -1;
  }
  if (mday < 1 || mday > 31)
  {
    ms_log (2, "ms_md2doy(): day-of-month (%d) is out of range\n", mday);
    return -1;
  }

  /* Test for leap year */
  leap = (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)) ? 1 : 0;

  /* Add a day to February if leap year */
  if (leap)
    days[1]++;

  /* Check that the day-of-month jives with specified month */
  if (mday > days[month - 1])
  {
    ms_log (2, "ms_md2doy(): day-of-month (%d) is out of range for month %d\n",
            mday, month);
    return -1;
  }

  *jday = 0;
  month--;

  for (idx = 0; idx < 12; idx++)
  {
    if (idx == month)
    {
      *jday += mday;
      break;
    }

    *jday += days[idx];
  }

  return 0;
} /* End of ms_md2doy() */

/***************************************************************************
 * ms_nstime2time:
 *
 * Convert an nstime_t to individual time components.
 *
 * Returns 0 on success and -1 on error.
 ***************************************************************************/
int
ms_nstime2time (nstime_t nstime, uint16_t *year, uint16_t *day,
                uint8_t *hour, uint8_t *min, uint8_t *sec, uint32_t *nsec)
{
  struct tm tms;
  int64_t isec;
  int ifract;

  /* Reduce to Unix/POSIX epoch time and fractional seconds */
  isec = MS_NSTIME2EPOCH (nstime);
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

/***************************************************************************
 * ms_nstime2isotimestr:
 *
 * Build a time string in ISO recommended format from a high precision
 * epoch time.
 *
 * The provided isostimestr must have enough room for the resulting time
 * string of 30 characters, i.e. '2001-07-29T12:38:00.000000000' + NULL.
 *
 * The 'subseconds' flag controls whenther the sub second portion of the
 * time is included or not:
 *  1 = include subseconds
 *  0 = do not include subseconds
 * -1 = include subseconds if non-zero
 *
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms_nstime2isotimestr (nstime_t nstime, char *isotimestr, int8_t subseconds)
{
  struct tm tms;
  int64_t isec;
  int nanosec;
  int microsec;
  int submicro;
  int ret;

  if (isotimestr == NULL)
    return NULL;

  /* Reduce to Unix/POSIX epoch time and fractional nanoseconds */
  isec = MS_NSTIME2EPOCH (nstime);
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

  if (subseconds == 1 || (subseconds == -1 && nanosec))
  {
    /* Print nanoseconds if sub-microseconds are non-zero */
    if (submicro)
      ret = snprintf (isotimestr, 30, "%4d-%02d-%02dT%02d:%02d:%02d.%09d",
                      tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, nanosec);
    /* Otherwise, print microseconds */
    else
      ret = snprintf (isotimestr, 30, "%4d-%02d-%02dT%02d:%02d:%02d.%06d",
                      tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, microsec);
  }
  else
  {
    ret = snprintf (isotimestr, 20, "%4d-%02d-%02dT%02d:%02d:%02d",
                    tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                    tms.tm_hour, tms.tm_min, tms.tm_sec);
  }

  if (ret != 29 && ret != 26 && ret != 19)
    return NULL;
  else
    return isotimestr;
} /* End of ms_nstime2isotimestr() */

/***************************************************************************
 * ms_nstime2mdtimestr:
 *
 * Build a time string in month-day format from a high precision
 * epoch time.
 *
 * The provided mdtimestr must have enough room for the resulting time
 * string of 30 characters, i.e. '2001-07-29 12:38:00.000000000' + NULL.
 *
 * The 'subseconds' flag controls whenther the sub second portion of the
 * time is included or not:
 *  1 = include microseconds always and nanoseconds if non-zero
 *  0 = do not include subseconds
 * -1 = include subseconds if non-zero and nanoseconds if non-zero
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms_nstime2mdtimestr (nstime_t nstime, char *mdtimestr, flag subseconds)
{
  struct tm tms;
  int64_t isec;
  int nanosec;
  int microsec;
  int submicro;
  int ret;

  if (mdtimestr == NULL)
    return NULL;

  /* Reduce to Unix/POSIX epoch time and fractional nanoseconds */
  isec = MS_NSTIME2EPOCH (nstime);
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

  if (subseconds == 1 || (subseconds == -1 && nanosec))
  {
    /* Print nanoseconds if sub-microseconds are non-zero */
    if (submicro)
      ret = snprintf (mdtimestr, 30, "%4d-%02d-%02d %02d:%02d:%02d.%09d",
                      tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, nanosec);
    /* Otherwise, print microseconds */
    else
      ret = snprintf (mdtimestr, 30, "%4d-%02d-%02d %02d:%02d:%02d.%06d",
                      tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, microsec);
  }
  else
  {
    ret = snprintf (mdtimestr, 20, "%4d-%02d-%02d %02d:%02d:%02d",
                    tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                    tms.tm_hour, tms.tm_min, tms.tm_sec);
  }

  if (ret != 29 && ret != 26 && ret != 19)
    return NULL;
  else
    return mdtimestr;
} /* End of ms_nstime2mdtimestr() */

/***************************************************************************
 * ms_nstime2seedtimestr:
 *
 * Build a SEED time string from a high precision epoch time.
 *
 * The provided seedtimestr must have enough room for the resulting time
 * string of 28 characters, i.e. '2001,195,12:38:00.000000000' + NULL.
 *
 * The 'subseconds' flag controls whenther the sub second portion of the
 * time is included or not:
 *  1 = include subseconds
 *  0 = do not include subseconds
 * -1 = include subseconds if non-zero
 *
 * Returns a pointer to the resulting string or NULL on error.
 ***************************************************************************/
char *
ms_nstime2seedtimestr (nstime_t nstime, char *seedtimestr, flag subseconds)
{
  struct tm tms;
  int64_t isec;
  int nanosec;
  int microsec;
  int submicro;
  int ret;

  if (seedtimestr == NULL)
    return NULL;

  /* Reduce to Unix/POSIX epoch time and fractional nanoseconds */
  isec = MS_NSTIME2EPOCH (nstime);
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

  if (subseconds == 1 || (subseconds == -1 && nanosec))
  {
    /* Print nanoseconds if sub-microseconds are non-zero */
    if (submicro)
      ret = snprintf (seedtimestr, 28, "%4d,%03d,%02d:%02d:%02d.%09d",
                      tms.tm_year + 1900, tms.tm_yday + 1,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, nanosec);
    /* Otherwise, print microseconds */
    else
      ret = snprintf (seedtimestr, 28, "%4d,%03d,%02d:%02d:%02d.%06d",
                      tms.tm_year + 1900, tms.tm_yday + 1,
                      tms.tm_hour, tms.tm_min, tms.tm_sec, microsec);
  }
  else
  {
    ret = snprintf (seedtimestr, 18, "%4d,%03d,%02d:%02d:%02d",
                    tms.tm_year + 1900, tms.tm_yday + 1,
                    tms.tm_hour, tms.tm_min, tms.tm_sec);
  }

  if (ret != 27 && ret != 24 && ret != 17)
    return NULL;
  else
    return seedtimestr;
} /* End of ms_nstime2seedtimestr() */

/***************************************************************************
 * ms_time2nstime_int:
 *
 * Convert specified time values to a high precision epoch time.  This
 * is an internal version which does no range checking, it is assumed
 * that checking the range for each value has already been done.
 *
 * Returns epoch time on success and NSTERROR on error.
 ***************************************************************************/
static nstime_t
ms_time2nstime_int (int year, int day, int hour, int min, int sec, uint32_t nsec)
{
  nstime_t nstime;
  int shortyear;
  int a4, a100, a400;
  int intervening_leap_days;
  int days;

  shortyear = year - 1900;

  a4 = (shortyear >> 2) + 475 - !(shortyear & 3);
  a100 = a4 / 25 - (a4 % 25 < 0);
  a400 = a100 >> 2;
  intervening_leap_days = (a4 - 492) - (a100 - 19) + (a400 - 4);

  days = (365 * (shortyear - 70) + intervening_leap_days + (day - 1));

  nstime = (nstime_t) (60 * (60 * ((nstime_t)24 * days + hour) + min) + sec) * NSTMODULUS + nsec;

  return nstime;
} /* End of ms_time2nstime_int() */

/***************************************************************************
 * ms_time2nstime:
 *
 * Convert specified time values to a high precision epoch time.  This
 * is essentially a frontend for ms_time2nstime that does range
 * checking for each input value.
 *
 * Expected ranges:
 * year : 1800 - 5000
 * day  : 1 - 366
 * hour : 0 - 23
 * min  : 0 - 59
 * sec  : 0 - 60
 * nsec : 0 - 999999999
 *
 * Returns epoch time on success and NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_time2nstime (int year, int day, int hour, int min, int sec, uint32_t nsec)
{
  if (year < 1800 || year > 5000)
  {
    ms_log (2, "ms_time2nstime(): Error with year value: %d\n", year);
    return NSTERROR;
  }

  if (day < 1 || day > 366)
  {
    ms_log (2, "ms_time2nstime(): Error with day value: %d\n", day);
    return NSTERROR;
  }

  if (hour < 0 || hour > 23)
  {
    ms_log (2, "ms_time2nstime(): Error with hour value: %d\n", hour);
    return NSTERROR;
  }

  if (min < 0 || min > 59)
  {
    ms_log (2, "ms_time2nstime(): Error with minute value: %d\n", min);
    return NSTERROR;
  }

  if (sec < 0 || sec > 60)
  {
    ms_log (2, "ms_time2nstime(): Error with second value: %d\n", sec);
    return NSTERROR;
  }

  if (nsec > 999999999)
  {
    ms_log (2, "ms_time2nstime(): Error with nanosecond value: %d\n", nsec);
    return NSTERROR;
  }

  return ms_time2nstime_int (year, day, hour, min, sec, nsec);
} /* End of ms_time2nstime() */

/***************************************************************************
 * ms_seedtimestr2nstime:
 *
 * Convert a SEED time string (day-of-year style) to a high precision
 * epoch time.  The time format expected is
 * "YYYY[,DDD,HH,MM,SS.FFFFFFFFF]", the delimiter can be a dash [-],
 * comma [,], colon [:] or period [.].  Additionally a [T] or space
 * may be used to seprate the day and hour fields.  The fractional
 * seconds ("FFFFFFFFF") must begin with a period [.] if present.
 *
 * The time string can be "short" in which case the omitted values are
 * assumed to be zero (with the exception of DDD which is assumed to
 * be 1): "YYYY,DDD,HH" assumes MM, SS and FFFFFFFF are 0.  The year
 * is required, otherwise there wouldn't be much for a date.
 *
 * Ranges are checked for each value.
 *
 * Returns epoch time on success and NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_seedtimestr2nstime (char *seedtimestr)
{
  int fields;
  int year = 0;
  int day = 1;
  int hour = 0;
  int min = 0;
  int sec = 0;
  double fsec = 0.0;
  int nsec = 0;

  fields = sscanf (seedtimestr, "%d%*[-,:.]%d%*[-,:.Tt ]%d%*[-,:.]%d%*[-,:.]%d%lf",
                   &year, &day, &hour, &min, &sec, &fsec);

  /* Convert fractional seconds to nanoseconds */
  if (fsec != 0.0)
  {
    nsec = (int)(fsec * 1000000000.0 + 0.5);
  }

  if (fields < 1)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error converting time string: %s\n", seedtimestr);
    return NSTERROR;
  }

  if (year < 1800 || year > 5000)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with year value: %d\n", year);
    return NSTERROR;
  }

  if (day < 1 || day > 366)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with day value: %d\n", day);
    return NSTERROR;
  }

  if (hour < 0 || hour > 23)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with hour value: %d\n", hour);
    return NSTERROR;
  }

  if (min < 0 || min > 59)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with minute value: %d\n", min);
    return NSTERROR;
  }

  if (sec < 0 || sec > 60)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with second value: %d\n", sec);
    return NSTERROR;
  }

  if (nsec < 0 || nsec > 999999999)
  {
    ms_log (2, "ms_seedtimestr2nstime(): Error with fractional second value: %d\n", nsec);
    return NSTERROR;
  }

  return ms_time2nstime_int (year, day, hour, min, sec, nsec);
} /* End of ms_seedtimestr2nstime() */

/***************************************************************************
 * ms_timestr2nstime:
 *
 * Convert a generic time string to a high precision epoch time.  The
 * time format expected is "YYYY[/MM/DD HH:MM:SS.FFFFFFFFF]", the
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
 * Ranges are checked for each value.
 *
 * Returns epoch time on success and NSTERROR on error.
 ***************************************************************************/
nstime_t
ms_timestr2nstime (char *timestr)
{
  int fields;
  int year = 0;
  int mon = 1;
  int mday = 1;
  int day = 1;
  int hour = 0;
  int min = 0;
  int sec = 0;
  double fsec = 0.0;
  int nsec = 0;

  fields = sscanf (timestr, "%d%*[-,/:.]%d%*[-,/:.]%d%*[-,/:.Tt ]%d%*[-,/:.]%d%*[-,/:.]%d%lf",
                   &year, &mon, &mday, &hour, &min, &sec, &fsec);

  /* Convert fractional seconds to nanoseconds */
  if (fsec != 0.0)
  {
    nsec = (int)(fsec * 1000000000.0 + 0.5);
  }

  if (fields < 1)
  {
    ms_log (2, "ms_timestr2nstime(): Error converting time string: %s\n", timestr);
    return NSTERROR;
  }

  if (year < 1800 || year > 5000)
  {
    ms_log (2, "ms_timestr2nstime(): Error with year value: %d\n", year);
    return NSTERROR;
  }

  if (mon < 1 || mon > 12)
  {
    ms_log (2, "ms_timestr2nstime(): Error with month value: %d\n", mon);
    return NSTERROR;
  }

  if (mday < 1 || mday > 31)
  {
    ms_log (2, "ms_timestr2nstime(): Error with day value: %d\n", mday);
    return NSTERROR;
  }

  /* Convert month and day-of-month to day-of-year */
  if (ms_md2doy (year, mon, mday, &day))
  {
    return NSTERROR;
  }

  if (hour < 0 || hour > 23)
  {
    ms_log (2, "ms_timestr2nstime(): Error with hour value: %d\n", hour);
    return NSTERROR;
  }

  if (min < 0 || min > 59)
  {
    ms_log (2, "ms_timestr2nstime(): Error with minute value: %d\n", min);
    return NSTERROR;
  }

  if (sec < 0 || sec > 60)
  {
    ms_log (2, "ms_timestr2nstime(): Error with second value: %d\n", sec);
    return NSTERROR;
  }

  if (nsec < 0 || nsec > 999999999)
  {
    ms_log (2, "ms_timestr2nstime(): Error with fractional second value: %d\n", nsec);
    return NSTERROR;
  }

  return ms_time2nstime_int (year, day, hour, min, sec, nsec);
} /* End of ms_timestr2nstime() */

/***************************************************************************
 * ms_dabs:
 *
 * Determine the absolute value of an input double, actually just test
 * if the input double is positive multiplying by -1.0 if not and
 * return it.
 *
 * Returns the positive value of input double.
 ***************************************************************************/
double
ms_dabs (double val)
{
  if (val < 0.0)
    val *= -1.0;
  return val;
} /* End of ms_dabs() */

/***************************************************************************
 * ms_readleapseconds:
 *
 * Read leap seconds from a file indicated by the specified
 * environment variable and populate the global leapsecondlist.
 *
 * Returns positive number of leap seconds read, -1 on file read
 * error, and -2 when the environment variable is not set.
 ***************************************************************************/
int
ms_readleapseconds (char *envvarname)
{
  char *filename;

  if ((filename = getenv (envvarname)))
  {
    return ms_readleapsecondfile (filename);
  }

  return -2;
} /* End of ms_readleapseconds() */

/***************************************************************************
 * ms_readleapsecondfile:
 *
 * Read leap seconds from the specified file and populate the global
 * leapsecondlist.  The file is expected to be standard IETF leap
 * second list format.  The list is usually available from:
 * https://www.ietf.org/timezones/data/leap-seconds.list
 *
 * Returns positive number of leap seconds read on success and -1 on error.
 ***************************************************************************/
int
ms_readleapsecondfile (char *filename)
{
  FILE *fp = NULL;
  LeapSecond *ls = NULL;
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
    free(leapsecondlist);
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
      fields = sscanf (readline, "#@ %" SCNd64, &expires);

      if (fields == 1)
      {
        /* Convert expires to Unix epoch */
        expires = expires - NTPPOSIXEPOCHDELTA;

        /* Compare expire time to current time */
        if (time (NULL) > expires)
        {
          char timestr[100];
          ms_nstime2mdtimestr (MS_EPOCH2NSTIME (expires), timestr, 0);
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
      if ((ls = malloc (sizeof (LeapSecond))) == NULL)
      {
        ms_log (2, "Cannot allocate LeapSecond, out of memory?\n");
        return -1;
      }

      /* Convert NTP epoch time to Unix epoch time and then to nttime_t */
      ls->leapsecond = MS_EPOCH2NSTIME ((leapsecond - NTPPOSIXEPOCHDELTA));
      ls->TAIdelta = TAIdelta;
      ls->next = NULL;
      count++;

      /* Add leap second to global list */
      if (!leapsecondlist)
      {
        leapsecondlist = ls;
        lastls = ls;
      }
      else
      {
        lastls->next = ls;
        lastls = ls;
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
