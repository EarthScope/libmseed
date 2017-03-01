
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use

# Extract version from libmseed.h, expected line should include LIBMSEED_VERSION "#.#.#"
MAJOR_VER = $(shell grep LIBMSEED_VERSION libmseed.h | grep -Eo '[0-9]+.[0-9]+.[0-9]+' | cut -d . -f 1)
FULL_VER = $(shell grep LIBMSEED_VERSION libmseed.h | grep -Eo '[0-9]+.[0-9]+.[0-9]+')
COMPAT_VER = $(MAJOR_VER).0.0

LIB_SRCS = fileutils.c genutils.c gswap.c lmplatform.c lookup.c \
           msrutils.c pack.c packdata.c traceutils.c tracelist.c \
           parseutils.c unpack.c unpackdata.c selection.c logging.c

LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_DOBJS = $(LIB_SRCS:.c=.lo)

LIB_A = libmseed.a
LIB_SO_BASE = libmseed.so
LIB_SO_NAME = $(LIB_SO_BASE).$(MAJOR_VER)
LIB_SO = $(LIB_SO_BASE).$(FULL_VER)
LIB_DYN_NAME = libmseed.dylib
LIB_DYN = libmseed.$(FULL_VER).dylib

all: static

static: $(LIB_A)

shared: $(LIB_SO)

dynamic: $(LIB_DYN)

# Build static library
$(LIB_A): $(LIB_OBJS)
	@echo "Building static library $(LIB_A)"
	rm -f $(LIB_A)
	ar -crs $(LIB_A) $(LIB_OBJS)

# Build shared library using GCC-style options
$(LIB_SO): $(LIB_DOBJS)
	@echo "Building shared library $(LIB_SO)"
	rm -f $(LIB_SO) $(LIB_SONAME) $(LIB_SO_BASE)
	$(CC) $(CFLAGS) -shared -Wl,-soname -Wl,$(LIB_SO_NAME) -o $(LIB_SO) $(LIB_DOBJS)
	ln -s $(LIB_SO) $(LIB_SO_BASE)
	ln -s $(LIB_SO) $(LIB_SO_NAME)

# Build dynamic library (usually for macOS)
$(LIB_DYN): $(LIB_DOBJS)
	@echo "Building dynamic library $(LIB_DYN)"
	rm -f $(LIB_DYN) $(LIB_DYN_NAME)
	$(CC) $(CFLAGS) -dynamiclib -compatibility_version $(COMPAT_VER) -current_version $(FULL_VER) -install_name $(LIB_DYN_NAME) -o $(LIB_DYN) $(LIB_DOBJS)
	ln -sf $(LIB_DYN) $(LIB_DYN_NAME)

test: static FORCE
	@$(MAKE) -C test test

clean:
	@rm -f $(LIB_OBJS) $(LIB_DOBJS) $(LIB_A) $(LIB_SO) $(LIB_SO_NAME) $(LIB_SO_BASE) $(LIB_DYN) $(LIB_DYN_NAME)
	@$(MAKE) -C test clean
	@echo "All clean."

install:
	@echo
	@echo "No install target, copy the library and libmseed.h header as needed"
	@echo


.SUFFIXES: .c .o .lo

# Standard object building
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

# Standard object building for dynamic library components using -fPIC
.c.lo:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

FORCE:
