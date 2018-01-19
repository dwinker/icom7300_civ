civ: civ.c
	gcc -o civ civ.c

.PHONY: all
all: civ

.PHONY: clean
clean:
	rm -f *~ *.o civ a.out
