
class A
  def a
    dummy = "\n while true do \n"
  end
  def b
    dummy = %Q{\n while true do \n}
  end
  def c
    dummy = %{\n while true do \n}
  end
  def d
    dummy = %/\n while true do \n/
  end
  
  # raw strings
  def e
    dummy = ' while true do '
  end
  def f
    dummy = %q{ while true do }
  end
  
  # sentinel for last test
  def sentinel
  end
end


v = "
def not_me1
  print \"i'm not a function\"
end
"
print v, "\n"
v = %Q{
def not_me2
  print "i'm not a function"
end
}
print v, "\n"
v = %{{
def not_me3
  print "i'm not a function"
end
}
print v, "\n"
v = %/
def not_me4
  print "i'm not a function"
end
\/
/
print v, "\n"
# raw strings
v = '
def not_me5
  print "i\'m not a function"
end
'
print v, "\n"
v = %q{
def not_me5
  print "i'm not a function"
end
\}
}
print v, "\n"
