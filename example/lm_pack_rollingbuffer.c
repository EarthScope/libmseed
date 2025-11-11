/***************************************************************************
 * A program for illustrating the use of a Trace List as an
 * intermediate, rolling buffer for production of data records.
 *
 * An input file of miniSEED is used as a convenient data source.
 * MS3Records can be constructed for any arbitrary data and follow the
 * same pattern of record generation.
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

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include <libmseed.h>

/***************************************************************************
 *
 * A simple record handler that parses and prints record details
 *
 ***************************************************************************/
void
record_handler (char *record, int reclen)
{
  MS3Record *msr = NULL;

  if (!msr3_parse (record, reclen, &msr, 0, 0))
  {
    msr3_print (msr, 0);
  }
  else
  {
    fprintf (stderr, "%s() Error parsing record\n", __func__);
  }

  msr3_free (&msr);
}

int
main (int argc, char **argv)
{
  MS3Record *msr = NULL;
  MS3TraceList *mstl = NULL;
  char *inputfile = NULL;
  uint32_t flags = 0;
  int8_t verbose = 0;
  int retcode;

  int reclen = 256; /* Desired maximum record length */
  uint8_t encoding = DE_STEIM2; /* Desired data encoding */

  MS3TraceListPacker *packer = NULL;
  char *record = NULL;
  int32_t reclen_generated = 0;
  int64_t packedsamples = 0;
  int recordcount = 0;
  int result;

  if (argc != 2)
  {
    ms_log (2, "Usage: %s <mseedfile>\n", argv[0]);
    return 1;
  }

  inputfile = argv[1];

  mstl = mstl3_init(NULL);
  if (mstl == NULL)
  {
    ms_log (2, "Cannot allocate memory\n");
    return 1;
  }

  /* Set bit flags to validate CRC and unpack data samples */
  flags |= MSF_VALIDATECRC;
  flags |= MSF_UNPACKDATA;

  /* Initialize packing state */
  packer = mstl3_pack_init (mstl, reclen, encoding, flags, verbose, NULL, 0);
  if (!packer)
  {
    ms_log (2, "Cannot initialize packing state\n");
    return 1;
  }

  /* Loop over the input file as a source of data */
  while ((retcode = ms3_readmsr (&msr, inputfile, flags, verbose)) == MS_NOERROR)
  {
    if (mstl3_addmsr (mstl, msr, 0, 1, flags, NULL) == NULL)
    {
      ms_log (2, "mstl3_addmsr() had problems\n");
      break;
    }

    /* Attempt to pack data in the Trace List buffer.
     * Only filled, or complete, records will be generated. */
    ms_log (0, "Calling mstl3_pack_next() to generate records\n");

    recordcount = 0;
    while ((result = mstl3_pack_next (packer, 0, &record, &reclen_generated)) == 1)
    {
      record_handler (record, reclen_generated);
      recordcount++;
    }

    if (result != 0)
    {
      ms_log (2, "mstl3_pack_next() returned an error: %d\n", result);
      break;
    }

    ms_log (0, "mstl3_pack_next() created %d records\n", recordcount);
  }

  if (retcode != MS_ENDOFFILE)
    ms_log (2, "Error reading %s: %s\n", inputfile, ms_errorstr (retcode));

  /* Final call to flush data buffers, adding MSF_FLUSHDATA flag */
  ms_log (0, "Calling mstl3_pack_next() with MSF_FLUSHDATA flag\n");

  /* Pack remaining data in the buffer with final call using MSF_FLUSHDATA flag */
  recordcount = 0;
  while ((result = mstl3_pack_next (packer, MSF_FLUSHDATA, &record, &reclen_generated)) == 1)
  {
    record_handler (record, reclen_generated);
    recordcount++;
  }

  if (result != 0)
  {
    ms_log (2, "mstl3_pack_next() returned an error: %d\n", result);
  }

  mstl3_pack_free (&packer, &packedsamples);

  ms_log (0, "Final mstl3_pack_next() created %d records for a total of %" PRId64 " samples\n", recordcount,
          packedsamples);

  /* Make sure everything is cleaned up */
  ms3_readmsr (&msr, NULL, flags, 0);

  if (msr)
    msr3_free (&msr);

  if (mstl)
    mstl3_free (&mstl, 0);

  return 0;
}
