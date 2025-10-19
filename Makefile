CC = gcc

CFLAGS = `pkg-config --cflags gio-2.0` -Iinclude
LDFLAGS = `pkg-config --libs gio-2.0 libpulse-mainloop-glib`

SOURCES = src/main.c src/ofono.c src/test.c src/pulse.c

TARGET = ofono-toned

PREFIX ?= /usr

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(SOURCES) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/libexec
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/libexec/
	install -d $(DESTDIR)$(PREFIX)/lib/systemd/user
	install -m 644 data/ofono-toned.service $(DESTDIR)$(PREFIX)/lib/systemd/user

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/libexec/$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/lib/systemd/user/ofono-toned.service
