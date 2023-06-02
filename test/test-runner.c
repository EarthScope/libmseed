/* Main entry point for tests.
 *********************************************************************/

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include <libmseed.h>
#include <tau/tau.h>


TAU_MAIN() // sets up Tau


/* Function to compare contents of two files.
 *********************************************************************/
#define MAX_FILE_COMPARE_BYTES 10000000
int
cmpfiles (char *fileA, char *fileB)
{
  struct stat statA = {0};
  struct stat statB = {0};
  FILE *fhA = NULL;
  FILE *fhB = NULL;
  uint8_t *bufferA = NULL;
  uint8_t *bufferB = NULL;
  int rv = -1;

  if (!fileA || !fileB)
    return rv;

  /* Open and stat specified files */
  if (!(fhA = fopen (fileA, "rb"))) /* Open file A */
  {
    fprintf (stderr, "%s() Error opening %s: %s\n", __func__, fileA, strerror (errno));
  }
  else if (fstat (fileno (fhA), &statA)) /* Stat file A */
  {
    fprintf (stderr, "%s() Error stat'ing %s: %s\n", __func__, fileA, strerror (errno));
  }
  else if (!(fhB = fopen (fileB, "rb"))) /* Open file A */
  {
    fprintf (stderr, "%s() Error opening %s: %s\n", __func__, fileB, strerror (errno));
  }
  else if (fstat (fileno (fhB), &statB)) /* Stat file B */
  {
    fprintf (stderr, "%s() Error stat'ing %s: %s\n", __func__, fileB, strerror (errno));
  }
  else if (statA.st_size != statB.st_size) /* Compare file sizes */
  {
    rv = -1;
  }
  else if (statA.st_size > MAX_FILE_COMPARE_BYTES) /* Refuse to compare large files */
  {
    fprintf (stderr, "%s() Files are too large to compare, max %d bytes\n", __func__, MAX_FILE_COMPARE_BYTES);
  }
  else if ((bufferA = (uint8_t *)malloc (statA.st_size)) == NULL) /* Allocate file A buffer */
  {
    fprintf (stderr, "%s() Error allocating memory\n", __func__);
  }
  else if ((bufferB = (uint8_t *)malloc (statB.st_size)) == NULL) /* Allocate file B buffer */
  {
    fprintf (stderr, "%s() Error allocating memory\n", __func__);
  }
  else if (fread (bufferA, statA.st_size, 1, fhA) != 1) /* Read file A into buffer */
  {
    fprintf (stderr, "%s() Error reading (%lld bytes) file: %s\n", __func__, (long long int) statA.st_size, fileA);
  }
  else if (fread (bufferB, statA.st_size, 1, fhB) != 1) /* Read file B into buffer */
  {
    fprintf (stderr, "%s() Error reading (%lld bytes) file: %s\n", __func__, (long long int) statB.st_size, fileB);
  }
  else /* Compare file contents */
  {
    rv = memcmp (bufferA, bufferB, statA.st_size);
  }

  /* Cleanup */
  if (fhA)
    fclose (fhA);
  if (fhB)
    fclose (fhB);

  if (bufferA)
    free (bufferA);
  if (bufferB)
    free (bufferB);

  return rv;
}

/* Function to compare arrays of int32_t.
 *********************************************************************/
int
cmpint32s (int32_t *arrayA, int32_t *arrayB, size_t length)
{
  int idx;

  for (idx = 0; idx < length; idx++)
  {
    if (arrayA[idx] != arrayB[idx])
    {
      return 1;
    }
  }

  return 0;
}

/* Function to compare arrays of floats.
 *********************************************************************/
int
cmpfloats (float *arrayA, float *arrayB, size_t length)
{
  int idx;

  for (idx = 0; idx < length; idx++)
  {
    if (arrayA[idx] != arrayB[idx])
    {
      return 1;
    }
  }

  return 0;
}

/* Function to compare arrays of doubles.
 *********************************************************************/
int
cmpdoubles (double *arrayA, double *arrayB, size_t length)
{
  int idx;

  for (idx = 0; idx < length; idx++)
  {
    if (arrayA[idx] != arrayB[idx])
    {
      return 1;
    }
  }

  return 0;
}