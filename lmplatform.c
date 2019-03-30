/***************************************************************************
 * Platform portability routines.
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
