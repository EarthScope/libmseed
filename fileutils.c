/****************************************************************************
 * Routines to manage files of miniSEED.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2019 Chad Trabant, IRIS Data Management Center
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
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "libmseed.h"

static int ms_fread (char *buf, int size, int num, FILE *stream);

/* Skip length in bytes when skipping non-data */
#define SKIPLEN 1

/* Initialize the global file reading parameters */
MS3FileParam gMS3FileParam = {NULL, "", NULL, 0, 0, 0, 0, 0};

/*****************************************************************/ /**
 * @brief Read miniSEED records from a file
 *
 * This routine is a wrapper for ms3_readmsr_selection() that uses the
 * global file reading parameters.  This routine is _not_ thread safe
 * and cannot be used to read more than one file at a time.
 *
 * See ms3_readmsr_selection() for a further description of arguments.
 *
 * @returns Return value from ms3_readmsr_selection()
 *********************************************************************/
int
ms3_readmsr (MS3Record **ppmsr, const char *msfile, int64_t *fpos,
             int8_t *last, uint32_t flags, int8_t verbose)
{
  MS3FileParam *msfp = &gMS3FileParam;

  return ms3_readmsr_selection (&msfp, ppmsr, msfile, fpos, last,
                                flags, NULL, verbose);
} /* End of ms3_readmsr() */

/*****************************************************************/ /**
 * @brief Read miniSEED records from a file in a thread-safe way
 *
 * This routine is a wrapper for ms3_readmsr_selection() that uses the
 * re-entrant capabilities.  This routine is thread safe and can be
 * used to read more than one file at a time as long as separate
 * MS3FileParam containers are used for each file.
 *
 * A ::MS3FileParam container will be allocated if \c *ppmsfp is \c
 * NULL.
 *
 * See ms3_readmsr_selection() for a further description of arguments.
 *
 * @returns Return value from ms3_readmsr_selection()
 *********************************************************************/
int
ms3_readmsr_r (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
               int64_t *fpos, int8_t *last, uint32_t flags, int8_t verbose)
{
  return ms3_readmsr_selection (ppmsfp, ppmsr, msfile, fpos, last,
                                flags, NULL, verbose);
} /* End of ms_readmsr_r() */

/**********************************************************************
 *
 * A helper routine to shift (remove bytes from the beginning of) the
 * file reading buffer for a MSFP.  The buffer length, reading offset
 * and file position indicators are all updated as necessary.
 *
 *********************************************************************/
static void
ms3_shift_msfp (MS3FileParam *msfp, int shift)
{
  if (!msfp)
    return;

  if (shift <= 0 && shift > msfp->readlength)
  {
    ms_log (2, "%s(): Cannot shift buffer, shift: %d, readlength: %d, readoffset: %d\n",
            __func__, shift, msfp->readlength, msfp->readoffset);
    return;
  }

  memmove (msfp->readbuffer, msfp->readbuffer + shift, msfp->readlength - shift);
  msfp->readlength -= shift;

  if (shift < msfp->readoffset)
  {
    msfp->readoffset -= shift;
  }
  else
  {
    msfp->filepos += (shift - msfp->readoffset);
    msfp->readoffset = 0;
  }

  return;
} /* End of ms3_shift_msfp() */

/* Macro to calculate length of unprocessed buffer */
#define MSFPBUFLEN(MSFP) (MSFP->readlength - MSFP->readoffset)

/* Macro to return current reading position */
#define MSFPREADPTR(MSFP) (MSFP->readbuffer + MSFP->readoffset)

/*****************************************************************/ /**
 * @brief Read miniSEED records from a file with optional selection
 *
 * This routine will open and read, with subsequent calls, all
 * miniSEED records in specified file.
 *
 * All file reading parameters are stored in a ::MS3FileParam
 * container and returned (via a pointer to a pointer) for the calling
 * routine to use in subsequent calls.  A ::MS3FileParam container
 * will be allocated if \c *ppmsfp is \c NULL.  This routine is thread
 * safe and can be used to read multiple files in parallel as long as
 * the file reading parameters are managed appropriately.
 *
 * If \a *fpos is not NULL it will be updated to reflect the file
 * position (offset from the beginning in bytes) from where the
 * returned record was read.  As a special case, if \a *fpos is not
 * NULL and the value it points to is less than 0 this will be
 * interpreted as a (positive) starting offset from which to begin
 * reading data allowing the caller to specify an initial read offset.
 *
 * If \a *last is not NULL it will be set to 1 when the last record in
 * the file is being returned, otherwise it will be 0.
 *
 * The \a flags argument are bit flags used to control the reading
 * process.  The following flags are supported:
 *  - ::MSF_SKIPNOTDATA - skip input that cannot be identified as
 * miniSEED
 *  - ::MSF_UNPACKDATA data samples will be unpacked
 *  - ::MSF_VALIDATECRC Validate CRC (if present in format)
 *
 * If \a selections is not NULL, the ::MS3Selections will be used to
 * limit what is returned to the caller.  Any data not matching the
 * selections will be skipped.
 *
 * After reading all the records in a file the calling program should
 * call this routine one last time with \a msfile set to NULL.  This
 * will close the file and free allocated memory.
 *
 * @param[out] ppmsfp Pointer-to-pointer of an ::MS3FileParam, which
 * contains the state of file reading across iterative calls of this
 * function.
 *
 * @param[out] ppmsr Pointer-to-pointer of an ::MS3Record, which will
 * contain a parsed record on success.
 *
 * @param[in] msfile File to read
 *
 * @param[in,out] fpos Position in file of last returned record as a
 * byte offset.  On initial call, if the referenced value is negative
 * it will be used as a starting position.
 *
 * @param[out] last Flag to indicate when the returned record is the
 * last one in the file.
 *
 * @param[in] flags Flags used to control parsing, see @ref
 * control-flags
 *
 * @param[in] selections Specify limits to which data should be
 * returned, see @ref data-selections
 *
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns ::MS_NOERROR and populates an ::MS3Record struct, at \a
 * *ppmsr, on successful read.  On error, a (negative) libmseed error
 * code is returned and *ppmsr is set to NULL.
 * @retval ::MS_ENDOFFILE on End-Of-File
 *
 * \sa @ref data-selections
 *********************************************************************/
int
ms3_readmsr_selection (MS3FileParam **ppmsfp, MS3Record **ppmsr, const char *msfile,
                       int64_t *fpos, int8_t *last, uint32_t flags,
                       MS3Selections *selections, int8_t verbose)
{
  MS3FileParam *msfp;
  uint32_t pflags = flags;

  int parseval  = 0;
  int readsize  = 0;
  int readcount = 0;
  int retcode   = MS_NOERROR;

  if (!ppmsr)
    return MS_GENERROR;

  if (!ppmsfp)
    return MS_GENERROR;

  msfp = *ppmsfp;

  /* Initialize the file read parameters if needed */
  if (!msfp)
  {
    msfp = (MS3FileParam *)libmseed_memory.malloc (sizeof (MS3FileParam));

    if (msfp == NULL)
    {
      ms_log (2, "%s(): Cannot allocate memory for MSFP\n", __func__);
      return MS_GENERROR;
    }

    /* Redirect the supplied pointer to the allocated params */
    *ppmsfp = msfp;

    msfp->fp          = NULL;
    msfp->filename[0] = '\0';
    msfp->readbuffer  = NULL;
    msfp->readlength  = 0;
    msfp->readoffset  = 0;
    msfp->filepos     = 0;
    msfp->filesize    = 0;
    msfp->recordcount = 0;
  }

  /* When cleanup is requested */
  if (msfile == NULL)
  {
    msr3_free (ppmsr);

    if (msfp->fp != NULL)
      fclose (msfp->fp);

    if (msfp->readbuffer != NULL)
      libmseed_memory.free (msfp->readbuffer);

    /* If the file parameters are the global parameters reset them */
    if (*ppmsfp == &gMS3FileParam)
    {
      gMS3FileParam.fp          = NULL;
      gMS3FileParam.filename[0] = '\0';
      gMS3FileParam.readbuffer  = NULL;
      gMS3FileParam.readlength  = 0;
      gMS3FileParam.readoffset  = 0;
      gMS3FileParam.filepos     = 0;
      gMS3FileParam.filesize    = 0;
      gMS3FileParam.recordcount = 0;
    }
    /* Otherwise free the MS3FileParam */
    else
    {
      libmseed_memory.free (*ppmsfp);
      *ppmsfp = NULL;
    }

    return MS_NOERROR;
  }

  /* Allocate reading buffer */
  if (msfp->readbuffer == NULL)
  {
    if (!(msfp->readbuffer = (char *)libmseed_memory.malloc (MAXRECLEN)))
    {
      ms_log (2, "%s(): Cannot allocate memory for read buffer\n", __func__);
      return MS_GENERROR;
    }
  }

  /* Sanity check: track if we are reading the same file */
  if (msfp->fp && strncmp (msfile, msfp->filename, sizeof (msfp->filename)))
  {
    ms_log (2, "%s() called with a different file name without being reset\n", __func__);

    /* Close previous file and reset needed variables */
    if (msfp->fp != NULL)
      fclose (msfp->fp);

    msfp->fp          = NULL;
    msfp->readlength  = 0;
    msfp->readoffset  = 0;
    msfp->filepos     = 0;
    msfp->filesize    = 0;
    msfp->recordcount = 0;
  }

  /* Open the file if needed, redirect to stdin if file is "-" */
  if (msfp->fp == NULL)
  {
    /* Store the filename for tracking */
    strncpy (msfp->filename, msfile, sizeof (msfp->filename) - 1);
    msfp->filename[sizeof (msfp->filename) - 1] = '\0';

    if (strcmp (msfile, "-") == 0)
    {
      msfp->fp = stdin;
    }
    else
    {
      if ((msfp->fp = fopen (msfile, "rb")) == NULL)
      {
        ms_log (2, "Cannot open file: %s (%s)\n", msfile, strerror (errno));
        msr3_free (ppmsr);

        return MS_GENERROR;
      }
      else
      {
        /* Determine file size */
        struct stat sbuf;

        if (fstat (fileno (msfp->fp), &sbuf))
        {
          ms_log (2, "Cannot open file: %s (%s)\n", msfile, strerror (errno));
          msr3_free (ppmsr);

          return MS_GENERROR;
        }

        msfp->filesize = (int64_t)sbuf.st_size;
      }
    }
  }

  /* Seek to a specified offset if requested */
  if (fpos != NULL && *fpos < 0)
  {
    /* Only try to seek in real files, not stdin */
    if (msfp->fp != stdin)
    {
      if (lmp_fseek64 (msfp->fp, *fpos * -1, SEEK_SET))
      {
        ms_log (2, "Cannot seek in file: %s (%s)\n", msfile, strerror (errno));

        return MS_GENERROR;
      }

      msfp->filepos    = *fpos * -1;
      msfp->readlength = 0;
      msfp->readoffset = 0;
    }
  }

  /* Zero the last record indicator */
  if (last)
    *last = 0;

  /* Defer data unpacking if selections are used by unsetting MSF_UNPACKDATA */
  if ((flags & MSF_UNPACKDATA) && selections)
    pflags &= ~(MSF_UNPACKDATA);

  /* Read data and search for records */
  for (;;)
  {
    /* Read more data into buffer if not at EOF and buffer has less than MINRECLEN
     * or more data is needed for the current record detected in buffer. */
    if (!feof (msfp->fp) && (MSFPBUFLEN (msfp) < MINRECLEN || parseval > 0))
    {
      /* Reset offsets if no unprocessed data in buffer */
      if (MSFPBUFLEN (msfp) <= 0)
      {
        msfp->readlength = 0;
        msfp->readoffset = 0;
      }
      /* Otherwise shift existing data to beginning of buffer */
      else if (msfp->readoffset > 0)
      {
        ms3_shift_msfp (msfp, msfp->readoffset);
      }

      /* Determine read size */
      readsize = (MAXRECLEN - msfp->readlength);

      /* Read data into record buffer */
      readcount = ms_fread (msfp->readbuffer + msfp->readlength, 1, readsize, msfp->fp);

      if (readcount != readsize)
      {
        if (!feof (msfp->fp))
        {
          ms_log (2, "Short read of %d bytes starting from %" PRId64 "\n",
                  readsize, msfp->filepos);
          retcode = MS_GENERROR;
          break;
        }
      }

      /* Update read buffer length */
      msfp->readlength += readcount;

      /* File position corresponding to start of buffer */
      if (msfp->fp != stdin)
        msfp->filepos = lmp_ftell64 (msfp->fp) - msfp->readlength;
    }

    /* Attempt to parse record from buffer */
    if (MSFPBUFLEN (msfp) >= MINRECLEN)
    {
      /* Set end of file flag if at EOF */
      if (feof (msfp->fp))
        pflags |= MSF_ATENDOFFILE;

      parseval = msr3_parse (MSFPREADPTR (msfp), MSFPBUFLEN (msfp), ppmsr, pflags, verbose);

      /* Record detected and parsed */
      if (parseval == 0)
      {
        /* Test against selections if supplied */
        if (selections &&
            !ms3_matchselect (selections, (*ppmsr)->sid, (*ppmsr)->starttime,
                              msr3_endtime (*ppmsr), (*ppmsr)->pubversion, NULL))
        {
          if (verbose > 1)
          {
            ms_log (1, "Skipping (selection) record for %s (%d bytes) starting at offset %" PRId64 "\n",
                    (*ppmsr)->sid, (*ppmsr)->reclen, msfp->filepos);
          }

          /* Skip record length bytes, update reading offset and file position */
          msfp->readoffset += (*ppmsr)->reclen;
          msfp->filepos += (*ppmsr)->reclen;
        }
        else
        {
          /* Unpack data samples if this has been deferred */
          if (!(pflags & MSF_UNPACKDATA) && (flags & MSF_UNPACKDATA) && (*ppmsr)->samplecnt > 0)
          {
            if (msr3_unpack_data ((*ppmsr), verbose) != (*ppmsr)->samplecnt)
            {
              ms_log (2, "Cannot unpack data samples for record at byte offset %" PRId64 ": %s\n",
                      msfp->filepos, msfile);

              retcode = MS_NOERROR;
              break;
            }
          }

          if (verbose > 1)
            ms_log (1, "Read record length of %d bytes\n", (*ppmsr)->reclen);

          /* Test if this is the last record if file size is known (not a pipe) */
          if (last && msfp->filesize)
            if ((msfp->filesize - (msfp->filepos + (*ppmsr)->reclen)) < MINRECLEN)
              *last = 1;

          /* Return file position for this record */
          if (fpos)
            *fpos = msfp->filepos;

          /* Update reading offset, file position and record count */
          msfp->readoffset += (*ppmsr)->reclen;
          msfp->filepos += (*ppmsr)->reclen;
          msfp->recordcount++;

          retcode = MS_NOERROR;
          break;
        }
      }
      else if (parseval < 0)
      {
        /* Skip non-data if requested */
        if (flags & MSF_SKIPNOTDATA)
        {
          if (verbose > 1)
          {
            ms_log (1, "Skipped %d bytes of non-data record at byte offset %" PRId64 "\n",
                    SKIPLEN, msfp->filepos);
          }

          /* Skip SKIPLEN bytes, update reading offset and file position */
          msfp->readoffset += SKIPLEN;
          msfp->filepos += SKIPLEN;
        }
        /* Parsing errors */
        else
        {
          ms_log (2, "Cannot detect record at byte offset %" PRId64 ": %s\n",
                  msfp->filepos, msfile);

          retcode = parseval;
          break;
        }
      }
      else /* parseval > 0 (found record but need more data) */
      {
        /* Check for parse hints that are larger than MAXRECLEN */
        if ((MSFPBUFLEN (msfp) + parseval) > MAXRECLEN)
        {
          if (flags & MSF_SKIPNOTDATA)
          {
            /* Skip SKIPLEN bytes, update reading offset and file position */
            msfp->readoffset += SKIPLEN;
            msfp->filepos += SKIPLEN;
          }
          else
          {
            retcode = MS_OUTOFRANGE;
            break;
          }
        }
        /* End of file check */
        else if (feof (msfp->fp))
        {
          if (verbose)
          {
            if (msfp->filesize)
              ms_log (1, "Truncated record at byte offset %" PRId64 ", filesize %" PRId64 ": %s\n",
                      msfp->filepos, msfp->filesize, msfile);
            else
              ms_log (1, "Truncated record at byte offset %" PRId64 "\n",
                      msfp->filepos);
          }

          retcode = MS_ENDOFFILE;
          break;
        }
      }
    } /* End of record detection */

    /* Finished when within MINRECLEN from EOF and buffer less than MINRECLEN */
    if ((msfp->filesize - msfp->filepos) < MINRECLEN && MSFPBUFLEN (msfp) < MINRECLEN)
    {
      if (msfp->recordcount == 0)
      {
        if (verbose > 0)
          ms_log (2, "%s: No data records read, not SEED?\n", msfile);
        retcode = MS_NOTSEED;
      }
      else
      {
        retcode = MS_ENDOFFILE;
      }

      break;
    }
  } /* End of reading, record detection and parsing loop */

  /* Cleanup target MS3Record if returning an error */
  if (retcode != MS_NOERROR)
  {
    msr3_free (ppmsr);
  }

  return retcode;
} /* End of ms3_readmsr_selection() */

/****************************************************************/ /**
 * @brief Read miniSEED from a file into a trace list
 *
 * This is a simple wrapper for ms3_readtracelist_selection() that
 * uses no selections.
 *
 * See ms3_readtracelist_selection() for a further description of
 * arguments.
 *
 * @returns Return value from ms3_readtracelist_selection()
 *
 * \sa @ref trace-list
 *********************************************************************/
int
ms3_readtracelist (MS3TraceList **ppmstl, const char *msfile,
                   MS3Tolerance *tolerance, int8_t splitversion,
                   uint32_t flags, int8_t verbose)
{
  return ms3_readtracelist_selection (ppmstl, msfile, tolerance, NULL,
                                      splitversion, flags, verbose);
} /* End of ms3_readtracelist() */

/****************************************************************/ /**
 * @brief Read miniSEED from a file into a trace list, with time range
 * selection
 *
 * This is a wrapper for ms3_readtraces_selection() that creates a
 * simple selection for a specified time window.
 *
 * See ms3_readtracelist_selection() for a further description of
 * arguments.
 *
 * @returns Return value from ms3_readtracelist_selection()
 *
 * \sa @ref trace-list
 *********************************************************************/
int
ms3_readtracelist_timewin (MS3TraceList **ppmstl, const char *msfile,
                           MS3Tolerance *tolerance,
                           nstime_t starttime, nstime_t endtime,
                           int8_t splitversion, uint32_t flags, int8_t verbose)
{
  MS3Selections selection;
  MS3SelectTime selecttime;

  selection.sidpattern[0] = '*';
  selection.sidpattern[1] = '\0';
  selection.timewindows   = &selecttime;
  selection.next          = NULL;

  selecttime.starttime = starttime;
  selecttime.endtime   = endtime;
  selecttime.next      = NULL;

  return ms3_readtracelist_selection (ppmstl, msfile, tolerance,
                                      &selection, splitversion, flags,
                                      verbose);
} /* End of ms3_readtracelist_timewin() */

/****************************************************************/ /**
 * @brief Read miniSEED from a file into a trace list, with selection
 * filtering
 *
 * This routine will open and read all miniSEED records in specified
 * file and populate a ::MS3TraceList, allocating this struture if
 * needed.  This routine is thread safe.
 *
 * If \a selections is not NULL, the ::MS3Selections will be used to
 * limit which records are added to the trace list.  Any data not
 * matching the selections will be skipped.
 *
 * As this routine reads miniSEED records it attempts to construct
 * continuous time series, merging segments when possible.  See
 * mstl3_addmsr() for details of \a tolerance.
 *
 * The \a splitversion flag controls whether data are grouped
 * according to data publication version (or quality for miniSEED
 * 2.x).  See mstl3_addmsr() for full details.
 *
 * If the ::MSF_RECORDLIST flag is set in \a flags, a ::MS3RecordList
 * will be built for each ::MS3TraceSeg.  The ::MS3RecordPtr entries
 * contain the location of the data record, bit flags, extra headers, etc.
 *
 * @param[out] ppmstl Pointer-to-pointer to a ::MS3TraceList to populate
 * @param[in] msfile File to read
 * @param[in] tolerance Tolerance function pointers as ::MS3Tolerance
 * @param[in] selections Pointer to ::MS3Selections for limiting data
 * @param[in] splitversion Flag to control splitting of version/quality
 * @param[in] flags Flags to control reading, see ms3_readmsr_selection()
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns ::MS_NOERROR and populates an ::MS3TraceList struct at *ppmstl
 * on success, otherwise returns a (negative) libmseed error code.
 *
 * \sa @ref trace-list
 * \sa @ref data-selections
 *********************************************************************/
int
ms3_readtracelist_selection (MS3TraceList **ppmstl, const char *msfile,
                             MS3Tolerance *tolerance, MS3Selections *selections,
                             int8_t splitversion, uint32_t flags, int8_t verbose)
{
  MS3Record *msr     = NULL;
  MS3FileParam *msfp = NULL;
  MS3TraceSeg *seg   = NULL;
  MS3RecordPtr *recordptr = NULL;
  uint32_t dataoffset;
  uint32_t datasize;
  int64_t fpos;
  int retcode;

  if (!ppmstl)
    return MS_GENERROR;

  /* Initialize MS3TraceList if needed */
  if (!*ppmstl)
  {
    *ppmstl = mstl3_init (*ppmstl);

    if (!*ppmstl)
      return MS_GENERROR;
  }

  /* Loop over the input file and add each record to trace list */
  while ((retcode = ms3_readmsr_selection (&msfp, &msr, msfile, &fpos, NULL,
                                           flags, selections, verbose)) == MS_NOERROR)
  {
    seg = mstl3_addmsr_recordptr (*ppmstl, msr, (flags & MSF_RECORDLIST) ? &recordptr : NULL,
                                  splitversion, 1, flags, tolerance);

    if (seg == NULL)
    {
      ms_log (2, "%s(%s) Cannot add record to trace list\n", __func__, msr->sid);

      retcode = MS_GENERROR;
      break;
    }

    /* Populate remaining fields of record pointer */
    if (recordptr)
    {
      /* Determine offset to data and length of data payload */
      if (msr3_data_bounds (msr, &dataoffset, &datasize))
      {
        retcode = MS_GENERROR;
        break;
      }

      recordptr->bufferptr  = NULL;
      recordptr->fileptr    = NULL;
      recordptr->filename   = msfile;
      recordptr->fileoffset = fpos;
      recordptr->dataoffset = dataoffset;
      recordptr->prvtptr    = NULL;
    }
  }

  /* Reset return code to MS_NOERROR on successful read by ms_readmsr_selection() */
  if (retcode == MS_ENDOFFILE)
    retcode = MS_NOERROR;

  ms3_readmsr_selection (&msfp, &msr, NULL, NULL, NULL, 0, NULL, 0);

  return retcode;
} /* End of ms3_readtracelist_selection() */

/*********************************************************************
 *
 * A wrapper for fread() that handles EOF and error conditions.
 *
 * Returns the return value from fread.
 *********************************************************************/
static int
ms_fread (char *buf, int size, int num, FILE *stream)
{
  int read = 0;

  read = (int)fread (buf, size, num, stream);

  if (read <= 0 && size && num)
  {
    if (ferror (stream))
      ms_log (2, "%s(): Cannot read input file\n", __func__);

    else if (!feof (stream))
      ms_log (2, "%s(): Unknown return from fread()\n", __func__);
  }

  return read;
} /* End of ms_fread() */

/***************************************************************************
 *
 * Internal record handler.  The handler data should be a pointer to
 * an open file descriptor to which records will be written.
 *
 ***************************************************************************/
static void
ms_record_handler_int (char *record, int reclen, void *ofp)
{
  if (fwrite (record, reclen, 1, (FILE *)ofp) != 1)
  {
    ms_log (2, "Error writing to output file\n");
  }
} /* End of ms_record_handler_int() */

/**********************************************************************/ /**
 * @brief Write miniSEED from an ::MS3Record container to a file
 *
 * Pack ::MS3Record data into miniSEED record(s) by calling
 * msr3_pack() and write to a specified file.  The ::MS3Record
 * container is used as a template for record(s) written to the file.
 *
 * The \a overwrite flag controls whether a existing file is
 * overwritten or not.  If true (non-zero) any existing file will be
 * replaced.  If false (zero) new records will be appended to an
 * existing file.  In either case, new files will be created if they
 * do not yet exist.
 *
 * @param[in,out] msr ::MS3Record containing data to write
 * @param[in] msfile File for output records
 * @param[in] overwrite Flag to control overwriting versus appending
 * @param[in] flags Flags controlling data packing, see msr3_pack()
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns the number of records written on success and -1 on error.
 *
 * \sa msr3_pack()
 ***************************************************************************/
int64_t
msr3_writemseed (MS3Record *msr, const char *msfile, int8_t overwrite,
                 uint32_t flags, int8_t verbose)
{
  FILE *ofp;
  const char *perms     = (overwrite) ? "wb" : "ab";
  int64_t packedrecords = 0;

  if (!msr || !msfile)
    return -1;

  /* Open output file or use stdout */
  if (strcmp (msfile, "-") == 0)
  {
    ofp = stdout;
  }
  else if ((ofp = fopen (msfile, perms)) == NULL)
  {
    ms_log (1, "Cannot open output file %s: %s\n", msfile, strerror (errno));

    return -1;
  }

  /* Pack the MS3Record */
  packedrecords = msr3_pack (msr, &ms_record_handler_int, ofp, NULL, flags, verbose - 1);

  if (packedrecords < 0)
  {
    ms_log (1, "Cannot write miniSEED for %s\n", msr->sid);
  }

  /* Close file and return record count */
  fclose (ofp);

  return (packedrecords >= 0) ? packedrecords : -1;
} /* End of msr3_writemseed() */

/**********************************************************************/ /**
 * @brief Write miniSEED from an ::MS3TraceList container to a file
 *
 * Pack ::MS3TraceList data into miniSEED record(s) by calling
 * mstl3_pack() and write to a specified file.
 *
 * The \a overwrite flag controls whether a existing file is
 * overwritten or not.  If true (non-zero) any existing file will be
 * replaced.  If false (zero) new records will be appended to an
 * existing file.  In either case, new files will be created if they
 * do not yet exist.
 *
 * @param[in,out] mstl ::MS3TraceList containing data to write
 * @param[in] msfile File for output records
 * @param[in] overwrite Flag to control overwriting versus appending
 * @param[in] maxreclen The maximum record length to create
 * @param[in] encoding encoding Encoding for data samples, see msr3_pack()
 * @param[in] flags Flags controlling data packing, see mstl3_pack() and msr3_pack()
 * @param[in] verbose Controls verbosity, 0 means no diagnostic output
 *
 * @returns the number of records written on success and -1 on error.
 *
 * \sa mstl3_pack()
 * \sa msr3_pack()
 ***************************************************************************/
int64_t
mstl3_writemseed (MS3TraceList *mstl, const char *msfile, int8_t overwrite,
                  int maxreclen, int8_t encoding, uint32_t flags, int8_t verbose)
{
  FILE *ofp;
  const char *perms     = (overwrite) ? "wb" : "ab";
  int64_t packedrecords = 0;

  if (!mstl || !msfile)
    return -1;

  /* Open output file or use stdout */
  if (strcmp (msfile, "-") == 0)
  {
    ofp = stdout;
  }
  else if ((ofp = fopen (msfile, perms)) == NULL)
  {
    ms_log (1, "Cannot open output file %s: %s\n", msfile, strerror (errno));

    return -1;
  }

  /* Do not modify the trace list during packing */
  flags |= MSF_MAINTAINMSTL;

  /* Pack all data */
  flags |= MSF_FLUSHDATA;

  packedrecords = mstl3_pack (mstl, &ms_record_handler_int, ofp, maxreclen,
                              encoding, NULL, flags, verbose, NULL);

  if (packedrecords < 0)
  {
    ms_log (1, "Cannot write miniSEED for trace list\n");
  }

  /* Close file and return record count */
  fclose (ofp);

  return (packedrecords >= 0) ? packedrecords : -1;
} /* End of mstl3_writemseed() */
