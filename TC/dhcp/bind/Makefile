#
# Copyright (C) 2009-2010 by Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $Id: Makefile.bind,v 1.2.2.7 2011-04-06 22:00:40 marka Exp $

# Configure and build the bind libraries for use by DHCP

include ./version.tmp
version=${MAJORVER}.${MINORVER}.${PATCHVER}${RELEASETYPE}${RELEASEVER}

# bindvar.tmp is constructed by configure, it has the paths for
# if GMAKE is blank the shell script couldn't find a gmake to use.
# binddir=
# GMAKE=
include ./bindvar.tmp

bindsrcdir=bind-${version}

all:
# Extract the source from the tarball, if it hasn't been already.
	@if test -d ${bindsrcdir} ; then                    \
		echo ${bindsrcdir} already unpacked... ;    \
	else                                                \
		gunzip -c bind.tar.gz | tar xf - ;          \
	fi

	@if test -z "${GMAKE}"; then                        \
		echo "unable to find gmake" 1>&2 ;          \
		exit 1;                                     \
	fi

# Configure the export libraries
# Currently disable the epoll and devpoll options as they don't interact
# well with the DHCP code.
	@echo Configuring BIND Export libraries for DHCP.
	@(cd ${bindsrcdir} && ./configure --disable-kqueue --disable-epoll --disable-devpoll --without-openssl --without-libxml2 --enable-exportlib --enable-threads=no --with-export-includedir=${binddir}/include --with-export-libdir=${binddir}/lib --with-gssapi=no > ${binddir}/configure.log)

# Build the export libraries
	@echo Building BIND Export libraries - this takes some time.
	@(cd ${bindsrcdir}/lib/export ; \
	  echo building in `pwd` ; \
	  MAKE=${GMAKE} ${GMAKE} > ${binddir}/build.log)

	@echo Installing BIND Export libraries to ${binddir}.
	@(cd ${bindsrcdir}/lib/export ; \
	  MAKE=${GMAKE} ${GMAKE} install > ${binddir}/install.log)

clean:
	@echo Cleaning BIND export library.
	rm -rf ${bindsrcdir} ./lib ./include ./configure.log ./build.log \
		./install.log

# Include the following so that this Makefile is happy when the parent
# tries to use them.

distdir:

distclean:

install:

check:

