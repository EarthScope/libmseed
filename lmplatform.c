/***************************************************************************
 * lmplatform.c:
 *
 * Platform portability routines.
 *
 ***************************************************************************/

/* Define _LARGEFILE_SOURCE to get ftello/fseeko on some systems (Linux) */
#define _LARGEFILE_SOURCE 1

#include "libmseed.h"

/***************************************************************************
 * lmp_ftell64:
 *
 * Return the current file position for the specified descriptor using
 * the system's closest match to the POSIX ftello.
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
 * the system's closest match to the POSIX fseeko.
 ***************************************************************************/
int
lmp_fseek64 (FILE *stream, int64_t offset, int whence)
{
#if defined(LMP_WIN)
  return (int)fseek (stream, (int64_t)offset, whence);
#else
  return (int)fseeko (stream, offset, whence);
#endif
} /* End of lmp_fseeko() */
