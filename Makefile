# cargame - a simple curses car game
# Copyright (C) 2018-2020 Robert Imschweiler
#
# This file is part of cargame.
#
# cargame is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# cargame is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with cargame.  If not, see <https://www.gnu.org/licenses/>.
.POSIX:

include config.mk

bin = cargame
hdr = config.h
src = cargame.c
obj = ${src:.c=.o}
files = cargame.c config.h COPYING README.md Makefile example_level_file

all: $(bin)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(bin)
debug_no_opt: CFLAGS = $(CFLAGS_DEBUG_NO_OPT)
debug_no_opt: $(bin)

$(bin): $(obj)
	$(CC) $(LDFLAGS) -o $(bin) $^

%.o: %.c $(hdr)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(bin) $(obj)

dist:
	tar -czvf $(bin)_$(version_str).orig.tar.gz $(files)

install:
	install $(bin) $(DESTDIR)$(prefix_dir)/bin
	install $(bin).1 $(DESTDIR)$(prefix_dir)/share/man/man1
	gzip $(DESTDIR)$(prefix_dir)/share/man/man1/$(bin).1

.PHONY: all clean dist install
