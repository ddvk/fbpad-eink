CFLAGS += -Wall -O2
LDFLAGS += -lutil -lm # for openpty

all: fbpad

%.o: %.c conf.h font.h
	$(CC) -c $(CFLAGS) $<

font.h:
	xxd -i fonts/courr.tf > font.h
fbpad: fbpad.o term.o pad.o draw.o font.o isdw.o scrsnap.o FBInk/Release/libfbink.a
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o fbpad
