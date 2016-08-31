#!/usr/bin/env perl
# fix-cxx-comments.pl
# Copyright (c) 2008, Daniel Richard G. <skunk(at)iskunk(dot)org>
#
#       Redistribution and use in source and binary forms, with or without
#       modification, are permitted provided that the following conditions are
#       met:
#
#       * Redistributions of source code must retain the above copyright
#         notice, this list of conditions and the following disclaimer.
#       * Redistributions in binary form must reproduce the above
#         copyright notice, this list of conditions and the following disclaimer
#         in the documentation and/or other materials provided with the
#         distribution.
#       * Neither the name of the author nor the names of
#         contributors may be used to endorse or promote products derived from
#         this software without specific prior written permission.
#
#       THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#       "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#       LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#       A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#       OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#       SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#       LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#       DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#       THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#       (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#       OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use warnings;

sub xform($$$)
{
	my ($a, $b, $c) = @_;

	$a =~ s!///!/**!;
	$b =~ s!(\a\s*)///!$1 **!g;
	$c =~ s!///! **!;

	$a =~ s!//!/*!;
	$b =~ s!(\a\s*)//!$1 *!g;
	$c =~ s!//! *!;

	$c .= ' */';

	return $a . $b . $c . "\a";
}

sub fix_cxx_comments()
{
	s/\r//g;	# This is Unix, not DOS
	s/[\t ]+$//g;	# Remove trailing whitespace
	s/\n/\a/g;	# Convert file to single line

	# Process multi-line comments:
	# $1 = first line of comment, $3 = last line, $2 = lines in between
	s! (\a\s*//[^\a]*) ((?:\a\s*//[^\a]*)*) (\a\s*//[^\a]*)\a !xform($1,$2,$3)!egx;

	# Process /// single-line comments
	s! (\a\s*)///(\s?)([^\a]*)\a !$1/**$2$3$2*/\a!gx;
	s! (\s+)///(\s?)([^\a]*)\a !$1/**$2$3$2*/\a!gx;

	# Process // single-line comments
	s! (\a\s*)//(\s?)([^\a]*)\a !$1/*$2$3$2*/\a!gx;
	s! (\s+)//(\s?)([^\a]*)\a !$1/*$2$3$2*/\a!gx;

	s/\a/\n/g;	# Convert back to multiple lines
}

{
	local $/;
	$_ = <>;
	fix_cxx_comments();
	print;
}

