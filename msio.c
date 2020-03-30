/***************************************************************************
 * I/O handling routines, for files and URLs.
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2020 Chad Trabant, IRIS Data Management Center
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

/* Define _LARGEFILE_SOURCE to get ftello/fseeko on some systems (Linux) */
#define _LARGEFILE_SOURCE 1

#include "msio.h"

/* Include libcurl library header if URL supported is requested */
#if defined(LIBMSEED_URL)
#include <curl/curl.h>

/* Control for enabling debugging information */
int libmseed_urldebug = -1;

/* A global libcurl easy handle for configuration options */
CURL *gCURLeasy = NULL;

/* Callback parameters */
struct callback_parameters
{
  char *buffer;
  size_t size;
  int is_paused;
};

/*********************************************************************
 * Callback fired when recv'ing data using libcurl.
 *
 * The destination buffer pointer and size in the callback parameters
 * are adjusted as data are added.
 *
 * Returns number of bytes added to the destination buffer.
 *********************************************************************/
static size_t
recv_callback (char *buffer, size_t size, size_t num, void *userdata)
{
  struct callback_parameters *db = (struct callback_parameters *)userdata;

  size *= num;

  /* Pause connection if passed data does not fit into destination buffer */
  if (size > db->size)
  {
    db->is_paused = 1;
    return CURL_WRITEFUNC_PAUSE;
  }
  /* Otherwise, copy data to destination buffer */
  else
  {
    memcpy (db->buffer, buffer, size);
    db->buffer += size;
    db->size -= size;
  }

  return size;
}
#endif /* defined(LIBMSEED_URL) */


/***************************************************************************
 * ms_fopen:
 *
 * Determine if requested path is a regular file or a URL and open or
 * initialize as appropriate.
 *
 * The 'mode' argument is only for file-system paths and ignored for
 * URLs.  If 'mode' is set to NULL, use default 'rb' mode.
 *
 * Return 0 on success and non-zero on error.
 ***************************************************************************/
int
ms_fopen (LMIO *io, const char *path, const char *mode)
{
  int knownfile = 0;

  if (!io || !path)
    return -1;

  if (!mode)
    mode = "rb";

  /* Treat "file://" specifications as local files by removing the scheme */
  if (!strncasecmp (path, "file://", 7))
  {
    path += 7;
    knownfile = 1;
  }

  /* Test for URL scheme via "://" */
  if (!knownfile && strstr (path, "://"))
  {
#if !defined(LIBMSEED_URL)
    ms_log (2, "%s(): URL support not included in library for %s\n", __func__, path);
    return -1;
#else
    /* Check for debugging environment variable */
    if (libmseed_urldebug < 0)
    {
      if (getenv ("URL_DEBUG"))
        libmseed_urldebug = 1;
      else
        libmseed_urldebug = 0;
    }

    /* Configure the libcurl easy handle, duplicate global options if present */
    io->handle = (gCURLeasy) ? curl_easy_duphandle (gCURLeasy) : curl_easy_init ();

    if (io->handle == NULL)
      return -1;

    /* Debug/development */
    if (libmseed_urldebug && curl_easy_setopt (io->handle, CURLOPT_VERBOSE, 1) != CURLE_OK)
      return -1;

    /* Set URL */
    if (curl_easy_setopt (io->handle, CURLOPT_URL, path) != CURLE_OK)
      return -1;

    /* Set User-Agent header */
    if (curl_easy_setopt (io->handle, CURLOPT_USERAGENT,
                          "libmseed/" LIBMSEED_VERSION " libcurl/" LIBCURL_VERSION) != CURLE_OK)
      return -1;

    /* Disable signals */
    if (curl_easy_setopt (io->handle, CURLOPT_NOSIGNAL, 1) != CURLE_OK)
      return -1;

    /* Configure write callback for recv'ed data */
    if (curl_easy_setopt (io->handle, CURLOPT_WRITEFUNCTION, recv_callback) != CURLE_OK)
      return -1;

    /* Configure the libcurl multi handle, for use with the asynchronous interface */
    if ((io->handle2 = curl_multi_init ()) == NULL)
      return -1;

    if (curl_multi_add_handle (io->handle2, io->handle) != CURLM_OK)
      return -1;

    io->still_running = -1; /* -1 == opened but not yet started */
    io->type          = LMIO_URL;
#endif /* defined(LIBMSEED_URL) */
  }
  else
  {
    if ((io->handle = fopen (path, mode)) == NULL)
    {
      return -1;
    }

    io->type = LMIO_FILE;
  }

  return 0;
}  /* End of ms_fopen() */

/*********************************************************************
 * ms_fclose:
 *
 * Close an IO handle.
 *
 * Returns 0 on success and negative value on error.
 *********************************************************************/
int
ms_fclose (LMIO *io)
{
  int rv;

  if (!io)
    return -1;

  if (io->handle == NULL || io->type == LMIO_NULL)
    return 0;

  if (io->type == LMIO_FILE)
  {
    rv = fclose (io->handle);

    if (rv)
      return -1;
  }
  else if (io->type == LMIO_URL)
  {
#if !defined(LIBMSEED_URL)
    ms_log (2, "%s(): URL support not included in library\n", __func__);
    return -1;
#else
    curl_multi_remove_handle (io->handle2, io->handle);
    curl_easy_cleanup (io->handle);
    curl_multi_cleanup (io->handle2);
#endif
  }

  io->type = LMIO_NULL;
  io->handle = NULL;
  io->handle2 = NULL;

  return 0;
} /* End of ms_fclose() */

/*********************************************************************
 * ms_fread:
 *
 * Read data from the identified IO handle into the specified buffer.
 * Up to the requested 'size' bytes are read.
 *
 * For URL support, with defined(LIBMSEED_URL), the destination
 * receive buffer MUST be at least as big as the curl receive buffer
 * (CURLOPT_BUFFERSIZE, which defaults to CURL_MAX_WRITE_SIZE of 16kB)
 * or the maximum size of a retrieved object if less than
 * CURL_MAX_WRITE_SIZE.  The caller must ensure this.
 *
 * Returns the number of bytes read on success and a negative value on
 * error.
 *********************************************************************/
size_t
ms_fread (LMIO *io, void *buffer, size_t size)
{
  size_t read = 0;

  if (!io || !buffer)
    return -1;

  if (size == 0)
    return 0;

  /* Read from regular file stream */
  if (io->type == LMIO_FILE)
  {
    read = fread (buffer, 1, size, io->handle);

    if (read <= 0)
    {
      /* Differentiate on returned errors, EOF is not a logged error */
      if (ferror (io->handle))
        ms_log (2, "%s(): Cannot read input file\n", __func__);

      else if (!feof (io->handle))
        ms_log (2, "%s(): Unknown return from fread()\n", __func__);
    }
  }
  /* Read from URL stream */
  else if (io->type == LMIO_URL)
  {
#if !defined(LIBMSEED_URL)
    ms_log (2, "%s(): URL support not included in library\n", __func__);
    return -1;
#else
    struct callback_parameters cbp;
    struct timeval timeout;
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd       = -1;
    long curl_timeo = -1;
    int rc;

    if (!io->still_running)
      return 0;

    /* Set up destination buffer in write callback parameters */
    cbp.buffer = buffer;
    cbp.size   = size;
    if (curl_easy_setopt (io->handle, CURLOPT_WRITEDATA, (void *)&cbp) != CURLE_OK)
      return -1;

    /* Unpause connection */
    curl_easy_pause (io->handle, CURLPAUSE_CONT);
    cbp.is_paused = 0;

    /* Receive data while connection running, destination space available
     * and connection is not paused. */
    do
    {
      /* Default timeout for read failure */
      timeout.tv_sec  = 60;
      timeout.tv_usec = 0;

      curl_multi_timeout (io->handle2, &curl_timeo);

      /* Tailor timeout based on maximum suggested by libcurl */
      if (curl_timeo >= 0)
      {
        timeout.tv_sec = curl_timeo / 1000;
        if (timeout.tv_sec > 1)
          timeout.tv_sec = 1;
        else
          timeout.tv_usec = (curl_timeo % 1000) * 1000;
      }

      FD_ZERO (&fdread);
      FD_ZERO (&fdwrite);
      FD_ZERO (&fdexcep);

      /* Extract descriptors from the multi-handle */
      if (curl_multi_fdset (io->handle2, &fdread, &fdwrite, &fdexcep, &maxfd) != CURLM_OK)
      {
        ms_log (2, "%s(): Error with curl_multi_fdset()\n", __func__);
        return -1;
      }

      /* libcurl/system needs time to work, sleep 100 milliseconds */
      if (maxfd == -1)
      {
        lmp_nanosleep (100000000);
        rc = 0;
      }
      else
      {
        rc = select (maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
      }

      /* Receive data */
      if (rc >= 0)
      {
        curl_multi_perform (io->handle2, &io->still_running);
      }
    } while (io->still_running > 0 && cbp.size && !cbp.is_paused);

    read = size - cbp.size;

#endif /* defined(LIBMSEED_URL) */
  }

  return read;
} /* End of ms_fread() */

/*********************************************************************
 * ms_feof:
 *
 * Test if end-of-stream.
 *
 * Returns 1 when stream is at end, 0 otherwise.
 *********************************************************************/
int
ms_feof (LMIO *io)
{
  if (!io)
    return 0;

  if (io->handle == NULL || io->type == LMIO_NULL)
    return 0;

  if (io->type == LMIO_FILE)
  {
    if (feof (io->handle))
      return 1;
  }
  else if (io->type == LMIO_URL)
  {
#if !defined(LIBMSEED_URL)
    ms_log (2, "%s(): URL support not included in library\n", __func__);
    return -1;
#else
    /* The still_running flag is only changed by curl_multi_perform()
     * and indicates current "transfers in progress".  Presumably no data
     * are in internal libcurl buffers either. */
    if (!io->still_running)
      return 1;
#endif
  }

  return 0;
} /* End of ms_feof() */

/*********************************************************************
 * ms_fseek:
 *
 * Set start, and potentially end, position in stream.
 *
 * Returns 0 on success and non-zero otherwise.
 *********************************************************************/
int
ms_fseek (LMIO *io, int64_t startoffset, int64_t endoffset)
{
  if (!io)
    return 0;

  if (io->handle == NULL || io->type == LMIO_NULL)
    return 0;

  /* If neither offset is set, nothing to do */
  if (startoffset <= 0 && endoffset <= 0)
    return 0;

  if (io->type == LMIO_FILE)
  {
    if (startoffset > 0)
    {
      return lmp_fseek64 (io->handle, startoffset, SEEK_SET);
    }
  }
  else if (io->type == LMIO_URL)
  {
#if !defined(LIBMSEED_URL)
    ms_log (2, "%s(): URL support not included in library\n", __func__);
    return -1;
#else
    char startstr[21] = {0};
    char endstr[21] = {0};
    char rangestr[42];

    if (io->still_running > 0)
    {
      ms_log (2, "%s(): Cannot set URL byte ranging on running connection\n", __func__);
      return -1;
    }

    /* Build Range header value, either of the range ends are optional */
    if (startoffset > 0)
      snprintf (startstr, sizeof(startstr), "%" PRId64, startoffset);
    if (endoffset > 0)
      snprintf (endstr, sizeof(endstr), "%" PRId64, endoffset);

    snprintf (rangestr, sizeof(rangestr), "%s-%s", startstr, endstr);

    /* Set Range header */
    if (curl_easy_setopt(io->handle, CURLOPT_RANGE, rangestr) != CURLE_OK)
    {
      return -1;
    }
#endif
  }

  return 0;
} /* End of ms_fseek() */


/***************************************************************************
 * lmp_ftell64:
 *
 * Return the current file position for the specified descriptor using
 * the system's closest match to the POSIX ftello().
 ***************************************************************************/
int64_t
lmp_ftell64 (FILE *stream)
{
#if defined(LMP_WIN)
  return (int64_t)_ftelli64 (stream);
#else
  return (int64_t)ftello (stream);
#endif
} /* End of lmp_ftell64() */


/***************************************************************************
 * lmp_fseek64:
 *
 * Seek to a specific file position for the specified descriptor using
 * the system's closest match to the POSIX fseeko().
 ***************************************************************************/
int
lmp_fseek64 (FILE *stream, int64_t offset, int whence)
{
#if defined(LMP_WIN)
  return (int)_fseeki64 (stream, offset, whence);
#else
  return (int)fseeko (stream, offset, whence);
#endif
} /* End of lmp_fseeko() */


/***************************************************************************
 * @brief Sleep for a specified number of nanoseconds
 *
 * Sleep for a given number of nanoseconds.  Under WIN use SleepEx()
 * and is limited to millisecond resolution.  For all others use the
 * POSIX.4 nanosleep(), which can be interrupted by signals.
 *
 * @param nanoseconds Nanoseconds to sleep
 *
 * @return On non-WIN: the remaining nanoseconds are returned if the
 * requested interval is interrupted.
 ***************************************************************************/
uint64_t
lmp_nanosleep (uint64_t nanoseconds)
{
#if defined(LMP_WIN)

  /* SleepEx is limited to milliseconds */
  SleepEx ((DWORD) (nanoseconds / 1e6), 1);

  return 0;
#else

  struct timespec treq, trem;

  treq.tv_sec  = (time_t) (nanoseconds / 1e9);
  treq.tv_nsec = (long)(nanoseconds - (uint64_t)treq.tv_sec * 1e9);

  nanosleep (&treq, &trem);

  return trem.tv_sec * 1e9 + trem.tv_nsec;

#endif
} /* End of lmp_nanosleep() */
