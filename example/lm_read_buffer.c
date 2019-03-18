/***************************************************************************
 * lm_read_buffer.c
 *
 * A libmseed snippet program for reading miniSEED from buffers
 ***************************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include <libmseed.h>

int
main (int argc, char **argv)
{
  struct stat sb = {0};
  int64_t records = 0;
  FILE *fh;

  MS3TraceList *mstl = NULL;
  char *buffer = NULL;
  uint64_t bufferlength = 0;
  double timetol = -1;
  double sampratetol = -1;
  int8_t splitversion = 0;
  uint32_t flags = 0;
  int8_t verbose = 0;

  if (argc != 2)
  {
    ms_log (2, "%s requires a single file name argument\n",argv[0]);
    return -1;
  }

  /* Read specified file into buffer */
  if (!(fh = fopen(argv[1], "rb")))
  {
    ms_log (2, "Error opening %s: %s\n", argv[1], strerror(errno));
    return -1;
  }
  if (fstat (fileno(fh), &sb))
  {
    ms_log (2, "Error stating %s: %s\n", argv[1], strerror(errno));
    return -1;
  }
  if (!(buffer = (char *)malloc(sb.st_size)))
  {
    ms_log (2, "Error allocating buffer of %lld bytes\n", (long long int)sb.st_size);
    return -1;
  }
  if (fread(buffer, sb.st_size, 1, fh) != 1)
  {
    ms_log (2, "Error reading file\n");
    return -1;
  }

  fclose(fh);

  bufferlength = sb.st_size;

  /* Set bit flags to validate CRC and unpack data samples */
  flags |= MSF_VALIDATECRC;
  flags |= MSF_UNPACKDATA;

  mstl = mstl3_init (NULL);

  if (!mstl)
  {
    ms_log (2, "Error allocating MS3TraceList\n");
    return -1;
  }

  /* Read all miniSEED in buffer, accumulate in MS3TraceList */
  records = mstl3_readbuffer (&mstl, buffer, bufferlength, timetol,
                              sampratetol, splitversion, flags, verbose);

  if (records < 0)
  {
    ms_log (2, "Problem reading miniSEED from buffer: %s\n", ms_errorstr(records));
  }

  /* Print summary */
  mstl3_printtracelist (mstl, ISOMONTHDAY, 1, 1);

  ms_log (1, "Total records: %" PRId64 "\n", records);

  /* Make sure everything is cleaned up */
  if (mstl)
    mstl3_free (&mstl, 0);

  free (buffer);

  return 0;
}
