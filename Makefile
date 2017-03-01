
# Build environment can be configured the following
# environment variables:
#   CC : Specify the C compiler to use
#   CFLAGS : Specify compiler options to use
#   LDFLAGS : Specify linker options to use
#   CPPFLAGS : Specify c-preprocessor options to use

# Extract version from libmseed.h, expected line should include LIBMSEED_VERSION "#.#.#"
MAJOR_VER = $(shell grep LIBMSEED_VERSION libmseed.h | grep -Eo '[0-9]+.[0-9]+.[0-9]+' | cut -d . -f 1)
FULL_VER = $(shell grep LIBMSEED_VERSION libmseed.h | grep -Eo '[0-9]+.[0-9]+.[0-9]+')
COMPAT_VER = $(MAJOR_VER).0.0

PREFIX	?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
LIBDIR	?= $(EXEC_PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
DATAROOTDIR ?= $(PREFIX)/share
DOCDIR	?= $(DATAROOTDIR)/doc/libmseed
MANDIR ?= $(DATAROOTDIR)/man
MAN3DIR ?= $(MANDIR)/man3

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

install: shared
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp libmseed.h $(DESTDIR)$(PREFIX)/include
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	cp -d libmseed.so* $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(DOCDIR)/example
	cp -r example $(DESTDIR)$(DOCDIR)/
	cp doc/libmseed-UsersGuide $(DESTDIR)$(DOCDIR)/
	mkdir -p $(DESTDIR)$(MAN3DIR)
	cp -d doc/ms*.3 $(DESTDIR)$(MAN3DIR)/

.SUFFIXES: .c .o .lo

# Standard object building
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Standard object building for dynamic library components using -fPIC
.c.lo:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

FORCE:
