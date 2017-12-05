/***************************************************************************
 * selection.c:
 *
 * Generic routines to manage selection lists.
 *
 * Written by Chad Trabant unless otherwise noted
 *   IRIS Data Management Center
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"

static int ms_isinteger (char *string);
static int ms_globmatch (char *string, char *pattern);

/***************************************************************************
 * ms3_matchselect:
 *
 * Test the specified parameters for a matching selection entry.  The
 * tsidpattern parameter may contain globbing characters.  The NULL value
 * (matching any times) for the start and end times is NSTERROR.
 *
 * Matching includes:
 *  glob match of tsid against tsidpattern in selection
 *  time window intersection with range in selection
 *  equal pubversion if selection pubversion > 0
 *
 * Return MS3Selections pointer to matching entry on successful match and
 * NULL for no match or error.
 ***************************************************************************/
MS3Selections *
ms3_matchselect (MS3Selections *selections, char *tsid, nstime_t starttime,
                 nstime_t endtime, int pubversion, MS3SelectTime **ppselecttime)
{
  MS3Selections *findsl = NULL;
  MS3SelectTime *findst = NULL;
  MS3SelectTime *matchst = NULL;

  if (selections)
  {
    findsl = selections;
    while (findsl)
    {
      if (ms_globmatch (tsid, findsl->tsidpattern))
      {
        if (findsl->pubversion > 0 && findsl->pubversion == pubversion)
        {
          findsl = findsl->next;
          continue;
        }

        findst = findsl->timewindows;
        while (findst)
        {
          if (starttime != NSTERROR && findst->starttime != NSTERROR &&
              (starttime < findst->starttime && !(starttime <= findst->starttime && endtime >= findst->starttime)))
          {
            findst = findst->next;
            continue;
          }
          else if (endtime != NSTERROR && findst->endtime != NSTERROR &&
                   (endtime > findst->endtime && !(starttime <= findst->endtime && endtime >= findst->endtime)))
          {
            findst = findst->next;
            continue;
          }

          matchst = findst;
          break;
        }
      }

      if (matchst)
        break;
      else
        findsl = findsl->next;
    }
  }

  if (ppselecttime)
    *ppselecttime = matchst;

  return (matchst) ? findsl : NULL;
} /* End of ms3_matchselect() */

/***************************************************************************
 * msr3_matchselect:
 *
 * A simple wrapper for calling ms3_matchselect() using details from a
 * MSRecord struct.
 *
 * Return MS3Selections pointer to matching entry on successful match and
 * NULL for no match or error.
 ***************************************************************************/
MS3Selections *
msr3_matchselect (MS3Selections *selections, MS3Record *msr, MS3SelectTime **ppselecttime)
{
  nstime_t endtime;

  if (!selections || !msr)
    return NULL;

  endtime = msr3_endtime (msr);

  return ms3_matchselect (selections, msr->tsid, msr->starttime, endtime,
                          msr->pubversion, ppselecttime);
} /* End of msr3_matchselect() */

/***************************************************************************
 * ms3_addselect:
 *
 * Add select parameters to a specified selection list.  The tsidpattern
 * argument may contain globbing parameters.  The NULL value (matching
 * any value) for the start and end times is NSTERROR.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
int
ms3_addselect (MS3Selections **ppselections, char *tsidpattern,
               nstime_t starttime, nstime_t endtime, uint8_t pubversion)
{
  MS3Selections *newsl = NULL;
  MS3SelectTime *newst = NULL;

  if (!ppselections || !tsidpattern)
    return -1;

  /* Allocate new SelectTime and populate */
  if (!(newst = (MS3SelectTime *)calloc (1, sizeof (MS3SelectTime))))
  {
    ms_log (2, "Cannot allocate memory\n");
    return -1;
  }

  newst->starttime = starttime;
  newst->endtime = endtime;

  /* Add new Selections struct to begining of list */
  if (!*ppselections)
  {
    /* Allocate new Selections and populate */
    if (!(newsl = (MS3Selections *)calloc (1, sizeof (MS3Selections))))
    {
      ms_log (2, "Cannot allocate memory\n");
      return -1;
    }

    strncpy (newsl->tsidpattern, tsidpattern, sizeof (newsl->tsidpattern));
    newsl->tsidpattern[sizeof (newsl->tsidpattern) - 1] = '\0';
    newsl->pubversion = pubversion;

    /* Add new MS3SelectTime struct as first in list */
    *ppselections = newsl;
    newsl->timewindows = newst;
  }
  else
  {
    MS3Selections *findsl = *ppselections;
    MS3Selections *matchsl = 0;

    /* Search for matching MS3Selections entry */
    while (findsl)
    {
      if (!strcmp (findsl->tsidpattern, tsidpattern) && findsl->pubversion == pubversion)
      {
        matchsl = findsl;
        break;
      }

      findsl = findsl->next;
    }

    if (matchsl)
    {
      /* Add time window selection to beginning of window list */
      newst->next = matchsl->timewindows;
      matchsl->timewindows = newst;
    }
    else
    {
      /* Allocate new MS3Selections and populate */
      if (!(newsl = (MS3Selections *)calloc (1, sizeof (MS3Selections))))
      {
        ms_log (2, "Cannot allocate memory\n");
        return -1;
      }

      strncpy (newsl->tsidpattern, tsidpattern, sizeof (newsl->tsidpattern));
      newsl->tsidpattern[sizeof (newsl->tsidpattern) - 1] = '\0';
      newsl->pubversion = pubversion;

      /* Add new MS3Selections to beginning of list */
      newsl->next = *ppselections;
      *ppselections = newsl;
      newsl->timewindows = newst;
    }
  }

  return 0;
} /* End of ms3_addselect() */

/***************************************************************************
 * ms3_addselect_comp:
 *
 * Add select parameters to a specified selection list based on
 * separate name components.  The network, station, location, channel
 * and quality arguments may contain globbing parameters.  The NULL
 * value (matching any value) for the start and end times is NSTERROR.
 *
 * If any of the naming parameters are not supplied (pointer is NULL)
 * a wildcard for all matches is substituted.  As a special case, if
 * the location ID (loc) is set to "--" to match a space-space/blank
 * ID it will be translated to an empty string to match libmseed's
 * notation.
 *
 * Return 0 on success and -1 on error.
 ***************************************************************************/
int
ms3_addselect_comp (MS3Selections **ppselections, char *net, char *sta, char *loc,
                    char *chan, nstime_t starttime, nstime_t endtime, uint8_t pubversion)
{
  char tsidpattern[100];
  char selnet[20];
  char selsta[20];
  char selloc[20];
  char selchan[20];

  if (!ppselections)
    return -1;

  if (net)
  {
    strncpy (selnet, net, sizeof (selnet));
    selnet[sizeof (selnet) - 1] = '\0';
  }
  else
  {
    strcpy (selnet, "*");
  }

  if (sta)
  {
    strncpy (selsta, sta, sizeof (selsta));
    selsta[sizeof (selsta) - 1] = '\0';
  }
  else
  {
    strcpy (selsta, "*");
  }

  if (loc)
  {
    /* Test for special case blank location ID */
    if (!strcmp (loc, "--"))
    {
      selloc[0] = '\0';
    }
    else
    {
      strncpy (selloc, loc, sizeof (selloc));
      selloc[sizeof (selloc) - 1] = '\0';
    }
  }
  else
  {
    strcpy (selloc, "*");
  }

  if (chan)
  {
    strncpy (selchan, chan, sizeof (selchan));
    selchan[sizeof (selchan) - 1] = '\0';
  }
  else
  {
    strcpy (selchan, "*");
  }

  /* Create the time series identifier globbing match for this entry */
  if (ms_nslc2tsid (tsidpattern, sizeof (tsidpattern), 0,
                    selnet, selsta, selloc, selchan))
    return -1;

  /* Add selection to list */
  if (ms3_addselect (ppselections, tsidpattern, starttime, endtime, pubversion))
    return -1;

  return 0;
} /* End of ms3_addselect_comp() */

#define MAX_SELECTION_FIELDS 8

/***************************************************************************
 * ms3_readselectionsfile:
 *
 * Read a list of data selections from a file and add them to the
 * specified selections list.  On errors this routine will leave
 * allocated memory unreachable (leaked), it is expected that this is
 * a program failing condition.
 *
 * As a special case if the filename is "-", selection lines will be
 * read from stdin.
 *
 * The selection file may contain two line formats:
 *   "TimeseriesID  [Starttime  [Endtime  [Pubversion]]]"
 *  or
 *   "Network  Station  Location  Channel  [Quality  [Starttime  [Endtime]]]"
 *
 * In the later version, the Quality field is ignored as of libmseed version 3.
 *
 * Returns count of selections added on success and -1 on error.
 ***************************************************************************/
int
ms3_readselectionsfile (MS3Selections **ppselections, char *filename)
{
  FILE *fp;
  nstime_t starttime;
  nstime_t endtime;
  uint8_t pubversion;
  char selectline[200];
  char *line;
  char *fields[MAX_SELECTION_FIELDS];
  char *cp;
  char next;
  int selectcount = 0;
  int linecount = 0;
  int fieldidx;
  uint8_t isstart2;
  uint8_t isend3;
  uint8_t isstart6;
  uint8_t isend7;

  if (!ppselections || !filename)
    return -1;

  if (strcmp (filename, "-"))
  {
    if (!(fp = fopen (filename, "rb")))
    {
      ms_log (2, "Cannot open file %s: %s\n", filename, strerror (errno));
      return -1;
    }
  }
  else
  {
    /* Use stdin as special case */
    fp = stdin;
  }

  while (fgets (selectline, sizeof (selectline) - 1, fp))
  {
    linecount++;

    /* Reset fields array */
    for (fieldidx = 0; fieldidx < MAX_SELECTION_FIELDS; fieldidx++)
      fields[fieldidx] = NULL;

    /* Guarantee termination */
    selectline[sizeof (selectline) - 1] = '\0';

    line = selectline;

    /* Trim trailing whitespace (including newlines, carriage returns, etc.) if any */
    cp = line;
    while (*cp != '\0')
    {
      cp++;
    }
    cp--;
    while (cp >= line && isspace((int)(*cp)))
    {
      *cp = '\0';
      cp--;
    }

    /* Trim leading whitespace if any */
    cp = line;
    while (isspace((int)(*cp)))
    {
      line = cp = cp+1;
    }

    printf ("DEBUG line: '%s'\n", line);

    /* Skip empty lines */
    if (!strlen (line))
      continue;

    /* Skip comment lines */
    if (*line == '#')
      continue;

    /* Set fields array to whitespace delimited fields */
    cp = line;
    next = 0;  /* For this loop: 0 = whitespace, 1 = non-whitespace */
    fieldidx = 0;
    while (*cp && fieldidx < MAX_SELECTION_FIELDS)
    {
      if (!isspace ((int)(*cp)))
      {
        /* Field starts at transition from whitespace to non-whitespace */
        if (next == 0)
        {
          fields[fieldidx] = cp;
          fieldidx++;
        }

        next = 1;
      }
      else
      {
        /* Field ends at transition from non-whitespace to whitespace */
        if (next == 1)
          *cp = '\0';

        next = 0;
      }

      cp++;
    }

/* Globbing pattern to match the beginning of a date "YYYY[-/,]#..." */
#define INITDATEGLOB "[0-9][0-9][0-9][0-9][-/,][0-9]*"

    isstart2 = (fields[1]) ? ms_globmatch (fields[1], INITDATEGLOB) : 0;
    isend3 = (fields[2]) ? ms_globmatch (fields[2], INITDATEGLOB) : 0;
    isstart6 = (fields[5]) ? ms_globmatch (fields[5], INITDATEGLOB) : 0;
    isend7 = (fields[6]) ? ms_globmatch (fields[6], INITDATEGLOB) : 0;

    /* Convert starttime to nstime_t */
    starttime = NSTERROR;
    cp = NULL;
    if (isstart2)
      cp = fields[1];
    else if (isstart6)
      cp = fields[5];
    if (cp)
    {
      starttime = ms_seedtimestr2nstime (cp);
      if (starttime == NSTERROR)
      {
        ms_log (2, "Cannot convert data selection start time (line %d): %s\n", linecount, cp);
        return -1;
      }
    }

    /* Convert endtime to nstime_t */
    endtime = NSTERROR;
    cp = NULL;
    if (isend3)
      cp = fields[2];
    else if (isend7)
      cp = fields[6];
    if (cp)
    {
      endtime = ms_seedtimestr2nstime (cp);
      if (endtime == NSTERROR)
      {
        ms_log (2, "Cannot convert data selection end time (line %d): %s\n", linecount, cp);
        return -1;
      }
    }

    /* Test for "TimeseriesID  [Starttime  [Endtime  [Pubversion]]]" */
    if (fieldidx == 1 ||
        (fieldidx == 2 && isstart2) ||
        (fieldidx == 3 && isstart2 && isend3) ||
        (fieldidx == 4 && isstart2 && isend3 && ms_isinteger(fields[3])))
    {
      /* Convert publication version to integer */
      pubversion = 0;
      if (fields[3])
      {
        long int longpver;

        longpver = strtol (fields[3], NULL, 10);

        if (longpver < 0 || longpver > 255 )
        {
          ms_log (2, "Cannot convert publication version (line %d): %s\n", linecount, fields[3]);
          return -1;
        }
        else
        {
          pubversion = (uint8_t) longpver;
        }
      }

      /* Add selection to list */
      if (ms3_addselect (ppselections, fields[0], starttime, endtime, pubversion))
      {
        ms_log (2, "[%s] Error adding selection on line %d\n", filename, linecount);
        return -1;
      }
    }
    /* Test for "Network  Station  Location  Channel  [Quality  [Starttime  [Endtime]]]" */
    else if (fieldidx == 4 || fieldidx == 5 ||
             (fieldidx == 6 && isstart6) ||
             (fieldidx == 7 && isstart6 && isend7))
    {
      /* Convert quality field to publication version if integer */
      pubversion = 0;
      if (fields[4] && ms_isinteger(fields[4]))
      {
        long int longpver;

        longpver = strtol (fields[4], NULL, 10);

        if (longpver < 0 || longpver > 255 )
        {
          ms_log (2, "Cannot convert publication version (line %d): %s\n", linecount, fields[4]);
          return -1;
        }
        else
        {
          pubversion = (uint8_t) longpver;
        }
      }

      if (ms3_addselect_comp (ppselections, fields[0], fields[1], fields[2], fields[3],
                              starttime, endtime, pubversion))
      {
        ms_log (2, "[%s] Error adding selection on line %d\n", filename, linecount);
        return -1;
      }
    }
    else
    {
      ms_log (2, "[%s] Skipping unrecognized data selection on line %d\n", filename, linecount);
    }

    selectcount++;
  }

  if (fp != stdin)
    fclose (fp);

  return selectcount;
} /* End of ms_readselectionsfile() */

/***************************************************************************
 * ms3_freeselections:
 *
 * Free all memory associated with a MS3Selections struct.
 ***************************************************************************/
void
ms3_freeselections (MS3Selections *selections)
{
  MS3Selections *select;
  MS3Selections *selectnext;
  MS3SelectTime *selecttime;
  MS3SelectTime *selecttimenext;

  if (selections)
  {
    select = selections;

    while (select)
    {
      selectnext = select->next;

      selecttime = select->timewindows;

      while (selecttime)
      {
        selecttimenext = selecttime->next;

        free (selecttime);

        selecttime = selecttimenext;
      }

      free (select);

      select = selectnext;
    }
  }

} /* End of ms3_freeselections() */

/***************************************************************************
 * ms3_printselections:
 *
 * Print the selections list using the ms_log() facility.
 ***************************************************************************/
void
ms3_printselections (MS3Selections *selections)
{
  MS3Selections *select;
  MS3SelectTime *selecttime;
  char starttime[50];
  char endtime[50];

  if (!selections)
    return;

  select = selections;
  while (select)
  {
    ms_log (0, "Selection: %s  pubversion: %d\n",
            select->tsidpattern, select->pubversion);

    selecttime = select->timewindows;
    while (selecttime)
    {
      if (selecttime->starttime != NSTERROR)
        ms_nstime2seedtimestr (selecttime->starttime, starttime, 1);
      else
        strncpy (starttime, "No start time", sizeof (starttime) - 1);

      if (selecttime->endtime != NSTERROR)
        ms_nstime2seedtimestr (selecttime->endtime, endtime, 1);
      else
        strncpy (endtime, "No end time", sizeof (endtime) - 1);

      ms_log (0, "  %30s  %30s\n", starttime, endtime);

      selecttime = selecttime->next;
    }

    select = select->next;
  }
} /* End of ms3_printselections() */

/***************************************************************************
 * ms_isinteger:
 *
 * Test a string for all digits, i.e. and integer
 *
 * Returns 1 if is integer and 0 otherwise.
 ***************************************************************************/
static int
ms_isinteger (char *string)
{
  while (*string)
  {
    if (!isdigit((int)(*string)))
      return 0;
    string++;
  }

  return 1;
}

/***********************************************************************
 * robust glob pattern matcher
 * ozan s. yigit/dec 1994
 * public domain
 *
 * glob patterns:
 *	*	matches zero or more characters
 *	?	matches any single character
 *	[set]	matches any character in the set
 *	[^set]	matches any character NOT in the set
 *		where a set is a group of characters or ranges. a range
 *		is written as two characters seperated with a hyphen: a-z denotes
 *		all characters between a to z inclusive.
 *	[-set]	set matches a literal hypen and any character in the set
 *	[]set]	matches a literal close bracket and any character in the set
 *
 *	char	matches itself except where char is '*' or '?' or '['
 *	\char	matches char, including any pattern character
 *
 * examples:
 *	a*c		ac abc abbc ...
 *	a?c		acc abc aXc ...
 *	a[a-z]c		aac abc acc ...
 *	a[-a-z]c	a-c aac abc ...
 *
 * Revision 1.4  2004/12/26  12:38:00  ct
 * Changed function name (amatch -> globmatch), variables and
 * formatting for clarity.  Also add matching header globmatch.h.
 *
 * Revision 1.3  1995/09/14  23:24:23  oz
 * removed boring test/main code.
 *
 * Revision 1.2  94/12/11  10:38:15  oz
 * charset code fixed. it is now robust and interprets all
 * variations of charset [i think] correctly, including [z-a] etc.
 *
 * Revision 1.1  94/12/08  12:45:23  oz
 * Initial revision
 ***********************************************************************/

#define GLOBMATCH_TRUE 1
#define GLOBMATCH_FALSE 0
#define GLOBMATCH_NEGATE '^' /* std char set negation char */

/***********************************************************************
 * ms_globmatch:
 *
 * Check if a string matches a globbing pattern.
 *
 * Return 0 if string does not match pattern and non-zero otherwise.
 **********************************************************************/
static int
ms_globmatch (char *string, char *pattern)
{
  int negate;
  int match;
  int c;

  while (*pattern)
  {
    if (!*string && *pattern != '*')
      return GLOBMATCH_FALSE;

    switch (c = *pattern++)
    {

    case '*':
      while (*pattern == '*')
        pattern++;

      if (!*pattern)
        return GLOBMATCH_TRUE;

      if (*pattern != '?' && *pattern != '[' && *pattern != '\\')
        while (*string && *pattern != *string)
          string++;

      while (*string)
      {
        if (ms_globmatch (string, pattern))
          return GLOBMATCH_TRUE;
        string++;
      }
      return GLOBMATCH_FALSE;

    case '?':
      if (*string)
        break;
      return GLOBMATCH_FALSE;

    /* set specification is inclusive, that is [a-z] is a, z and
	   * everything in between. this means [z-a] may be interpreted
	   * as a set that contains z, a and nothing in between.
	   */
    case '[':
      if (*pattern != GLOBMATCH_NEGATE)
        negate = GLOBMATCH_FALSE;
      else
      {
        negate = GLOBMATCH_TRUE;
        pattern++;
      }

      match = GLOBMATCH_FALSE;

      while (!match && (c = *pattern++))
      {
        if (!*pattern)
          return GLOBMATCH_FALSE;

        if (*pattern == '-') /* c-c */
        {
          if (!*++pattern)
            return GLOBMATCH_FALSE;
          if (*pattern != ']')
          {
            if (*string == c || *string == *pattern ||
                (*string > c && *string < *pattern))
              match = GLOBMATCH_TRUE;
          }
          else
          { /* c-] */
            if (*string >= c)
              match = GLOBMATCH_TRUE;
            break;
          }
        }
        else /* cc or c] */
        {
          if (c == *string)
            match = GLOBMATCH_TRUE;
          if (*pattern != ']')
          {
            if (*pattern == *string)
              match = GLOBMATCH_TRUE;
          }
          else
            break;
        }
      }

      if (negate == match)
        return GLOBMATCH_FALSE;

      /* If there is a match, skip past the charset and continue on */
      while (*pattern && *pattern != ']')
        pattern++;
      if (!*pattern++) /* oops! */
        return GLOBMATCH_FALSE;
      break;

    case '\\':
      if (*pattern)
        c = *pattern++;
    default:
      if (c != *string)
        return GLOBMATCH_FALSE;
      break;
    }

    string++;
  }

  return !*string;
} /* End of ms_globmatch() */
