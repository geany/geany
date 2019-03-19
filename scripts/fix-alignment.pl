#!/usr/bin/env perl
# Copyright:	2009 The Geany contributors
# License:		GNU GPL V2 or later, as published by the Free Software Foundation, USA.
# Warranty:		NONE

# Re-align C source code for Geany.
# Doesn't handle indents/blank lines/anything complicated ;-)

use strict;
use warnings;

use Getopt::Std;

my %args = ();
getopts('w', \%args);
my $opt_write = $args{'w'};

my $argc = $#ARGV + 1;
my $scriptname = $0;

(($argc == 1) or ($argc >= 1 and $opt_write)) or die <<END;
Usage:
$scriptname sourcefile [>outfile]
  Print formatted output to STDOUT or outfile.
  Warning: do not use the same file for outfile.

$scriptname -w sourcefile(s)
  Writes to the file(s) in-place.
  Warning: backup your file(s) first or use clean version control files.
END


sub parse($)
{
	my ($infile) = @_;
	my @lines;

	open(INPUT, $infile) or die "Couldn't open $infile for reading: $!\n";

	while (<INPUT>) {
		my $line = $_;	# read a line, including \n char

		# strip trailing space & newline
		$line =~ s/\s+$//g;

		# for now, don't process lines with comments, strings, preproc non-defines, overflowed lines or chars
		# multi-line comment internal lines are skipped only if they start with '* '.
		if (!($line =~ m,/\*|\*/|//|"|\\$|',) and !($line =~ m/^\s*(\*\s|#[^d])/)) {
			# make binary operators have *one* space each side
			# operators must have longer variants first otherwise trailing operators can be broken e.g. "+ ="
			# '*' ignored as could be pointer
			my $ops = '<<=,<<,>>=,>>,<=,>=,<,>,||,|=,|,&&,&=,-=,+=,+,*=,/=,/,==,!=,%=,%,^=,^,=';
			$ops =~ s/([|*+])/\\$1/g; # escape regex chars
			$ops =~ s/,/|/g;
			$line =~ s/([\w)\]]) ?($ops) ?([\w(]|$)/$1 $2 $3/g;

			# space binary operators that can conflict with unaries with cast and/or 'return -1/&foo'
			# '-' could be unary "(gint)-j"
			# '&' could be address-of "(type*)&foo"
			$line =~ s/([\w\]])(-|&) ?([\w(]|$)/$1 $2 $3/g;

			# space ternary conditional operator
			$line =~ s/ ?\? ?(.+?) ?: ?/ ? $1 : /g;

			# space comma operator (allowing for possible alignment space afterwards)
			$line =~ s/ ?,(\S)/, $1/g;

			# space after statements
			my $statements = 'for|if|while|switch';
			$line =~ s/\b($statements)\b\s*/$1 /g;

			# no space on inside of brackets
			$line =~ s/\(\s+/(/g;
			$line =~ s/(\S)\s+\)/$1)/g;

			# enforce 'fn(void);' in prototypes
			$line =~ s/^(\w.+\w\()\);$/$1void);/;
		}
		# strip trailing space again (e.g. a trailing operator now has space afterwards)
		$line =~ s/\s+$//g;

		push(@lines, $line);
	}
	close(INPUT);

	my $text = join("\n", @lines);
	undef @lines;	# free memory
	$text .= "\n";

	# 1+ newline -> 2 newlines after function
	$text =~ s/^}\n\n+([^\n])/}\n\n\n$1/gm;

	if (!$opt_write) {
		print $text;
	}
	else {
		open(OUTPUT, ">$infile") or die "Couldn't open $infile for writing: $!\n";
		print OUTPUT $text;
		close(OUTPUT);
	}
}


foreach my $infile (@ARGV)
{
	parse($infile);
}

