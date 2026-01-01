## Makefile.am - template for generating Makefile via Automake
##
## Copyright (C) 2006-2014, 2016-2017, 2020-2025 Free Software
## Foundation, Inc.
##
## This file is part of GNU M4.
##
## GNU M4 is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## GNU M4 is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <https://www.gnu.org/licenses/>.
##
## This file written by Eric Blake <ebb9@byu.net>

AM_DISTCHECK_DVI_TARGET = pdf
# EXTRA_DIST = m0.pdf

info_TEXINFOS = m0.texi m0algo.texi
m0_TEXINFOS = fdl-1.3.texi gpl-3.0.texi sds.texi chap_builtin.texi chap_programs.texi m4emu.texi
m0algo_TEXINFOS = fdl-1.3.texi gpl-3.0.texi

m4emu.texi: $(builddir)/../emu/m4emu.texi
	cp $(builddir)/../emu/m4emu.texi .


man_MANS = $(srcdir)/m0.1
EXTRA_DIST = $(man_MANS) gendocs_template
MAINTAINERCLEANFILES = $(man_MANS) gendocs_template
SUFFIXES = .1

# The customization variable CHECK_NORMAL_MENU_STRUCTURE is necessary with
# makeinfo versions ≥ 6.8.
AM_MAKEINFOFLAGS = -c CHECK_NORMAL_MENU_STRUCTURE=1

HELP2MAN = $(SHELL) $(top_srcdir)/missing --run help2man

# Depend on ../.version for version, m0.c for usage text.  Do not depend on
# built m0 executable, since not everyone has help2man or perl.
# Build the man page once in the srcdir, rather than in every VPATH build
# dir, to match how automake builds info pages.  This is safe for 'make
# distcheck' since it is distributed pre-built.
$(srcdir)/m0.1:  $(top_srcdir)/src/main.c
	@if test -x ../src/m0$(EXEEXT) ; then \
	  echo "Updating man page m0.1" ; \
	  $(HELP2MAN) --name="a flexible macro processor" --source='$(PACKAGE_STRING)' \
	    --info-page=m0 --output=$@ ../src/m0$(EXEEXT) ; \
	else \
	  echo "WARNING: The \`man' page \`$@' cannot be updated yet."; \
	  echo "         Retry once the program executable is ready."; \
	fi
