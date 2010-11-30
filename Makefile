# This should be the --prefix of >=js-1.8.5+, where ./lib/libmozjs.so exists (usually /usr)
IPREFIX = ..
# Likewise, for irssi
JPREFIX = ..
INCLUDE = -I$(JPREFIX)/include -I$(IPREFIX)/include/js \
          -I$(IPREFIX)/include/irssi -I$(IPREFIX)/include/irssi/src -I$(IPREFIX)/include/irssi/src/core -I$(IPREFIX)/include/irssi/src/irc/core
CFLAGS += -DHAVE_CONFIG_H $(INCLUDE) `pkg-config --cflags glib-2.0` -fPIC -Wall -Wno-parentheses
LDFLAGS += -L$(JPREFIX)/lib -lmozjs

objects = jsirssi.o modlist.o mod_irssi.o modules.o

libjs_core.so: $(objects)
	ld $(LDFLAGS) -g -shared -soname $@ -fPIC -o $@ $(objects)

clean:
	rm -f libjs_core.so *.o
