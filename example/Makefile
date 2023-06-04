
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Automatically configure URL support if libcurl is present
# Test for curl-config command and add build options if so
ifneq (,$(shell command -v curl-config))
        export LM_CURL_VERSION=$(shell curl-config --version)
        export CFLAGS:=$(CFLAGS) -DLIBMSEED_URL
        export LDFLAGS:=$(LDFLAGS) $(shell curl-config --libs)
endif

# Required compiler parameters
CFLAGS += -I..

LDFLAGS += -L..
LDLIBS := -lmseed $(LDLIBS)

# Build all *.c source as independent programs
SRCS := $(sort $(wildcard *.c))
BINS := $(SRCS:%.c=%)

.PHONY: all
.NOTPARALLEL: all
all: libmseed $(BINS)

.PHONY: libmseed
libmseed:
	@if [ ! -z "$(LM_CURL_VERSION)" ]; then \
		echo "Configured with $(LM_CURL_VERSION)"; \
	fi
	$(MAKE) -C .. $(MAKECMDGOALS)

# Build programs and check for executable
$(BINS) : % : %.c
	@printf 'Building $<\n';
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

.PHONY: clean
clean:
	@rm -rf *.o $(BINS) *.dSYM

.PHONY: install
install:
	@echo
	@echo "No install target, copy the executable(s) as needed"
	@echo
