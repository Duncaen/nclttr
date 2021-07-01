CFLAGS?=-g -O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wall
LDLIBS=-lX11 -lXi -lXfixes
PROG=nclttr
OBJS=nclttr.o

PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin
MANDIR?=$(PREFIX)/share/man

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

README: $(PROG).1
	mandoc -Tutf8 $< | col -bx >$@

clean:
	rm -f $(PROG) $(OBJS)

install:
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m755 $(PROG) $(DESTDIR)$(BINDIR)
	install -m644 $(PROG).1 $(DESTDIR)$(MANDIR)/man1
