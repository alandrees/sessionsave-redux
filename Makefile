# Change this to the path where your pidgin source tree is:
PIDGIN_SRCDIR=/mnt/data/compiled_from_source/pidgin-2.10.1/pidgin

# Comment this out and uncomment the next if you are not building on Windows.
# Note that on windows, you need to build pidgin fully using 'make -f
# Makefile.mingw'. On unix you can get away with just running ./configure.
#PLATFORM = win32
#PLATFORM = unix

# If this doesn't work, you'll have to set it manually to the path where
# pidgin installs.  To find where that is, type "which pidgin", and remove the
# "bin/pidgin" at the end of the output.
PIDGIN_INSTALLPREFIX=`grep ^prefix $(PIDGIN_SRCDIR)/Makefile | sed -e 's,[^/]*,,'`

# You shouldn't need to change this.
PIDGIN_LIBDIR=$(PIDGIN_INSTALLPREFIX)/lib/pidgin
PIDGIN_PLUGINDIR=$(PIDGIN_SRCDIR)/plugins

VERSION = 2.6.5-1

ifeq ($(PLATFORM), win32)
MAKEARGS = -f Makefile.mingw
PLUGIN = sessionsave.dll
else
MAKEARGS =
PLUGIN = sessionsave.so
endif

DISTFILES=\
COPYING \
INSTALL \
Makefile \
sessionsave.c \
win32

all: $(PLUGIN)

$(PLUGIN): sessionsave.c
	cp sessionsave.c $(PIDGIN_PLUGINDIR)/
	make -C $(PIDGIN_PLUGINDIR) $(MAKEARGS) $(PLUGIN)
	cp -a $(PIDGIN_PLUGINDIR)/$(PLUGIN) .

install: all
	cp $(PIDGIN_PLUGINDIR)/$(PLUGIN) $(PIDGIN_LIBDIR)

win32: all
	cp $(PIDGIN_PLUGINDIR)/$(PLUGIN) win32
	make -C win32

dist:
	rm -rf sessionsave-$(VERSION)
	mkdir sessionsave-$(VERSION)
	cp -a $(DISTFILES) sessionsave-$(VERSION)
	make -C sessionsave-$(VERSION) clean
	tar czf sessionsave-$(VERSION).tar.gz sessionsave-$(VERSION)
	rm -rf sessionsave-$(VERSION)

clean:
	make -C win32 clean
	rm -f $(PIDGIN_PLUGINDIR)/$(PLUGIN)
