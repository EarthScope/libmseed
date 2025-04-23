/***************************************************************************
 * A program illustrating mapping source identifiers and SEED codes
 *
 * This file is part of the miniSEED Library.
 *
 * Copyright (c) 2025 Chad Trabant, EarthScope Data Services
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

#include <libmseed.h>

int
map_sid (const char *original_sid)
{
  char sid[LM_SIDLEN];
  char network[11];
  char station[11];
  char location[11];
  char channel[31];
  int rv;

  /* Parse network, station, location and channel from SID */
  rv = ms_sid2nslc_n (original_sid, network, sizeof (network), station, sizeof (station),
                      location, sizeof (location), channel, sizeof (channel));

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
map_nslc (const char *original_network, const char *original_station,
          const char *original_location, const char *original_channel)
{
  char sid[LM_SIDLEN];
  char network[11];
  char station[11];
  char location[11];
  char channel[31];
  int rv;

  /* Construct SID from network, station, location and channel */
  rv = ms_nslc2sid (sid, sizeof (sid), 0,
                    original_network, original_station,
                    original_location, original_channel);

  if (rv <= 0)
  {
    printf ("Error returned ms_nslc2sid()\n");
    return -1;
  }

  /* Parse network, station, location and channel from SID */
  rv = ms_sid2nslc_n (sid, network, sizeof (network), station, sizeof (station),
                      location, sizeof (location), channel, sizeof (channel));

  if (rv)
  {
    printf ("Error returned ms_sid2nslc()\n");
    return -1;
  }

  printf ("Original network: '%s', station: '%s', location: '%s', channel: '%s'\n",
          original_network, original_station, original_location, original_channel);
  printf ("  SID: '%s'\n", sid);
  printf ("  network: '%s', station: '%s', location: '%s', channel: '%s'\n",
          network, station, location, channel);

  return 0;
}

int
main (int argc, char **argv)
{
  /* A single argument is an FDSN SourceID */
  if (argc == 2)
  {
    if (map_sid (argv[1]))
    {
      printf ("Error with map_sid()\n");
      return 1;
    }
  }
  /* Four arguments are network, station, location, and channel */
  else if (argc == 5)
  {
    /* Four arguments are network, station, location and channel */
    if (map_nslc (argv[1], argv[2], argv[3], argv[4]))
    {
      printf ("Error with map_nslc()\n");
      return 1;
    }
  }
  else
  {
    printf ("Usage: %s <SID> | <network> <station> <location> <channel>\n", argv[0]);
    printf ("  <SID> is a FDSN SourceID, e.g. 'FDSN:NET_STA_LOC_C_H_N'\n");
    printf ("  <network> <station> <location> <channel> are SEED codes\n");
    return 1;
  }

  return 0;
}
