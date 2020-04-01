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

version_str = 0.1.0
name_str = cargame

INCS = -I. -I/usr/include
LIBS = -lncursesw -lrt
CPPFLAGS = -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_DEFAULT_SOURCE \
	   -I/usr/include/ncursesw \
	   -DVERSION_STR=\"$(version_str)\"\
	   -DNAME_STR=\"$(name_str)\"

CFLAGS = -std=c99 -pedantic -Wall -Wextra -O2 $(INCS) $(CPPFLAGS)
CFLAGS_DEBUG = -ggdb -std=c99 -pedantic -Wall -Wextra -Og $(INCS) $(CPPFLAGS)
CFLAGS_DEBUG_NO_OPT = -ggdb -std=c99 -pedantic -Wall -Wextra -O0 $(INCS) $(CPPFLAGS)

LDFLAGS = $(LIBS)
CC = gcc

#prefix_dir = /usr/local
prefix_dir = ~/.local
