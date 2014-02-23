module HasInterfaces
  ! INTERFACE blocks can have names
  ! usually only used for when overloading, but good practice anyway!
  interface MyFunc
    function Func1(arg)
      integer :: arg
    end function Func1

    function Func2(arg)
      integer :: arg
    end function Func2
  end interface MyFunc

  ! without a name we just use a generic one
  interface
    subroutine CHEEV(...)
    ! ...
    end subroutine CHEEV
  end interface

contains

  function Func1(arg)
    integer :: arg, func1
    func1 = arg
  end function Func1

  function Func2(arg)
    integer :: arg, func2
    func2 = arg
  end function Func2

end module HasInterfaces

program Main
  use HasInterfaces

  ! can also occur in PROGRAMs
  interface MySubroutine
    subroutine Sub1(arg)
    ! ...
    end subroutine Sub1
  end interface MySubroutine

end program Main
