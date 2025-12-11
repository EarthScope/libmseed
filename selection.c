/***************************************************************************
 * Generic routines to manage selection lists.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2024 Chad Trabant, EarthScope Data Services
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libmseed.h"

static int ms_isinteger (const char *string);
static int ms_globmatch (const char *string, const char *pattern);

/** ************************************************************************
 * @brief Test the specified parameters for a matching selection entry
 *
 * Search the ::MS3Selections for an entry matching the provided
 * parameters.  The ::MS3Selections.sidpattern may contain globbing
 * characters.  The ::MS3Selections.timewindows many contain start and
 * end times set to ::NSTUNSET to denote "open" times.
 *
 * Positive matching requires:
 * @parblock
 *  -# glob match of sid against sidpattern in selection
 *  -# time window intersection with range in selection
 *  -# equal pubversion if selection pubversion > 0
 * @endparblock
 *
 * @param[in] selections ::MS3Selections to search
 * @param[in] sid Source ID to match
 * @param[in] starttime Start time to match
 * @param[in] endtime End time to match
 * @param[in] pubversion Publication version to match
 * @param[out] ppselecttime Pointer-to-pointer to return the matching ::MS3SelectTime entry
 *
 * @returns A pointer to matching ::MS3Selections entry on success and NULL for
 * no match or error.
 ***************************************************************************/
const MS3Selections *
ms3_matchselect (const MS3Selections *selections, const char *sid, nstime_t starttime,
                 nstime_t endtime, int pubversion, const MS3SelectTime **ppselecttime)
{
  const MS3Selections *findsl = NULL;
  const MS3SelectTime *findst = NULL;
  const MS3SelectTime *matchst = NULL;

  if (selections)
  {
    findsl = selections;
    while (findsl)
    {
      if (ms_globmatch (sid, findsl->sidpattern))
      {
        if (findsl->pubversion > 0 && findsl->pubversion != pubversion)
        {
          findsl = findsl->next;
          continue;
        }

        /* If no time selection, this is a match */
        if (!findsl->timewindows)
        {
          if (ppselecttime)
            *ppselecttime = NULL;

          return findsl;
        }

        /* Otherwise, search the time selections */
        findst = findsl->timewindows;
        while (findst)
        {
          if (starttime != NSTERROR && starttime != NSTUNSET && findst->starttime != NSTERROR &&
              findst->starttime != NSTUNSET &&
              (starttime < findst->starttime &&
               !(starttime <= findst->starttime && endtime >= findst->starttime)))
          {
            findst = findst->next;
            continue;
          }
          else if (endtime != NSTERROR && endtime != NSTUNSET && findst->endtime != NSTERROR &&
                   findst->endtime != NSTUNSET &&
                   (endtime > findst->endtime &&
                    !(starttime <= findst->endtime && endtime >= findst->endtime)))
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

/** ************************************************************************
 * @brief Test the ::MS3Record for a matching selection entry
 *
 * Search the ::MS3Selections for an entry matching the provided
 * parameters.
 *
 * Positive matching requires:
 * @parblock
 *  -# glob match of sid against sidpattern in selection
 *  -# time window intersection with range in selection
 *  -# equal pubversion if selection pubversion > 0
 * @endparblock
 *
 * @param[in] selections ::MS3Selections to search
 * @param[in] msr ::MS3Record to match against selections
 * @param[out] ppselecttime Pointer-to-pointer to return the matching ::MS3SelectTime entry
 *
 * @returns A pointer to matching ::MS3Selections entry successful
 * match and NULL for no match or error.
 ***************************************************************************/
const MS3Selections *
msr3_matchselect (const MS3Selections *selections, const MS3Record *msr,
                  const MS3SelectTime **ppselecttime)
{
  nstime_t endtime;

  if (!selections || !msr)
    return NULL;

  endtime = msr3_endtime (msr);

  return ms3_matchselect (selections, msr->sid, msr->starttime, endtime, msr->pubversion,
                          ppselecttime);
} /* End of msr3_matchselect() */

/** ************************************************************************
 * @brief Add selection parameters to selection list.
 *
 * The @p sidpattern may contain globbing characters.
 *
 * The @p starttime and @p endtime may be set to ::NSTUNSET to denote
 * "open" times.
 *
 * The @p pubversion may be set to 0 to match any publication
 * version.
 *
 * @param[in] ppselections ::MS3Selections to add new selection to
 * @param[in] sidpattern Source ID pattern, may contain globbing characters
 * @param[in] starttime Start time for selection, ::NSTUNSET for open
 * @param[in] endtime End time for selection, ::NSTUNSET for open
 * @param[in] pubversion Publication version for selection, 0 for any
 *
 * @returns 0 on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
ms3_addselect (MS3Selections **ppselections, const char *sidpattern, nstime_t starttime,
               nstime_t endtime, uint8_t pubversion)
{
  MS3Selections *newsl = NULL;
  MS3SelectTime *newst = NULL;

  if (!ppselections || !sidpattern)
  {
    ms_log (2, "%s(): Required input not defined: 'ppselections' or 'sidpattern'\n", __func__);
    return -1;
  }

  /* Allocate new SelectTime and populate */
  if (!(newst = (MS3SelectTime *)libmseed_memory.malloc (sizeof (MS3SelectTime))))
  {
    ms_log (2, "Cannot allocate memory\n");
    return -1;
  }
  memset (newst, 0, sizeof (MS3SelectTime));

  newst->starttime = starttime;
  newst->endtime = endtime;

  /* Add new Selections struct to begining of list */
  if (!*ppselections)
  {
    /* Allocate new Selections and populate */
    if (!(newsl = (MS3Selections *)libmseed_memory.malloc (sizeof (MS3Selections))))
    {
      ms_log (2, "Cannot allocate memory\n");
      return -1;
    }
    memset (newsl, 0, sizeof (MS3Selections));

    strncpy (newsl->sidpattern, sidpattern, sizeof (newsl->sidpattern));
    newsl->sidpattern[sizeof (newsl->sidpattern) - 1] = '\0';
    newsl->pubversion = pubversion;

    /* Add new MS3SelectTime struct as first in list */
    *ppselections = newsl;
    newsl->timewindows = newst;
  }
  else
  {
    MS3Selections *findsl = *ppselections;
    MS3Selections *matchsl = NULL;

    /* Search for matching MS3Selections entry */
    while (findsl)
    {
      if (!strcmp (findsl->sidpattern, sidpattern) && findsl->pubversion == pubversion)
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
      if (!(newsl = (MS3Selections *)libmseed_memory.malloc (sizeof (MS3Selections))))
      {
        ms_log (2, "Cannot allocate memory\n");
        return -1;
      }
      memset (newsl, 0, sizeof (MS3Selections));

      strncpy (newsl->sidpattern, sidpattern, sizeof (newsl->sidpattern));
      newsl->sidpattern[sizeof (newsl->sidpattern) - 1] = '\0';
      newsl->pubversion = pubversion;

      /* Add new MS3Selections to beginning of list */
      newsl->next = *ppselections;
      *ppselections = newsl;
      newsl->timewindows = newst;
    }
  }

  return 0;
} /* End of ms3_addselect() */

/** ************************************************************************
 * @brief Add selection parameters to a selection list based on
 * separate source name codes
 *
 * The @p network, @p station, @p location, and @p channel arguments may
 * contain globbing parameters.

 * The @p starttime and @p endtime may be set to ::NSTUNSET to denote
 * "open" times.
 *
 * The @p pubversion may be set to 0 to match any publication
 * version.
 *
 * If any of the naming parameters are not supplied (pointer is NULL)
 * a wildcard for all matches is substituted.
 *
 * As a special case, if the location code is set to @c "--" to match an empty
 * location code it will be translated to an empty string to match the internal
 * handling of empty location codes.
 *
 * @param[in] ppselections ::MS3Selections to add new selection to
 * @param[in] network Network code, may contain globbing characters
 * @param[in] station Station code, may contain globbing characters
 * @param[in] location Location code, may contain globbing characters
 * @param[in] channel Channel code, may contain globbing characters
 * @param[in] starttime Start time for selection, ::NSTUNSET for open
 * @param[in] endtime End time for selection, ::NSTUNSET for open
 * @param[in] pubversion Publication version for selection, 0 for any
 *
 * @return 0 on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
ms3_addselect_comp (MS3Selections **ppselections, char *network, char *station, char *location,
                    char *channel, nstime_t starttime, nstime_t endtime, uint8_t pubversion)
{
  char sidpattern[100];
  char selnet[20];
  char selsta[20];
  char selloc[20];
  char selchan[20];

  if (!ppselections)
  {
    ms_log (2, "%s(): Required input not defined: 'ppselections'\n", __func__);
    return -1;
  }

  if (network)
  {
    strncpy (selnet, network, sizeof (selnet));
    selnet[sizeof (selnet) - 1] = '\0';
  }
  else
  {
    strcpy (selnet, "*");
  }

  if (station)
  {
    strncpy (selsta, station, sizeof (selsta));
    selsta[sizeof (selsta) - 1] = '\0';
  }
  else
  {
    strcpy (selsta, "*");
  }

  if (location)
  {
    /* Test for special case blank location ID */
    if (!strcmp (location, "--"))
    {
      selloc[0] = '\0';
    }
    else
    {
      strncpy (selloc, location, sizeof (selloc));
      selloc[sizeof (selloc) - 1] = '\0';
    }
  }
  else
  {
    strcpy (selloc, "*");
  }

  if (channel)
  {
    /* Convert a 3-character SEED 2.x channel code to an extended code */
    if (ms_globmatch (channel, "[?*a-zA-Z0-9][?*a-zA-Z0-9][?*a-zA-Z0-9]"))
    {
      ms_seedchan2xchan (selchan, channel);
    }
    else
    {
      strncpy (selchan, channel, sizeof (selchan));
      selchan[sizeof (selchan) - 1] = '\0';
    }
  }
  else
  {
    strcpy (selchan, "*");
  }

  /* Create the source identifier globbing match for this entry */
  if (ms_nslc2sid (sidpattern, sizeof (sidpattern), 0, selnet, selsta, selloc, selchan) < 0)
    return -1;

  /* Add selection to list */
  if (ms3_addselect (ppselections, sidpattern, starttime, endtime, pubversion))
    return -1;

  return 0;
} /* End of ms3_addselect_comp() */

#define MAX_SELECTION_FIELDS 8

/** ************************************************************************
 * @brief Read data selections from a file
 *
 * Selections from a file are added to the specified selections list.
 * On errors this routine will leave allocated memory unreachable
 * (leaked), it is expected that this is a program failing condition.
 *
 * As a special case if the filename is "-", selection lines will be
 * read from stdin.
 *
 * Each line of the file contains a single selection and may be one of
 * these two line formats:
 * @code
 *   SourceID  [Starttime  [Endtime  [Pubversion]]]
 * @endcode
 *  or
 * @code
 *   Network  Station  Location  Channel  [Pubversion  [Starttime  [Endtime]]]
 * @endcode
 *
 * The @c Starttime and @c Endtime values must be in a form recognized
 * by ms_timestr2nstime() and include a full date (i.e. just a year is
 * not allowed).
 *
 * In the latter version, if the "Channel" field is a SEED 2.x channel
 * (3-characters) it will automatically be converted into extended
 * channel form (band_source_subsource).
 *
 * In the latter version, the "Pubversion" field, which was "Quality"
 * in earlier versions of the library, is assumed to be a publication
 * version if it is an integer, otherwise it is ignored.
 *
 * @returns Count of selections added on success and -1 on error.
 *
 * @ref MessageOnError - this function logs a message on error
 ***************************************************************************/
int
ms3_readselectionsfile (MS3Selections **ppselections, const char *filename)
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
  {
    ms_log (2, "%s(): Required input not defined: 'ppselections' or 'filename'\n", __func__);
    return -1;
  }

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
    while (cp >= line && isspace ((int)(*cp)))
    {
      *cp = '\0';
      cp--;
    }

    /* Trim leading whitespace if any */
    cp = line;
    while (isspace ((int)(*cp)))
    {
      line = cp = cp + 1;
    }

    /* Skip empty lines */
    if (!strlen (line))
      continue;

    /* Skip comment lines */
    if (*line == '#')
      continue;

    /* Set fields array to whitespace delimited fields */
    cp = line;
    next = 0; /* For this loop: 0 = whitespace, 1 = non-whitespace */
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
    starttime = NSTUNSET;
    cp = NULL;
    if (isstart2)
      cp = fields[1];
    else if (isstart6)
      cp = fields[5];
    if (cp)
    {
      starttime = ms_timestr2nstime (cp);
      if (starttime == NSTUNSET)
      {
        ms_log (2, "Cannot convert data selection start time (line %d): %s\n", linecount, cp);
        return -1;
      }
    }

    /* Convert endtime to nstime_t */
    endtime = NSTUNSET;
    cp = NULL;
    if (isend3)
      cp = fields[2];
    else if (isend7)
      cp = fields[6];
    if (cp)
    {
      endtime = ms_timestr2nstime (cp);
      if (endtime == NSTUNSET)
      {
        ms_log (2, "Cannot convert data selection end time (line %d): %s\n", linecount, cp);
        return -1;
      }
    }

    /* Test for "SourceID  [Starttime  [Endtime  [Pubversion]]]" */
    if (fieldidx == 1 || (fieldidx == 2 && isstart2) || (fieldidx == 3 && isstart2 && isend3) ||
        (fieldidx == 4 && isstart2 && isend3 && ms_isinteger (fields[3])))
    {
      /* Convert publication version to integer */
      pubversion = 0;
      if (fields[3])
      {
        long int longpver;

        longpver = strtol (fields[3], NULL, 10);

        if (longpver < 0 || longpver > 255)
        {
          ms_log (2, "Cannot convert publication version (line %d): %s\n", linecount, fields[3]);
          return -1;
        }
        else
        {
          pubversion = (uint8_t)longpver;
        }
      }

      /* Add selection to list */
      if (ms3_addselect (ppselections, fields[0], starttime, endtime, pubversion))
      {
        ms_log (2, "%s: Error adding selection on line %d\n", filename, linecount);
        return -1;
      }
    }
    /* Test for "Network  Station  Location  Channel  [Quality  [Starttime  [Endtime]]]" */
    else if (fieldidx == 4 || fieldidx == 5 || (fieldidx == 6 && isstart6) ||
             (fieldidx == 7 && isstart6 && isend7))
    {
      /* Convert quality field to publication version if integer */
      pubversion = 0;
      if (fields[4] && ms_isinteger (fields[4]))
      {
        long int longpver;

        longpver = strtol (fields[4], NULL, 10);

        if (longpver < 0 || longpver > 255)
        {
          ms_log (2, "Cannot convert publication version (line %d): %s\n", linecount, fields[4]);
          return -1;
        }
        else
        {
          pubversion = (uint8_t)longpver;
        }
      }

      if (ms3_addselect_comp (ppselections, fields[0], fields[1], fields[2], fields[3], starttime,
                              endtime, pubversion))
      {
        ms_log (2, "%s: Error adding selection on line %d\n", filename, linecount);
        return -1;
      }
    }
    else
    {
      ms_log (1, "%s: Skipping unrecognized data selection on line %d\n", filename, linecount);
    }

    selectcount++;
  }

  if (fp != stdin)
    fclose (fp);

  return selectcount;
} /* End of ms_readselectionsfile() */

/** ************************************************************************
 * @brief Free all memory associated with a ::MS3Selections
 *
 * All memory from one or more ::MS3Selections (in a linked list) are freed.
 *
 * @param[in] selections Start of ::MS3Selections to free
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

        libmseed_memory.free (selecttime);

        selecttime = selecttimenext;
      }

      libmseed_memory.free (select);

      select = selectnext;
    }
  }

} /* End of ms3_freeselections() */

/** ************************************************************************
 * @brief Print the selections list using the ms_log() facility.
 *
 * All selections are printed with simple formatting.
 *
 * @param[in] selections Start of ::MS3Selections to print
 ***************************************************************************/
void
ms3_printselections (const MS3Selections *selections)
{
  const MS3Selections *select;
  MS3SelectTime *selecttime;
  char starttime[50];
  char endtime[50];

  if (!selections)
    return;

  select = selections;
  while (select)
  {
    ms_log (0, "Selection: %s  pubversion: %d\n", select->sidpattern, select->pubversion);

    selecttime = select->timewindows;
    while (selecttime)
    {
      if (selecttime->starttime != NSTERROR && selecttime->starttime != NSTUNSET)
        ms_nstime2timestr_n (selecttime->starttime, starttime, sizeof (starttime), ISOMONTHDAY_Z,
                             NANO_MICRO_NONE);
      else
        strncpy (starttime, "No start time", sizeof (starttime) - 1);

      if (selecttime->endtime != NSTERROR && selecttime->endtime != NSTUNSET)
        ms_nstime2timestr_n (selecttime->endtime, endtime, sizeof (endtime), ISOMONTHDAY_Z,
                             NANO_MICRO_NONE);
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
ms_isinteger (const char *string)
{
  while (*string)
  {
    if (!isdigit ((int)(*string)))
      return 0;
    string++;
  }

  return 1;
}

static int _match_charclass (const char **pp, unsigned char c);

/** ************************************************************************
 * @brief Check if a string matches a globbing pattern.
 *
 * Supported semantics:
 * `*` matches zero or more characters, e.g. `*.txt`
 * `?` matches a single character, e.g. `a?c`
 * `[]` matches a set of characters `[abc]`
 * `[a-z]` matches a range of characters `[A-Z]`
 * `[!abc]` negation, matches when no characters in the set, e.g. `[!ABC]` or `[^ABC]`
 * `[!a-z]` negation, matches when no characters in the range, e.g. `[!A-Z]` or `[^A-Z]`
 * `\` prefix to match a literal character, e.g. `\*`, `\?`, `\[`
 *
 * @param string  The string to check.
 * @param pattern The globbing pattern to match.
 *
 * @returns 0 if string does not match pattern and non-zero otherwise.
 ***************************************************************************/
static int
ms_globmatch (const char *string, const char *pattern)
{
  if (string == NULL || pattern == NULL)
    return 0;

  const char *star_p = NULL; /* position of last '*' in pattern */
  const char *star_s = NULL; /* position in string when last '*' seen */
  unsigned char c;

  if (string == NULL || pattern == NULL)
    return 0;

  for (;;)
  {
    c = (unsigned char)*pattern++;

    switch (c)
    {
    case '\0':
      /* End of pattern: must also be end of string unless a previous '*'
         can consume more characters. */
      if (*string == '\0')
        return 1;
      if (star_p)
        goto star_backtrack;
      return 0;

    case '?':
      if (*string == '\0')
        goto star_backtrack;
      string++;
      break;

    case '*':
      /* Collapse consecutive '*' */
      while (*pattern == '*')
        pattern++;

      /* Trailing '*' matches everything */
      if (*pattern == '\0')
        return 1;

      /* If the next significant pattern character is a literal, fast-forward
         the string to its next occurrence to reduce backtracking. */
      {
        unsigned char next = (unsigned char)*pattern;

        if (next == '\\' && pattern[1])
          next = (unsigned char)pattern[1];

        if (next != '?' && next != '[' && next != '*')
        {
          while (*string && (unsigned char)*string != next)
            string++;
        }
      }

      star_p = pattern - 1; /* remember position of '*' */
      star_s = string;      /* remember current string position */
      continue;

    case '[':
    {
      const char *pp = pattern;
      if (*string == '\0')
        goto star_backtrack;
      if (!_match_charclass (&pp, (unsigned char)*string))
        goto star_backtrack;
      pattern = pp;
      string++;
      break;
    }

    case '\\':
      if (*pattern)
        c = (unsigned char)*pattern++;
      /* FALLTHROUGH */

    default:
      if ((unsigned char)*string != c)
        goto star_backtrack;
      string++;
      break;
    }

    continue;

  star_backtrack:
    /* If there was a previous '*', backtrack: let it consume one more
       character and retry from pattern just after that '*'. */
    if (star_p)
    {
      if (*star_s == '\0')
        return 0;
      string = ++star_s;
      pattern = star_p + 1;
      continue;
    }
    return 0;
  }
}

/***************************************************************************
 * Character class parser helper function.
 *
 *   On entry: *pp points just past '['.
 *             If the class is negated, the next character will be '^'
 *             and is handled inside this function.
 *
 *   On return: *pp is advanced past the closing ']'.
 *
 * Return 1 if c matches the class, 0 otherwise.
 ***************************************************************************/
static int
_match_charclass (const char **pp, unsigned char c)
{
  const char *p;
  int negate = 0;
  int matched = 0;

  if (pp == NULL || *pp == NULL)
    return 0;

  p = *pp;

  /* Handle negation */
  if (*p == '^' || *p == '!')
  {
    negate = 1;
    p++;
  }

  /* Per glob rules, leading ']' is literal */
  if (*p == ']')
  {
    matched = (c == ']');
    p++;
  }

  /* Per glob rules, leading '-' is literal */
  if (*p == '-')
  {
    matched |= (c == '-');
    p++;
  }

  /* Main loop until ']' or end of string */
  while (*p && *p != ']')
  {
    unsigned char pc = (unsigned char)*p;

    if (p[1] == '-' && p[2] && p[2] != ']' && (unsigned char)pc <= (unsigned char)p[2])
    {
      /* Range X-Y (only ascending ranges are supported) */
      unsigned char start = pc;
      unsigned char end = (unsigned char)p[2];

      matched |= (c >= start && c <= end);

      p += 3; /* skip X-Y */
    }
    else
    {
      /* Literal character */
      matched |= (c == pc);
      p++;
    }
  }

  /* Malformed class (no closing ']') → no match */
  if (*p != ']')
  {
    *pp = p;
    return 0;
  }

  *pp = p + 1; /* skip ']' */

  return negate ? !matched : matched;
}
