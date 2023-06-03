# This Makefile requires GNU make, sometimes available as gmake.
#
# A simple test suite for libmseed.
# See README for description.
#
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Required compiler parameters
CFLAGS += -I.. -I.

LDFLAGS += -L..
LDLIBS := -lmseed $(LDLIBS)

# Source code from example programs
EXAMPLE_SRCS := $(sort $(wildcard lm_*.c))
EXAMPLE_BINS := $(EXAMPLE_SRCS:%.c=%)

# Source code for tests
TEST_SRCS := $(sort $(wildcard test-*.c))
TEST_RUNNER := test-runner

# ASCII color coding for test results, green for PASSED and red for FAILED
PASSED := \033[0;32mPASSED\033[0m
FAILED := \033[0;31mFAILED\033[0m

test all: $(EXAMPLE_BINS) $(TEST_RUNNER) runtests

# Build example programs and check for executables
$(EXAMPLE_BINS) : % : %.c
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS); exit 0;
	@if test -x $@; \
	  then printf '$(PASSED) Building $<\n'; \
	  else printf '$(FAILED) Building $<\n'; exit 1; \
	fi

# Build tests
.PHONY: $(TEST_RUNNER)
$(TEST_RUNNER):
	$(CC) $(CFLAGS) -o $@ $(TEST_SRCS) $(LDFLAGS) $(LDLIBS) -Wno-unused-but-set-variable

# Execute tests
runtests: $(TEST_RUNNER)
	@./$(TEST_RUNNER)

clean:
	@rm -rf $(EXAMPLE_BINS) $(TEST_RUNNER) testdata-* *.dSYM