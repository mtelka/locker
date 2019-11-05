# Public Domain

locker: locker.c
	gcc -Wall -o locker locker.c

clean:
	rm -f locker

.PHONY: clean
