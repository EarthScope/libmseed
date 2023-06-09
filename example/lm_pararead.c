/***************************************************************************
 * A simple example of using libmseed to read miniSEED in parallel
 * using re-entrant interfaces and POSIX threading.
 *
 * Windows is not supported.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2023 Chad Trabant, EarthScope Data Services
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

#include <libmseed.h>

/* File read test portability, possibly in the future */
#if defined(LMP_WIN)
  #define _access access
  #define R_OK    4       /* Test for read permission */
#else
  #include <pthread.h>
  #include <unistd.h>
#endif

#define VERSION "[libmseed " LIBMSEED_VERSION " example]"
#define PACKAGE "lm_pararead"

static int8_t verbose = 0;
static int8_t printrecs = 0;
static uint32_t readflags = 0;

/* File information structure */
typedef struct FileEntry
{
  char *filename;
  pthread_t tid;
  MS3TraceList *mstl;
  uint64_t recordcount;
  int result;
  struct FileEntry *next;
} FileEntry;

/* List of files to read */
static FileEntry *files = NULL;

/* Thread function to read miniSEED file */
static void *ReadMSFileThread (void *vfe)
{
  FileEntry *fe      = (FileEntry *)vfe;
  MS3FileParam *msfp = NULL;
  MS3Record *msr     = NULL;

  /* NOTE: This example is contrived for illustration.
   * The combination of ms3_readmsr_r() with mstl3_addmsr()
   * is only needed if you wish to do something for each record.
   * Otherwise, consider using ms3_readtracelist() if only
   * a MS3TraceList is desired.*/

  /* Loop over the input file record by record */
  while ((fe->result = ms3_readmsr_r (&msfp, &msr,
                                      fe->filename,
                                      readflags, verbose)) == MS_NOERROR)
  {
    fe->recordcount++;

    if (printrecs > 0)
      msr3_print (msr, printrecs - 1);

    /* Add record to trace list */
    if (mstl3_addmsr (fe->mstl, msr, 0, 1, 0, NULL) == NULL)
    {
      ms_log (2, "Error adding record to list\n");
      fe->result = 1;
    }
  }

  /* Make sure everything is cleaned up */
  ms3_readmsr_r (&msfp, &msr, NULL, 0, 0);

  return NULL;
}

int
main (int argc, char **argv)
{
  FileEntry *fe    = NULL;
  int idx;

  /* Simplistic argument parsing */
  for (idx = 1; idx < argc; idx++)
  {
    if (strncmp (argv[idx], "-v", 2) == 0)
      verbose += strspn (&argv[idx][1], "v");
    else if (strncmp (argv[idx], "-p", 2) == 0)
      printrecs += strspn (&argv[idx][1], "p");
    else
    {
      /* Add read-able files to file entry list */
      if (access(argv[idx], R_OK) == 0)
      {
        if ((fe = (FileEntry *)malloc(sizeof(FileEntry))) == NULL)
        {
          ms_log (2, "Error allocating memory");
          return -1;
        }

        fe->filename = argv[idx];
        fe->mstl = mstl3_init(NULL);
        fe->recordcount = 0;
        fe->result = 0;
        fe->next = files;

        files = fe;
      }
      else
      {
        ms_log (2, "Cannot find file: %s\n", argv[idx]);
        return -1;
      }
    }
  }

  /* Make sure input file(s) specified */
  if (!files)
  {
    ms_log (1, "No input file(s) specified\n\n");
    ms_log (1, "%s version %s\n\n", PACKAGE, VERSION);
    ms_log (1, "Read specified miniSEED files in parallel\n\n");
    ms_log (1, "Usage: %s [-p] [-v] file1 [file2 .. fileN]\n", PACKAGE);
    ms_log (1, "  -v  Be more verbose, multiple flags can be used\n");
    ms_log (1, "  -p  Print record details, multiple flags can be used\n\n");
    return 0;
  }

  /* Report the program version */
  if (verbose)
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);

  /* Add program name and version to User-Agent for URL-based requests */
  if (libmseed_url_support() && ms3_url_useragent(PACKAGE, VERSION))
    return -1;

  /* Set flag to validate CRCs when reading */
  readflags |= MSF_VALIDATECRC;

  /* Parse byte range from file/URL path name if present */
  readflags |= MSF_PNAMERANGE;

  /* Create a thread to read each file */
  for (fe = files; fe; fe = fe->next)
  {
    pthread_create (&(fe->tid), NULL, ReadMSFileThread, fe);
  }

  /* Wait for threads to finish */
  for (fe = files; fe; fe = fe->next)
  {
    pthread_join(fe->tid, NULL);
  }

  /* Report details for each file */
  for (fe = files; fe; fe = fe->next)
  {
    ms_log (0, "%s: records: %" PRIu64" result: %d\n",
            fe->filename, fe->recordcount, fe->result);

    if (fe->result == MS_NOERROR || fe->result == MS_ENDOFFILE)
      mstl3_printtracelist (fe->mstl, ISOMONTHDAY, 1, 1, 0);
  }

  return 0;
} /* End of main() */
