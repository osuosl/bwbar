## $Id: Makefile,v 1.7 2006/08/26 16:37:17 hpa Exp $
## -----------------------------------------------------------------------
##   
##   Copyright 2001-2006 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

PROGS = bwbar

-include MCONFIG

all:	$(PROGS)

clean:
	rm -f *.o $(PROGS)

distclean: clean
	autoconf
	rm -f *~ \#* core MCONFIG config.*
	rm -rf autom4te.cache

bwbar: bwbar.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

MCONFIG: config.status MCONFIG.in
	./config.status

config.status: configure
	./configure

configure: configure.in
	autoconf


