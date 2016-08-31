#include
! preprocessor directives on line 1 (and only line 1) cause breakage
module Invisible

  integer :: nope

contains

  function SpillsOutside
  ! ...
  end function SpillsOutside

end module Invisible

program Main
! ...
end program Main
