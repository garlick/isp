PWD=    $(shell pwd)

CFLAGS= -Wall -g -I..
LDADD=  ../isp/libisp.a -lexpat -lssl
PROGS=  corruptfile srcxml sinkxml
DEPS=   ../isp/libisp.a

all: $(PROGS)

corruptfile: corruptfile.o $(DEPS)
	$(CC) -o $@ corruptfile.o $(LDADD)
srcxml: srcxml.o $(DEPS)
	$(CC) -o $@ srcxml.o $(LDADD)
sinkxml: sinkxml.o $(DEPS)
	$(CC) -o $@ sinkxml.o $(LDADD)

clean: testclean
	rm -f $(PROGS) a.out core *.o

testclean:
	rm -rf isptest.*

test: $(PROGS)
	./runtests.sh
