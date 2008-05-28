#!/usr/bin/env perl

#    convert_version.pl: generate a version integer from a version text
#
#    Copyright (C) 2006  Ludovic Rousseau  <ludovic.rousseau@free.fr>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#    02110-1301 USA.

use warnings;
use strict;

# convert "1.2.3-svn-xyz" in "0x01020003"
my ($major, $minor, $patch) = split /\./, $ARGV[0];

# remove the -svn-xyz part if any
$patch =~ s/-.*//;

printf "0x%02X%02X%04X\n", $major, $minor, $patch;

