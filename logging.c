/***************************************************************************
 * Common logging facility.
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmseed.h"

void ms_loginit_main (MSLogParam *logp,
                      void (*log_print) (char *), const char *logprefix,
                      void (*diag_print) (char *), const char *errprefix);

int ms_log_main (MSLogParam *logp, int level, va_list *varlist);

/* Initialize the global logging parameters */
MSLogParam gMSLogParam = {NULL, NULL, NULL, NULL};

/**********************************************************************/ /**
 * @brief Initialize the global logging parameters.
 *
 * Any message printing functions indicated must except a single
 * argument, namely a string \c (char *) that will contain the log
 * message.  If the function pointers are NULL, defaults will be used.
 *
 * If the log and error prefixes have been set they will be pre-pended
 * to the message.  If the prefixes are NULL, defaults will be used.
 *
 * @param[in] log_print Function to print log messages
 * @param[in] logprefix Prefix to add to log and diagnostic messages
 * @param[in] diag_print Function to print diagnostic and error messages
 * @param[in] errprefix Prefix to add to error messages
 ***************************************************************************/
void
ms_loginit (void (*log_print) (char *), const char *logprefix,
            void (*diag_print) (char *), const char *errprefix)
{
  ms_loginit_main (&gMSLogParam, log_print, logprefix, diag_print, errprefix);
} /* End of ms_loginit() */

/**********************************************************************/ /**
 * @brief Initialize specified ::MSLogParam logging parameters.
 *
 * If the logging parameters have not been initialized (logp == NULL),
 * new parameter space will be allocated.
 *
 * Any message printing functions indicated must except a single
 * argument, namely a string \c (char *) that will contain the log
 * message.  If the function pointers are NULL, defaults will be used.
 *
 * If the log and error prefixes have been set they will be pre-pended
 * to the message.  If the prefixes are NULL, defaults will be used.
 *
 * @param[in] logp ::MSLogParam logging parameters
 * @param[in] log_print Function to print log messages
 * @param[in] logprefix Prefix to add to log and diagnostic messages
 * @param[in] diag_print Function to print diagnostic and error messages
 * @param[in] errprefix Prefix to add to error messages
 *
 * @returns a pointer to the created/re-initialized MSLogParam struct
 * on success and NULL on error.
 ***************************************************************************/
MSLogParam *
ms_loginit_l (MSLogParam *logp,
              void (*log_print) (char *), const char *logprefix,
              void (*diag_print) (char *), const char *errprefix)
{
  MSLogParam *llog;

  if (logp == NULL)
  {
    llog = (MSLogParam *)libmseed_memory.malloc (sizeof (MSLogParam));

    if (llog == NULL)
    {
      ms_log (2, "%s(): Cannot allocate memory\n", __func__);
      return NULL;
    }

    llog->log_print = NULL;
    llog->logprefix = NULL;
    llog->diag_print = NULL;
    llog->errprefix = NULL;
  }
  else
  {
    llog = logp;
  }

  ms_loginit_main (llog, log_print, logprefix, diag_print, errprefix);

  return llog;
} /* End of ms_loginit_l() */

/**********************************************************************/ /**
 * @brief Initialize the logging subsystem.
 *
 * This function modifies the logging parameters in the supplied
 * ::MSLogParam. Use NULL for the function pointers or the prefixes if
 * they should not be changed from previously set or default values.
 *
 ***************************************************************************/
void
ms_loginit_main (MSLogParam *logp,
                 void (*log_print) (char *), const char *logprefix,
                 void (*diag_print) (char *), const char *errprefix)
{
  if (!logp)
    return;

  if (log_print)
    logp->log_print = log_print;

  if (logprefix)
  {
    if (strlen (logprefix) >= MAX_LOG_MSG_LENGTH)
    {
      ms_log_l (logp, 2, "%s", "log message prefix is too large\n");
    }
    else
    {
      logp->logprefix = logprefix;
    }
  }

  if (diag_print)
    logp->diag_print = diag_print;

  if (errprefix)
  {
    if (strlen (errprefix) >= MAX_LOG_MSG_LENGTH)
    {
      ms_log_l (logp, 2, "%s", "error message prefix is too large\n");
    }
    else
    {
      logp->errprefix = errprefix;
    }
  }

  return;
} /* End of ms_loginit_main() */

/**********************************************************************/ /**
 * @brief Log message using global logging parameters.
 *
 * Three message levels are recognized, see @ref logging-levels for
 * more information.
 *
 * This function builds the log/error message and passes to it to the
 * appropriate print function.  If custom printing functions have not
 * been defined, messages will be printed with \c fprintf(), log
 * messages to \c stdout and error messages to \c stderr.
 *
 * If the log/error prefixes have been set they will be pre-pended to
 * the message.  If no custom log prefix is set none will be included.
 * If no custom error prefix is set \c "Error: " will be included.
 *
 * All messages will be truncated at the ::MAX_LOG_MSG_LENGTH, this
 * includes any prefix.
 *
 * @param[in] level Message level
 * @param[in] ... Message in printf() style
 *
 * @returns Return value of ms_log_main().
 ***************************************************************************/
int
ms_log (int level, ...)
{
  int retval;
  va_list varlist;

  va_start (varlist, level);

  retval = ms_log_main (&gMSLogParam, level, &varlist);

  va_end (varlist);

  return retval;
} /* End of ms_log() */

/**********************************************************************/ /**
 * @brief Log message using specified logging parameters.
 *
 * The function uses logging parameters specified in the supplied
 * ::MSLogParam.  This reentrant capability allows using different
 * parameters in different parts of a program or different threads.
 *
 * Three message levels are recognized, see @ref logging-levels for
 * more information.
 *
 * This function builds the log/error message and passes to it to the
 * appropriate print function.  If custom printing functions have not
 * been defined, messages will be printed with \c fprintf(), log
 * messages to \c stdout and error messages to \c stderr.
 *
 * If the log/error prefixes have been set they will be pre-pended to
 * the message.  If no custom log prefix is set none will be included.
 * If no custom error prefix is set \c "Error: " will be included.
 *
 * All messages will be truncated at the ::MAX_LOG_MSG_LENGTH, this
 * includes any prefix.
 *
 * @param[in] logp Pointer to ::MSLogParam to use for this message
 * @param[in] level Message level
 * @param[in] ... Message in printf() style
 *
 * @returns The number of characters formatted on success, and a
 * negative value on error.
 ***************************************************************************/
int
ms_log_l (MSLogParam *logp, int level, ...)
{
  int retval;
  va_list varlist;
  MSLogParam *llog;

  if (!logp)
    llog = &gMSLogParam;
  else
    llog = logp;

  va_start (varlist, level);

  retval = ms_log_main (llog, level, &varlist);

  va_end (varlist);

  return retval;
} /* End of ms_log_l() */

/**********************************************************************/ /**
 * @brief Log message using specified logging parameters and \c va_list
 *
 * The central logging routine.  Usually a calling program would not
 * use this directly, but instead would use ms_log() and ms_log_l().
 *
 * @param[in] logp Pointer to ::MSLogParam to use for this message
 * @param[in] level Message level
 * @param[in] varlist Message in a \c va_list in printf() style
 *
 * @returns The number of characters formatted on success, and a
 * negative value on error.
 ***************************************************************************/
int
ms_log_main (MSLogParam *logp, int level, va_list *varlist)
{
  static char message[MAX_LOG_MSG_LENGTH];
  int retvalue = 0;
  int presize;
  const char *format;

  if (!logp)
  {
    fprintf (stderr, "%s() called without specifying log parameters", __func__);
    return -1;
  }

  message[0] = '\0';

  format = va_arg (*varlist, const char *);

  if (level >= 2) /* Error message */
  {
    if (logp->errprefix != NULL)
    {
      strncpy (message, logp->errprefix, MAX_LOG_MSG_LENGTH);
      message[MAX_LOG_MSG_LENGTH - 1] = '\0';
    }
    else
    {
      strncpy (message, "Error: ", MAX_LOG_MSG_LENGTH);
    }

    presize = strlen (message);
    retvalue = vsnprintf (&message[presize],
                          MAX_LOG_MSG_LENGTH - presize,
                          format, *varlist);

    message[MAX_LOG_MSG_LENGTH - 1] = '\0';

    if (logp->diag_print != NULL)
    {
      logp->diag_print (message);
    }
    else
    {
      fprintf (stderr, "%s", message);
    }
  }
  else if (level == 1) /* Diagnostic message */
  {
    if (logp->logprefix != NULL)
    {
      strncpy (message, logp->logprefix, MAX_LOG_MSG_LENGTH);
      message[MAX_LOG_MSG_LENGTH - 1] = '\0';
    }

    presize = strlen (message);
    retvalue = vsnprintf (&message[presize],
                          MAX_LOG_MSG_LENGTH - presize,
                          format, *varlist);

    message[MAX_LOG_MSG_LENGTH - 1] = '\0';

    if (logp->diag_print != NULL)
    {
      logp->diag_print (message);
    }
    else
    {
      fprintf (stderr, "%s", message);
    }
  }
  else if (level == 0) /* Normal log message */
  {
    if (logp->logprefix != NULL)
    {
      strncpy (message, logp->logprefix, MAX_LOG_MSG_LENGTH);
      message[MAX_LOG_MSG_LENGTH - 1] = '\0';
    }

    presize = strlen (message);
    retvalue = vsnprintf (&message[presize],
                          MAX_LOG_MSG_LENGTH - presize,
                          format, *varlist);

    message[MAX_LOG_MSG_LENGTH - 1] = '\0';

    if (logp->log_print != NULL)
    {
      logp->log_print (message);
    }
    else
    {
      fprintf (stdout, "%s", message);
    }
  }

  return retvalue;
} /* End of ms_log_main() */
