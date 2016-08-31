# This file is intended to test package keyword support along with
# multi-line subroutine definitions, like this:
#   sub
# 
#   somefunction
#   {...

package Test::One;

package Test::One::Two;

package Test;

use strict;
use warnings;

sub

 init {
}

use constant
CONST => 1;

=head2 quo

This is 'quo' subroutine

=cut

sub quo;

quo;

Test::quo;

sub quo {
    goto END;
    die;
END:
END2  :
    print "END!\n";
    return;
}

sub xuz :lvalue;

sub xtz (&@;$) :lvalue;
  
sub
# This should create a tag
dude
{
    1;
}

1;
