CC = gcc
CFLAGS = -Wall -O3 $(shell pkg-config --cflags copper-eval)
LDFLAGS = -fopenmp $(shell pkg-config --libs --static copper-eval)

.PHONY: clean

all: swish-batch

swish-batch: swish-batch.c
	$(CC) $(CFLAGS) swish-batch.c -o $@ $(LDFLAGS)

clean:
	rm -f swish-batch
