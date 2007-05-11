# Makefile for domhub-tools/domapp
# Jacobsen
# $Id: Makefile,v 1.17 2007/03/26 17:01:08 jacobsen Exp $

INSTALL_BIN  = /usr/local/bin
INSTALL_CONF = /usr/local/share

BINS = domapptest decomp decodemoni decodeeng decodesn decodedelta

all:
	make $(BINS)

decodemoni: decodemoni.c domapp.h
	gcc -Wall -o decodemoni decodemoni.c

decodeeng: decodeeng.c bele.h
	gcc -Wall -o decodeeng -I ../devel/domhub-tools decodeeng.c

domapptest: domapptest.c domapp.h
	gcc -Wall -o domapptest domapptest.c

decomp: decomp.c
	gcc -Wall -o decomp decomp.c

decodesn: decodesn.c
	gcc -Wall -o decodesn decodesn.c

decodedelta: decodedelta.c
	gcc -Wall -o decodedelta decodedelta.c

rpm:
	./dorpm `cat domapp-tools-version`

clean:
	rm -f $(BINS)

install:
	install domapp-tools-version $(INSTALL_CONF)
	install domapptest           $(INSTALL_BIN)
	install upload_domapp.pl     $(INSTALL_BIN)
	install uda.pl               $(INSTALL_BIN)
	install decomp               $(INSTALL_BIN)
	install decodemoni           $(INSTALL_BIN)
	install decodeeng            $(INSTALL_BIN)
	install decodesn             $(INSTALL_BIN)
	install domapp_multitest.pl  $(INSTALL_BIN)
	install domapp-versions      $(INSTALL_BIN)
	ln -f -s $(INSTALL_BIN)/domapp_multitest.pl $(INSTALL_BIN)/dmt
