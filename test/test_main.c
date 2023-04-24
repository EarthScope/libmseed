/* Main entry point for ynit tests orchestrated using µnit.
 *********************************************************************/

#include "munit.h"

/* Tests in other files */
extern MunitTest time_tests[];
extern MunitTest crc_tests[];
extern MunitTest sid_tests[];

/* Array of test suites */
static MunitSuite all_suites[] = {
    {"/time", time_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/crc", crc_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/sid", sid_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE}};

/* Top level suite including all test tests */
static MunitSuite suite = {
  (char*) "", NULL, &all_suites[0], 1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
  return munit_suite_main(&suite, (void*) "libmseed", argc, argv);
}