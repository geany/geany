module Enums
  real :: somevar

  ! we now have enumerators in F2003/8, for the sake of interop with C
  enum, bind(c) ! unnamed 1
    enumerator :: red =1, blue
    enumerator gold, silver, bronze
    enumerator :: purple
  end enum


  ! here follow nonstandard enum declarations, which may become valid in a later standard
  ! no real harm implementing these as long as valid stuff isn't broken
  enum
    enumerator :: no_c_binding
  end enum

  enum :: Colons
    enumerator :: r
  end enum

  enum BodyPart
    enumerator :: arm, leg
  end enum

  enum(8) Paren_kind
    enumerator :: b
  end enum

  enum*8 Aster_kind
    enumerator :: c
  end enum

  enum(8) :: Paren_colon
    enumerator :: d
  end enum

  enum*8 :: Aster_colon
    enumerator :: e
  end enum

  enum, bind(c) :: Name_colon
    enumerator :: d
  end enum

  ! another entry to verify the parsing hasn't broken
  real, parameter :: othervar

contains

  function Func(arg)
  ! ...
  end function Func

end module Enums
