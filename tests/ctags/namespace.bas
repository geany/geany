namespace first 'first defines something'
  sub first_func
  end sub

  namespace second 'second defines something'
    sub first_func  'oh a second first_func
    end sub

    sub second_func
    end sub
  end namespace 'ignored'
end namespace

sub first_func 'oh another first_func
end sub
