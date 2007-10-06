# This Makefile presumes isp is installed in /usr/lib and /usr/include.

all: hello

hello: hello.o
	$(CC) -o $@ $< -lisp -lexpat -lssl

clean:
	rm -f hello.o hello
