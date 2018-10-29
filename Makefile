all: wc4

wc4: wc4.c
	gcc wc4.c -o wc4 -Wno-incompatible-library-redeclaration

clean:
	rm -f wc4
