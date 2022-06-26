VERSION = 5.3
PREFIX = /usr/local
BINDIR = ${PREFIX}/bin
LIBDIR = $(PREFIX)/lib/lua/${VERSION}
MANDIR = $(PREFIX)/man
MAN1DIR = $(MANDIR)/man1

# uncomment this to disable LuaJIT or readline
USE_LUAJIT = 1
USE_READLINE = 1

READLINE_CFLAGS = -DHAVE_LIBREADLINE -DHAVE_READLINE_READLINE_H -DHAVE_READLINE_HISTORY -DHAVE_READLINE_HISTORY_H
READLINE_LIBS = -lreadline -lhistory -ltinfo -lncurses -lm -ldl

LUA_ARCHIVE = /usr/lib/libluajit-5.1.a
LUA_ARCHIVE = /usr/lib/x86_64-linux-gnu/libluajit-5.1.a

CFLAGS = -g -fPIC -D_GNU_SOURCE $(READLINE_CFLAGS) -DHAVE_ASPRINTF
CFLAGS += -Wall -Wextra -Wno-unused-parameter -DHAVE_ASPRINTF

# osx fix
CFLAGS += -I/usr/local/opt/readline/include -L/usr/local/opt/readline/lib

ifdef USE_LUAJIT
	LUA_CFLAGS  = $(shell pkg-config --cflags luajit)
	LUA_LDFLAGS = $(shell pkg-config --libs luajit)
	CFLAGS += -DLUAP_LUAJIT=1 -I/usr/include/luajit-2.0 -I/data/data/com.termux/files/usr/include/luajit-2.0
else # regular lua
	LUA_CFLAGS = $(shell pkg-config --cflags lua${VERSION})
	LUA_LDFLAGS = $(shell pkg-config --libs-only-L lua${VERSION})
endif

# Comment out the following to suppress completion of certain kinds of
# symbols.

CFLAGS += -DCOMPLETE_KEYWORDS     # Keywords such as for, while, etc.
CFLAGS += -DCOMPLETE_MODULES      # Module names.
CFLAGS += -DCOMPLETE_TABLE_KEYS   # Table keys, including global variables.
CFLAGS += -DCOMPLETE_METATABLE_KEYS # Keys in the __index metafield, if
                                          # it exists and is a table.
CFLAGS += -DCOMPLETE_FILE_NAMES   # File names.

# Comment out the following to disable tracking of results.  When
# enabled each returned value, that is, each value the prompt prints
# out, is also added to a table for future reference.

# CFLAGS += '-DSAVE_RESULTS'

# The name of the table holding the results can be configured below.

CFLAGS += '-DRESULTS_TABLE_NAME="_"'

# The table holding the results, can also be made to have weak values,
# so as not to interfere with garbage collection.  To enable this
# uncomment the second line below.

# CFLAGS += '-DWEAK_RESULTS'

# Uncomment the following line and customize the prefix as desired to
# keep the auto-completer from considering certain table keys (and
# hence global variables) for completion.

# CFLAGS += '-DHIDDEN_KEY_PREFIX="_"'

# When completing certain kinds of values, such as tables or
# functions, the completer also appends certain useful suffixes such
# as '.', '[' or '('. Normally these are appended only when the
# value's name has already been fully entered, or previously fully
# completed, so that one can still complete the name without the
# suffix.  In order to append the suffix one then only has to press
# the completion key one more time.
#
# Uncomment the following line to make the completer always append
# these suffixes.

# CFLAGS += -DALWAYS_APPEND_SUFFIXES

# The autocompleter can complete module names as if they were already
# require'd and available as a global variable.  Once the module name
# is fully completed a further tab press loads the module and exports
# it as a global variable so that all further tab-completions now
# apply to the module's table.
#
# Uncomment the following line to disable this functionality.  Module
# names will then only be completed inside strings (for use with
# require).

# CFLAGS += -DNO_MODULE_LOAD

# Uncomment to make the auto-completer ask for confirmation before
# loading or globalizing a module.

# CFLAGS += -DCONFIRM_MODULE_LOAD

LDFLAGS := ${LDFLAGS}
INSTALL  = /usr/bin/install

ifdef USE_READLINE
  LDFLAGS += $(READLINE_LIBS)
endif

# if building within termux, include android libc
ifdef ANDROID_DATA
  LDFLAGS += -landroid-glob
endif

all: lp

lp: lp.c prompt.c prompt.h
	$(CC) -o lp ${CFLAGS} ${LUA_CFLAGS} lp.c prompt.c ${DEFS} ${LDFLAGS} ${LUA_LDFLAGS}

static: lp.c prompt.c prompt.h
	$(CC) -static -o lp ${CFLAGS} ${LUA_CFLAGS} lp.c prompt.c ${DEFS} ${LUA_ARCHIVE} ${LDFLAGS} ${LUA_LDFLAGS}

install:lp
	mkdir -p $(BINDIR)
	$(INSTALL) lp $(BINDIR)/lp
	# mkdir -p $(MAN1DIR)
	# $(INSTALL) -m 644 lp.1 $(MAN1DIR)/lp.1

uninstall:
	rm -f $(BINDIR)/lp $(MAN1DIR)/lp.1

clean:
	rm -f lp *~
