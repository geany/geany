m = module Foo
end

m += module Quux
end

c = class Bar
  x = def method_a
    if 1
    else
    end
  end

  def method_b
    x = while 1 do
      break
    end
  end

  def method_c
    x = if 1
    else
    end
  end

  def method_d
    x += if 1
    else
    end
  end

  def method_e
    x ||= if 1
    else
    end
  end

  def method_f
  end
end

c ||= class Zook
end
