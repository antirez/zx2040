all: uf2-append uf2-ls

uf2-append: uf2-append.c
	$(CC) -O2 -Wall -W uf2-append.c -o uf2-append

uf2-ls: uf2-ls.c
	$(CC) -O2 -Wall -W uf2-ls.c -o uf2-ls

clean:
	rm -f uf2-append uf2-ls
