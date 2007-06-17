CFLAGS=		-fPIC -Wall -g -DHAVE_CONFIG_H 
LIBOBJS=	list.o xml.o xin.o xout.o util.o isp.o error.o 
LIBOBJS+=	init.o unit.o handle.o
LIB=		libisp.a
DSO=		libisp.so

all: $(LIB) $(DSO)

$(LIB): $(LIBOBJS)
	ar rv $@ $(LIBOBJS)

$(DSO): $(LIBOBJS)
	gcc -shared -o libisp.so $(LIBOBJS)

clean:
	rm -f $(LIB) *.o *.so
