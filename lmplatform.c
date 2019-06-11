/***************************************************************************
 * Platform portability routines.
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

/* Define _LARGEFILE_SOURCE to get ftello/fseeko on some systems (Linux) */
#define _LARGEFILE_SOURCE 1

#include "libmseed.h"

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
