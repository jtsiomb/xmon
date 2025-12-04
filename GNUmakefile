PREFIX = /usr/local

src = $(wildcard src/*.c) $(src_$(sys))
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = xmon

CFLAGS = -std=gnu89 -pedantic -Wall -g -Isrc $(CFLAGS_$(sys)) -MMD
LDFLAGS = $(LDFLAGS_$(sys)) -lX11

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

sys = $(shell uname -s | sed 's/IRIX.*/IRIX/')

# -- platform source files --
src_Linux = $(wildcard src/linux/*.c)
src_IRIX = $(wildcard src/irix/*.c)
src_Darwin = $(wildcard src/darwin/*.c)

# -- platform flags --
CFLAGS_Darwin = -I/opt/X11/include
LDFLAGS_Darwin = -L/opt/X11/lib

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
