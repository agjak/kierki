CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu17 -g
TARGETS = kierki-klient

all: $(TARGETS)

kierki-klient: kierki-klient.o err.o kierki-common.o

# To są zależności wygenerowane automatycznie za pomocą polecenia `gcc -MM *.c`.

err.o: err.c err.h
kierki-klient.o: kierki-klient.c kierki-common.h err.h
kierki-common.o: kierki-common.c err.h

clean:
	rm -f $(TARGETS) *.o
