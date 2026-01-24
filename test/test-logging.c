#include <tau/tau.h>
#include <libmseed.h>
#include <string.h>

/* Helper variables to track custom print function calls */
static int log_print_called = 0;
static int diag_print_called = 0;
static char last_log_message[1024] = {0};
static char last_diag_message[1024] = {0};

/* Custom print function for log messages */
static void
custom_log_print (const char *message)
{
  log_print_called++;
  strncpy (last_log_message, message, sizeof (last_log_message) - 1);
}

/* Custom print function for diagnostic/error messages */
static void
custom_diag_print (const char *message)
{
  diag_print_called++;
  strncpy (last_diag_message, message, sizeof (last_diag_message) - 1);
}

/* Reset helper variables */
static void
reset_print_counters (void)
{
  log_print_called = 0;
  diag_print_called = 0;
  last_log_message[0] = '\0';
  last_diag_message[0] = '\0';
}

TEST (logging, rloginit_basic)
{
  /* Test basic initialization with all NULL parameters */
  ms_rloginit (NULL, NULL, NULL, NULL, 0);

  /* Test initialization with custom print functions */
  reset_print_counters ();
  ms_rloginit (custom_log_print, "LOG: ", custom_diag_print, "ERROR: ", 0);

  /* Verify custom functions are called */
  ms_log (0, "Test log message");
  CHECK (log_print_called > 0, "Custom log print function was not called");
  CHECK_STREQ (last_log_message, "LOG: Test log message");

  ms_log (2, "Test error message");
  CHECK (diag_print_called > 0, "Custom diag print function was not called");
  CHECK_STREQ (last_diag_message, "ERROR: Test error message");

  /* Test initialization with message registry enabled */
  ms_rloginit (NULL, NULL, NULL, NULL, 10);
  ms_log (2, "Error to be stored");

  /* Reset to defaults */
  ms_rloginit (NULL, NULL, NULL, NULL, 0);

  /* Test with very long prefix (should be rejected) */
  char long_prefix[MAX_LOG_MSG_LENGTH + 10];
  memset (long_prefix, 'X', sizeof (long_prefix) - 1);
  long_prefix[sizeof (long_prefix) - 1] = '\0';

  ms_rloginit (custom_log_print, long_prefix, custom_diag_print, NULL, 0);
  CHECK_STREQ (last_diag_message, "ERROR: log message prefix is too large");

  ms_rloginit (custom_log_print, NULL, custom_diag_print, long_prefix, 0);
  CHECK_STREQ (last_diag_message, "ERROR: error message prefix is too large");
}

TEST (logging, rloginit_l)
{
  MSLogParam *logp;

  /* Test allocation when logp is NULL */
  logp = ms_rloginit_l (NULL, NULL, NULL, NULL, NULL, 0);
  REQUIRE (logp != NULL, "ms_rloginit_l failed to allocate MSLogParam");

  /* Verify initial values */
  CHECK (logp->registry.maxmessages == 0, "maxmessages not initialized correctly");
  CHECK (logp->registry.messagecnt == 0, "messagecnt not initialized correctly");
  CHECK (logp->registry.messages == NULL, "messages not initialized to NULL");

  /* Test reinitialization with custom values */
  logp = ms_rloginit_l (logp, custom_log_print, "PREFIX: ", custom_diag_print, "ERR: ", 0);
  REQUIRE (logp != NULL, "ms_rloginit_l failed to reinitialize");
  CHECK (logp->log_print == custom_log_print, "log_print function not set");
  CHECK (logp->diag_print == custom_diag_print, "diag_print function not set");
  CHECK_STREQ (logp->logprefix, "PREFIX: ");
  CHECK_STREQ (logp->errprefix, "ERR: ");

  /* Test that custom functions work */
  reset_print_counters ();
  ms_log_l (logp, 0, "Test message");
  CHECK (log_print_called == 1, "Custom log function not used");
  CHECK_STREQ (last_log_message, "PREFIX: Test message");
  ms_log_l (logp, 2, "Test error message");
  CHECK (diag_print_called == 1, "Custom diag function not used");
  CHECK_STREQ (last_diag_message, "ERR: Test error message");

  /* Test reinitialization with log registry enabled */
  logp = ms_rloginit_l (logp, custom_log_print, "PREFIX: ", custom_diag_print, "ERR: ", 5);
  REQUIRE (logp != NULL, "ms_rloginit_l failed to reinitialize");
  CHECK (logp->registry.maxmessages == 5, "maxmessages not set correctly");
  CHECK (logp->registry.messagecnt == 0, "messagecnt not reset to 0");
  CHECK (logp->registry.messages == NULL, "messages not reset to NULL");

  free (logp);
}

TEST (logging, loginit_macros)
{
  MSLogParam custom_param = MSLogParam_INITIALIZER;
  MSLogParam *logp;

  /* Test ms_loginit macro (wrapper that disables registry) */
  reset_print_counters ();
  ms_loginit (custom_log_print, "LOG: ", custom_diag_print, "ERR: ");

  ms_log (0, "Test message");
  CHECK (log_print_called > 0, "ms_loginit macro did not set log function");

  /* Test ms_loginit_l macro */
  reset_print_counters ();
  logp = ms_loginit_l (&custom_param, custom_log_print, NULL, custom_diag_print, NULL);
  REQUIRE (logp != NULL, "ms_loginit_l macro failed to allocate MSLogParam");

  CHECK (logp == &custom_param, "ms_loginit_l macro returned wrong pointer");
  CHECK (logp->registry.maxmessages == 0, "ms_loginit_l should disable registry (maxmessages=0)");

  ms_log_l (logp, 1, "Warning message");
  CHECK (diag_print_called > 0, "ms_loginit_l macro did not set diag function");
}

TEST (logging, logregistry_basic)
{
  MSLogParam custom_param = MSLogParam_INITIALIZER;
  MSLogParam *logp;
  int freed;

  /* Initialize with registry enabled */
  logp = ms_rloginit_l (&custom_param, custom_log_print, NULL, custom_diag_print, NULL, 10);
  REQUIRE (logp != NULL, "ms_rloginit_l failed to allocate MSLogParam");

  /* Add some messages to registry */
  ms_log_l (logp, 1, "Warning 1");
  ms_log_l (logp, 2, "Error 1");
  ms_log_l (logp, 1, "Warning 2");
  ms_log_l (logp, 2, "Error 2");

  CHECK (logp->registry.messagecnt == 4, "messagecnt should be 4");
  CHECK (logp->registry.messages != NULL, "messages should not be NULL");

  /* Emit all messages */
  reset_print_counters ();
  int emitted = ms_rlog_emit (logp, 0, 0);
  CHECK (emitted == 4, "Should have emitted 4 messages");
  CHECK (diag_print_called == 4, "Custom diag function not used 4 times");
  CHECK_STREQ (last_diag_message, "Error: Error 2");
  CHECK (logp->registry.messagecnt == 0, "messagecnt should be 0 after emit all");
  CHECK (logp->registry.messages == NULL, "messages should be NULL after emit all");

  /* Add more messages and free them */
  ms_log_l (logp, 2, "Error 2");
  ms_log_l (logp, 2, "Error 3");

  freed = ms_rlog_free (logp);
  CHECK (freed == 2, "Should have freed 2 messages");
  CHECK (logp->registry.messagecnt == 0, "messagecnt should be 0 after free");
  CHECK (logp->registry.messages == NULL, "messages should be NULL after free");
}

TEST (logging, logregistry_pop)
{
  MSLogParam custom_param = MSLogParam_INITIALIZER;
  MSLogParam *logp;
  char message[256];
  int length;

  /* Initialize with registry enabled */
  logp = ms_rloginit_l (&custom_param, custom_log_print, NULL, custom_diag_print, NULL, 10);
  REQUIRE (logp != NULL, "ms_rloginit_l failed to allocate MSLogParam");

  /* Test pop from empty registry */
  length = ms_rlog_pop (logp, message, sizeof (message), 0);
  CHECK (length == 0, "Pop from empty registry should return 0");

  /* Add messages */
  ms_log_l (logp, 2, "First error");
  ms_log_l (logp, 2, "Second error");
  ms_log_l (logp, 2, "Third error");

  CHECK (logp->registry.messagecnt == 3, "Should have 3 messages");

  /* Pop latest message (should be "Third error") */
  length = ms_rlog_pop (logp, message, sizeof (message), 0);
  CHECK (length > 0, "Pop should return message length");
  CHECK (strstr (message, "Third error") != NULL, "Should pop third message first");
  CHECK (logp->registry.messagecnt == 2, "messagecnt should be 2 after pop");

  /* Pop next message */
  length = ms_rlog_pop (logp, message, sizeof (message), 0);
  CHECK (strstr (message, "Second error") != NULL, "Should pop second message");
  CHECK (logp->registry.messagecnt == 1, "messagecnt should be 1 after second pop");

  /* Pop last message */
  length = ms_rlog_pop (logp, message, sizeof (message), 0);
  CHECK (strstr (message, "First error") != NULL, "Should pop first message last");
  CHECK (logp->registry.messagecnt == 0, "messagecnt should be 0 after popping all");
  CHECK (logp->registry.messages == NULL, "messages should be NULL after popping all");

  /* Try to pop from empty registry again */
  length = ms_rlog_pop (logp, message, sizeof (message), 0);
  CHECK (length == 0, "Pop from empty registry should return 0");
}

TEST (logging, logregistry_pop_validation)
{
  MSLogParam custom_param = MSLogParam_INITIALIZER;
  MSLogParam *logp;
  char message[256];
  int result;

  /* Initialize with registry enabled */
  logp = ms_rloginit_l (&custom_param, NULL, NULL, NULL, NULL, 10);
  ms_log_l (logp, 2, "Test error");

  /* Test with NULL message buffer */
  result = ms_rlog_pop (logp, NULL, sizeof (message), 0);
  CHECK (result == -1, "Pop with NULL buffer should return -1");

  /* Test with zero size */
  result = ms_rlog_pop (logp, message, 0, 0);
  CHECK (result == -1, "Pop with zero size should return -1");

  /* Message should still be in registry */
  CHECK (logp->registry.messagecnt == 1, "Message should not be removed after failed pops");

  /* Clean up */
  ms_rlog_free (logp);
}

TEST (logging, logregistry_maxmessages)
{
  MSLogParam custom_param = MSLogParam_INITIALIZER;
  MSLogParam *logp;
  int i;

  /* Initialize with max 5 messages */
  logp = ms_rloginit_l (&custom_param, custom_log_print, NULL, custom_diag_print, NULL, 5);

  /* Add 10 messages, only 5 should be kept */
  for (i = 0; i < 10; i++)
  {
    ms_log_l (logp, 2, "Error %d", i);
  }

  /* Should only have 5 messages (oldest discarded) */
  CHECK (logp->registry.messagecnt == 5, "Should only have maxmessages (5) in registry");

  /* Clean up */
  ms_rlog_free (logp);
}
