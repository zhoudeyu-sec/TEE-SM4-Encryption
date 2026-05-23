CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall
LDFLAGS = -lteec

all: ca_crypto

ca_crypto: ca_crypto.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f ca_crypto