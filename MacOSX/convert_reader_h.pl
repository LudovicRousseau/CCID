#!/usr/bin/env perl

#    convert_reader_h.pl: convert reader.h.in in reader.h with
#
#    Copyright (C) 2008  Ludovic Rousseau  <ludovic.rousseau@free.fr>
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
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

use warnings;
use strict;

my $text = 
"#ifdef __BIG_ENDIAN__
#define HOST_TO_CCID_16(x) ((((x) >> 8) & 0xFF) + ((x & 0xFF) << 8))
#define HOST_TO_CCID_32(x) ((((x) >> 24) & 0xFF) + (((x) >> 8) & 0xFF00) + ((x & 0xFF00) << 8) + (((x) & 0xFF) << 24))
#else
#define HOST_TO_CCID_16(x) (x)
#define HOST_TO_CCID_32(x) (x)
#endif
";

while (<>)
{
	if (m/host_to_ccid_16/)
	{
		print $text;
		<>;
		next;
	}
	print;
}
