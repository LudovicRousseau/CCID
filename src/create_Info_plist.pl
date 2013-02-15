#!/usr/bin/env perl

#    create_Info_plist.pl: generate Infor.plist from a template and a
#    list of suported readers
#
#    Copyright (C) 2004-2009  Ludovic Rousseau  <ludovic.rousseau@free.fr>
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
use Getopt::Long;

my (@manuf, @product, @name);
my ($manuf, $product, $name);
my $target = "libccid.so";
my $version = "1.0.0";
my $class = "<key>CFBundleName</key>
	<string>CCIDCLASSDRIVER</string>";
my $noclass = 0;

GetOptions(
	"target=s" => \$target,
	"version=s" => \$version,
	"no-class" => \$noclass);

if ($#ARGV < 1)
{
	print "usage: $0 supported_readers.txt Info.plist
	--target=$target
	--version=$version\n";
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
	$name =~ s/&/&amp;/g;
	push @name, $name
}
close IN;

map { $_ = "\t\t<string>$_</string>\n" } @manuf;
map { $_ = "\t\t<string>$_</string>\n" } @product;
map { $_ = "\t\t<string>$_</string>\n" } @name;

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
	if (m/MAGIC_TARGET/)
	{
		s/MAGIC_TARGET/$target/;
		print;
		next;
	}
	if (m/MAGIC_VERSION/)
	{
		s/MAGIC_VERSION/$version/;
		print;
		next;
	}
	if (m/MAGIC_CLASS/)
	{
		next if ($noclass);

		s/MAGIC_CLASS/$class/;
		print;
		next;
	}
	print;
}

close IN;

