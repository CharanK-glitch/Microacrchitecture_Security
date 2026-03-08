CC = gcc
CFLAGS = -O0 -Wall  
all: bernstein

bernstein: main.o aes.o
	$(CC) $(CFLAGS) -o bernstein main.o aes.o

main.o: main.c aes.h
	$(CC) $(CFLAGS) -c main.c

aes.o: aes.c aes.h
	$(CC) $(CFLAGS) -c aes.c

clean:
	rm -f *.o bernstein

run: all
	./bernstein