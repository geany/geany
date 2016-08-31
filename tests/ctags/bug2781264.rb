
class Foo
  def foo
    File.open("foo", "r") do |infile|
      infile.readline
    end
  end

  def bar
    print "bar"
  end
end
