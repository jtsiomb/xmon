PREFIX = /usr/local

sys = $(shell uname -s | sed 's/IRIX.*/IRIX/' | sed 's/MINGW.*/MINGW/')

# -- platform source files --
src_Linux = $(wildcard src/linux/*.c) $(wildcard src/x11/*.c)
src_IRIX = $(wildcard src/irix/*.c) $(wildcard src/x11/*.c)
src_FreeBSD = $(wildcard src/bsd/*.c) $(wildcard src/x11/*.c)
src_Darwin = $(wildcard src/darwin/*.c) $(wildcard src/x11/*.c)
src_MINGW = $(wildcard src/win32/*.c)

# -- platform flags --
LDFLAGS_Linux = -lX11 -lXext
LDFLAGS_IRIX = -lX11 -lXext

CFLAGS_FreeBSD = -I/usr/local/include
LDFLAGS_FreeBSD = -L/usr/local/lib -lX11 -lXext

CFLAGS_Darwin = -I/opt/X11/include
LDFLAGS_Darwin = -L/opt/X11/lib -lX11 -lXext

LDFLAGS_MINGW = -mwindows


src = $(wildcard src/*.c) $(src_$(sys))
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = xmon

CFLAGS = -std=gnu89 -pedantic -Wall -g -Isrc $(CFLAGS_$(sys)) -MMD
LDFLAGS = -L/usr/X11R6/lib $(LDFLAGS_$(sys))

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: install
install: $(bin)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(bin) $(DESTDIR)$(PREFIX)/bin/$(bin)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)

.PHONY: cross
cross:
	$(MAKE) CC=i686-w64-mingw32-gcc sys=MINGW

.PHONY: cross-clean
cross-clean:
	$(MAKE) sys=MINGW clean
