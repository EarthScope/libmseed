
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

# Default settings for install target
PREFIX ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
LIBDIR ?= $(EXEC_PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
DATAROOTDIR ?= $(PREFIX)/share
DOCDIR ?= $(DATAROOTDIR)/doc/libmseed
MANDIR ?= $(DATAROOTDIR)/man
MAN3DIR ?= $(MANDIR)/man3

LIB_SRCS = fileutils.c genutils.c gswap.c lmplatform.c lookup.c \
           msrutils.c pack.c packdata.c traceutils.c tracelist.c \
           parseutils.c unpack.c unpackdata.c selection.c logging.c

LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_LOBJS = $(LIB_SRCS:.c=.lo)

LIB_NAME = libmseed
LIB_A = $(LIB_NAME).a

OS := $(shell uname -s)

# Build dynamic (.dylib) on macOS/Darwin, otherwise shared (.so)
ifeq ($(OS), Darwin)
	LIB_SO_BASE = $(LIB_NAME).dylib
	LIB_SO_MAJOR = $(LIB_NAME).$(MAJOR_VER).dylib
	LIB_SO = $(LIB_NAME).$(FULL_VER).dylib
	LIB_OPTS = -dynamiclib -compatibility_version $(COMPAT_VER) -current_version $(FULL_VER) -install_name $(LIB_SO)
else
	LIB_SO_BASE = $(LIB_NAME).so
	LIB_SO_MAJOR = $(LIB_NAME).so.$(MAJOR_VER)
	LIB_SO = $(LIB_NAME).so.$(FULL_VER)
	LIB_OPTS = -shared -Wl,--version-script=libmseed.map -Wl,-soname,$(LIB_SO_MAJOR)
endif

all: static

static: $(LIB_A)

shared dynamic: $(LIB_SO)

# Build static library
$(LIB_A): $(LIB_OBJS)
	@echo "Building static library $(LIB_A)"
	$(RM) -f $(LIB_A)
	$(AR) -crs $(LIB_A) $(LIB_OBJS)

# Build shared/dynamic library
$(LIB_SO): $(LIB_LOBJS)
	@echo "Building shared library $(LIB_SO)"
	$(RM) -f $(LIB_SO) $(LIB_SO_MAJOR) $(LIB_SO_BASE)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIB_OPTS) -o $(LIB_SO) $(LIB_LOBJS)
	ln -s $(LIB_SO) $(LIB_SO_BASE)
	ln -s $(LIB_SO) $(LIB_SO_MAJOR)

test check: static FORCE
	@$(MAKE) -C test test

clean:
	$(RM) $(LIB_OBJS) $(LIB_LOBJS) $(LIB_A) $(LIB_SO) $(LIB_SO_MAJOR) $(LIB_SO_BASE) 
	@$(MAKE) -C test clean
	@echo "All clean."

install: shared
	@echo "Installing into $(PREFIX)"
	@mkdir -p $(DESTDIR)$(PREFIX)/include
	@cp libmseed.h $(DESTDIR)$(PREFIX)/include
	@mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	@cp -a $(LIB_SO_BASE) $(LIB_SO_MAJOR) $(LIB_SO_NAME) $(LIB_SO) $(DESTDIR)$(LIBDIR)
	@sed -e 's|@prefix@|$(PREFIX)|g' \
	     -e 's|@exec_prefix@|$(EXEC_PREFIX)|g' \
	     -e 's|@libdir@|$(LIBDIR)|g' \
	     -e 's|@includedir@|$(PREFIX)/include|g' \
	     -e 's|@PACKAGE_NAME@|libmseed|g' \
	     -e 's|@PACKAGE_URL@|http://ds.iris.edu/ds/nodes/dmc/software/downloads/libmseed/|g' \
	     -e 's|@VERSION@|$(FULL_VER)|g' \
	     mseed.pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/mseed.pc
	@mkdir -p $(DESTDIR)$(DOCDIR)/example
	@cp -r example $(DESTDIR)$(DOCDIR)/
	@cp doc/libmseed-UsersGuide $(DESTDIR)$(DOCDIR)/
	@mkdir -p $(DESTDIR)$(MAN3DIR)
	@cp -a doc/ms*.3 $(DESTDIR)$(MAN3DIR)/

.SUFFIXES: .c .o .lo

# Standard object building
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Standard object building for shared library using -fPIC
.c.lo:
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

FORCE:
