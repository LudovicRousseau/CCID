#!/usr/bin/env perl

#    create_Info_plist.pl: generate Infor.plist from a template and a
#    list of suported readers
#
#    Copyright (C) 2004  Ludovic Rousseau  <ludovic.rousseau@free.fr>
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use warnings;
use strict;

my (@manuf, @product, @name);
my ($manuf, $product, $name);
my $ifdCapabilities;

if ($#ARGV ne 2)
{
	print "usage: $0 supported_readers.txt Info.plist ifdCapabilities\n";
	exit;
}

open IN, "< $ARGV[0]" or die "Can't open $ARGV[0]: $!";
while (<IN>)
{
	next if (m/^#/);
	next if (m/^$/);

	chomp;
	($manuf, $product, $name) = split /:/;
	# print "m: $manuf, p: $product, n: $name\n";
	push @manuf, $manuf;
	push @product, $product;
	push @name, $name
}
close IN;

map { $_ = "\t\t<string>$_</string>\n" } @manuf;
map { $_ = "\t\t<string>$_</string>\n" } @product;
map { $_ = "\t\t<string>$_</string>\n" } @name;

$ifdCapabilities=$ARGV[2];

open IN, "< $ARGV[1]" or die "Can't open $ARGV[1]: $!";

while (<IN>)
{
	if (m/MAGIC_VENDOR/)
	{
		print @manuf;
		next;
	}
	if (m/MAGIC_PRODUCT/)
	{
		print @product;
		next;
	}
	if (m/MAGIC_FRIENDLYNAME/)
	{
		print @name;
		next;
	}
	if (m/MAGIC_IFDCAPABILITIES/)
	{
		s/MAGIC_IFDCAPABILITIES/$ifdCapabilities/;
		print;
		next;
	}
	print;
}

close IN;

