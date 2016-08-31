module Blocks
  implicit none

  integer :: i

contains

  function MyFunc(thing)
  integer, intent(in) :: thing
  integer             :: myfunc

  block
    ! these obviously won't show up, because variables inside functions inside modules don't anyway
    ! and that's fine
    integer :: tempint
    tempint = 1
    if (tempint == 1) then
      tempint = 2
    end if
    myfunc = tempint
  end block

  end function MyFunc

end module Blocks

program Main
  use Blocks

  implicit none

  ! these variable declarations should definitely show up in the symbol list
  integer :: otherint, moreint

  otherint = 3

  block
    ! These shouldn't, because...
    integer :: newtempint

    newtempint = 2
    block
      ! ...the damn things can be arbitrarily nested!
      integer :: newer_int

      newer_int = 3
    end block
    newtempint = 2 * newtempint
  end block

end program Main
