CFLAGS = -Wall -Wextra -pedantic -Wconversion -std=gnu99

ftserver: ftserver.c
	gcc -o ftserver ftserver.c $(CFLAGS)