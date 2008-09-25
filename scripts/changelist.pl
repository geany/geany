#!/usr/bin/env perl
# Copyright:	2008, Nick Treleaven
# License:		GNU GPL V2 or later
# Warranty:		NONE

# Searches a ChangeLog file for a line matching 'matchstring', then matches
# all lines until two consecutive empty lines are found. The process then
# repeats until all matching blocks of text are found.
# Results are printed in reverse, hence in chronological order (as ChangeLogs
# are usually written in reverse date order).

use strict;
use warnings;

my $scriptname = "changelist.pl";
my $argc = $#ARGV + 1;

($argc == 2)
	or die "Usage:\n$scriptname matchstring changelogfile\n";

my ($matchstr, $infile) = @ARGV;

open(INPUT, $infile)
	or die "Couldn't open $infile for reading: $!\n";

my $entry;	# the current matching block of text
my @entries;
my $found = 0;	# if we're in a matching block of text
my $blank = 0;	# whether the last line was empty

while (<INPUT>) {
	my $line = $_;	# read a line, including \n char

	if (! $found) {
		($line =~ m/$matchstr/) and $found = 1;
	} else {
		if (length($line) <= 1)	# current line is empty
		{
			if ($blank > 0) {	# previous line was also empty
				push(@entries, $entry);	# append entry
				$entry = "";
				$found = 0;	# now look for next match
				$blank = 0;
			}
			else {
				$blank = 1;
			}
		}
	}
	$found and $entry .= $line;
}
close(INPUT);

foreach $entry (reverse @entries) {
	print "$entry\n\n";
}
