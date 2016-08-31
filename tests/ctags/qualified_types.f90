module test
  implicit none
  type goodtype(p1, p2, p3, p4) ! the stuff in brackets after the name of the type shouldn't appear in the type's name
                                ! this is already correctly handled, so that's fine
    integer, kind     :: p1, p3 
    integer, len      :: p2, p4 ! the question is whether or not these "kind" and "len"s should be shown as members
    real(kind=p1)     :: c1
    character(len=p2) :: c2
    complex           :: c3(p3)
    integer           :: c4 = p1
contains
end module test
