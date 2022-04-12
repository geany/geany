--  Taken from #2925 submitted by @koenmeersman
package My_Package is

  generic
    type Num is digits <>;
  package Conversions is
    function From_Big_Real (Arg : Integer) return Num;
  end Conversions;

  type Missing_Tag is record
    Num : Integer;
  end record;

  type Primary_Color is (Red, Green, Blue);

end My_Package;
