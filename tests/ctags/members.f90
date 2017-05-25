module Members
  implicit none

  type HasMembers
    ! a "derived type" in Fortran is analogous to a "class" in other languages
    integer, kind :: kind_member
    integer, len  :: len_member
    integer       :: member
  contains
    procedure :: MyMethod
  end type HasMembers

contains

  subroutine MySubroutine(arg)
  ! ...
  end subroutine MySubroutine

end module Members
