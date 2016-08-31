! For some reason, the Fortran standard does not prohibit this...

module Program

  type Data
    integer :: contents
  end type Data

  integer :: i

  interface Program
    function myFunc(arg)
      !...
    end function myFunc
  end interface Program

contains

  function MyFunc(arg)
  ! ...
  end function MyFunc

end module Program

program Interface
  use Program
  ! ...
end program Interface
