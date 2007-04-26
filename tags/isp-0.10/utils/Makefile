PWD=	$(shell pwd)

CFLAGS=	-Wall -g -I..
LDADD=	../isp/libisp.a -lexpat -lssl
PROGS=	ispcat ispexec ispbarrier isprename ispunit ispunitsplit \
	ispstats isprun ispprogress ispcount ispdelay
DEPS=	../isp/libisp.a

all: $(PROGS)

ispcat: ispcat.o $(DEPS)
	$(CC) -o $@ ispcat.o $(LDADD)

ispexec: ispexec.o $(DEPS)
	$(CC) -o $@ ispexec.o $(LDADD)

ispbarrier: ispbarrier.o $(DEPS)
	$(CC) -o $@ ispbarrier.o $(LDADD)

isprename: isprename.o $(DEPS)
	$(CC) -o $@ isprename.o $(LDADD)

ispstats: ispstats.o $(DEPS)
	$(CC) -o $@ ispstats.o $(LDADD) -lm

ispunit: ispunit.o $(DEPS)
	$(CC) -o $@ ispunit.o $(LDADD)

isprun: isprun.o $(DEPS)
	$(CC) -o $@ isprun.o $(LDADD)

ispunitsplit: ispunitsplit.o $(DEPS)
	$(CC) -o $@ ispunitsplit.o $(LDADD)

ispprogress: ispprogress.o progress.o $(DEPS)
	$(CC) -o $@ ispprogress.o progress.o $(LDADD)

ispcount: ispcount.o $(DEPS)
	$(CC) -o $@ ispcount.o $(LDADD)

ispdelay: ispdelay.o $(DEPS)
	$(CC) -o $@ ispdelay.o $(LDADD)

clean:
	rm -f $(PROGS) *.o
