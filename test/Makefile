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
CFLAGS += -I..

LDFLAGS += -L..
LDLIBS := -lmseed $(LDLIBS)

EXAMPLE_SRCS := $(sort $(wildcard lm_*.c))
EXAMPLE_BINS := $(EXAMPLE_SRCS:%.c=%)

EXAMPLE_TESTS := $(sort $(wildcard *.test))
EXAMPLE_TESTOUTS := $(EXAMPLE_TESTS:%.test=%.test.out)

UNIT_SRCS := $(sort $(wildcard test_*.c))
UNIT_TESTS := unittest-runner

# ASCII color coding for test results, green for PASSED and red for FAILED
PASSED := \033[0;32mPASSED\033[0m
FAILED := \033[0;31mFAILED\033[0m

TESTCOUNT := 0

test all: $(EXAMPLE_BINS) $(EXAMPLE_TESTOUTS) $(UNIT_TESTS) unittests
	@printf '%d tests conducted\n' $(TESTCOUNT)

# Build example programs and check for executables
$(EXAMPLE_BINS) : % : %.c
	@$(eval TESTCOUNT=$(shell echo $$(($(TESTCOUNT)+1))))
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS); exit 0;
	@if test -x $@; \
	  then printf '$(PASSED) Building $<\n'; \
	  else printf '$(FAILED) Building $<\n'; exit 1; \
        fi

# Run integration test scripts, create %.test.out files and compare to %.test.ref references
$(EXAMPLE_TESTOUTS) : %.test.out : %.test $(EXAMPLE_BINS) FORCE
	@$(eval TESTCOUNT=$(shell echo $$(($(TESTCOUNT)+1))))
	@$(shell ./$< > $@ 2>&1)
	@diff $<.ref $@ >/dev/null; \
          if [ $$? -eq 0 ]; \
            then printf '$(PASSED) Test $<\n'; \
            else printf '$(FAILED) Test $<, Compare $<.ref $@\n'; \
	    exit 0; \
          fi

# Build unit tests
$(UNIT_TESTS) : munit.h munit.c $(UNIT_SRCS)
	$(CC) $(CFLAGS) -o $@ munit.c $(UNIT_SRCS) $(LDFLAGS) $(LDLIBS)

# Execute unit tests
unittests: $(UNIT_TESTS)
	./$(UNIT_TESTS)

clean:
	@rm -rf $(EXAMPLE_BINS) $(EXAMPLE_TESTOUTS) $(UNIT_TESTS) *.dSYM

# Any targets using this empty FORCE rule as a prerequisite will always run
FORCE:
