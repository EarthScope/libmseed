/***************************************************************************
 * ms_readleapsecondfile_test.c
 *
 * A test program for ms_readleapsecondfile
 *
 ***************************************************************************/

#include <errno.h>
#include <stdbool.h>
#include <libmseed.h>

#define VERSION "[libmseed " LIBMSEED_VERSION " example]"
#define PACKAGE "lmtestpack"

static flag verbose = 0;
static bool preallocate = false;

static char* leapsecondfile = NULL;
static char *outfile = NULL;

static int parameter_proc (int argcount, char **argvec);
static void print_stderr (char *message);
static void usage (void);


int
main (int argc, char **argv)
{
  FILE *ofp = NULL;
  /* Redirect libmseed logging facility to stderr for consistency */
  ms_loginit (print_stderr, NULL, print_stderr, NULL);

  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;

  /* Open output file or use stdout */
  if (strcmp (outfile, "-") == 0)
  {
    ofp = stdout;
  }
  else if ((ofp = fopen (outfile, "wb")) == NULL)
  {
    ms_log (1, "Cannot open output file %s: %s\n", outfile, strerror (errno));

    return -1;
  }

  if (preallocate == true)
  {
    ms_readleapsecondfile (leapsecondfile);
  }

  int count = ms_readleapsecondfile (leapsecondfile);
  fprintf (ofp, "%d\n", count);
  // Print leapsecond list
  LeapSecond *ls = leapsecondlist;
  while (ls != NULL)
  {
    fprintf (ofp, "%" PRIi64 " %" PRIi32"\n", ls->leapsecond, ls->TAIdelta);
    ls = ls->next;
  }

  if (ofp != stdout)
  {
    fclose (ofp);
  }

  return 0;
}

/***************************************************************************
 * parameter_proc:
 *
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  int optind;

  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
  {
    if (strcmp (argvec[optind], "-V") == 0)
    {
      ms_log (1, "%s version: %s\n", PACKAGE, VERSION);
      exit (0);
    }
    else if (strcmp (argvec[optind], "-h") == 0)
    {
      usage ();
      exit (0);
    }
    else if (strncmp (argvec[optind], "-v", 2) == 0)
    {
      verbose += strspn (&argvec[optind][1], "v");
    }
    else if (strcmp (argvec[optind], "-i") == 0)
    {
      leapsecondfile = argvec[++optind];
    }
    else if (strcmp (argvec[optind], "-p") == 0)
    {
      preallocate = true;
    }
    else if (strcmp (argvec[optind], "-o") == 0)
    {
      outfile = argvec[++optind];
    }
    else
    {
      ms_log (2, "Unknown option: %s\n", argvec[optind]);
      exit (1);
    }
  }

  /* Make sure an outfile was specified */
  if (!outfile)
  {
    ms_log (2, "No output file was specified\n\n");
    ms_log (1, "Try %s -h for usage\n", PACKAGE);
    exit (1);
  }

  /* Report the program version */
  if (verbose)
    ms_log (1, "%s version: %s\n", PACKAGE, VERSION);

  return 0;
} /* End of parameter_proc() */

/***************************************************************************
 * print_stderr():
 * Print messsage to stderr.
 ***************************************************************************/
static void
print_stderr (char *message)
{
  fprintf (stderr, "%s", message);
} /* End of print_stderr() */

/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Usage: %s [options] -o outfile\n\n", PACKAGE);
  fprintf (stderr,
           " ## Options ##\n"
           " -V             Report program version\n"
           " -h             Show this usage message\n"
           " -v             Be more verbose, multiple flags can be used\n"
           " -i infile      Specify the input leap seconds file\n"
           " -p             Allocate global leap seconds list before reading file\n"
           "\n"
           " -o outfile     Specify the output file, required\n"
           "\n"
           "This program tests ms_readleapsecondfile from genutil.\n"
           "\n");
} /* End of usage() */
