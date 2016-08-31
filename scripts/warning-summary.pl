#!/usr/bin/env perl
# warning-summary.pl
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

use encoding 'utf8';

binmode(STDIN, ":utf8");

open(OUT, "| sort | uniq -c | sort -nr");
binmode(OUT, ":utf8");

while (<>)
{
	/warning:/ || next;
	/near initialization for/ && next;
	/shadowed declaration is here/ && next;
	/\(this will be reported only once per input file\)/ && next;

	s/^.*: warning: //g;

	tr/\x{2018}\x{2019}/''/;

	s/\barg(ument|) \d+\b/arg$1 N/g;

	s/\b(type|function|variable|enumeration value|declaration of|argument N of|parameter|type of|type of bit-field|prototype for) '[^']+'/$1 'blah'/g;

	s/"[^"]+" (is not defined)/"BLAH" $1/g;

	s/'[^']+' (defined but not used)/'blah' $1/g;

	s/format '%\w+'/format '%blah'/g;

	s/'%\w+' printf format/'%blah' printf format/g;

	s/'\d+'/'NNN'/g;

	s/'[^']+' (declared 'static' but never defined)/'blah' $1/g;

	s/(missing braces around initializer for) '\w+'/$1 'blah'/g;

	s/(missing initializer for member) '\w+::\w+'/$1 'Foo::bar'/g;

	print OUT;
}

close(OUT);

# end warning-summary.pl

