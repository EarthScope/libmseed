/***************************************************************************
 * A program illustrating mapping source identifiers and SEED codes
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2019 Chad Trabant, IRIS Data Management Center
 *
 * The miniSEED Library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * The miniSEED Library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License (GNU-LGPL) for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software. If not, see
 * <https://www.gnu.org/licenses/>
 ***************************************************************************/

#include <libmseed.h>

int
map_sid (char *original_sid)
{
  char sid[LM_SIDLEN];
  char network[11];
  char station[11];
  char location[11];
  char channel[11];
  int rv;

  /* Parse network, station, location and channel from SID */
  rv = ms_sid2nslc (original_sid, network, station, location, channel);

  if (rv)
  {
    printf ("Error returned ms_sid2nslc()\n");
    return -1;
  }

  /* Construct SID from network, station, location and channel */
  rv = ms_nslc2sid (sid, sizeof(sid), 0, network, station, location, channel);

  if (rv <= 0)
  {
    printf ("Error returned ms_nslc2sid()\n");
    return -1;
  }

  printf ("Original SID: '%s'\n", original_sid);
  printf ("  network: '%s', station: '%s', location: '%s', channel: '%s'\n",
          network, station, location, channel);
  printf ("  SID: '%s'\n", sid);

  return 0;
}

int
main (int argc, char **argv)
{
  char *sid;
  int idx;

  sid = "XFDSN:NET_STA_LOC_C_H_N";

  if (map_sid (sid))
  {
    printf ("Error with map_sid()\n");
    return 1;
  }

  /* Map each value give on the command line */
  if (argc > 1)
  {
    for (idx = 1; idx < argc; idx++)
    {
      if (map_sid (argv[idx]))
      {
        printf ("Error with map_sid()\n");
        return 1;
      }
    }
  }

  return 0;
}
