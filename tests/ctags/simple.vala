class Demo.HelloWorld : GLib.Object {

	/* atomic types */
	unichar c = 'u';
	float percentile = 0.75f;
	const double MU_BOHR = 927.400915E-26;
	bool the_box_has_crashed = false;

	/* defining a struct */
	struct Vector {
		public double x;
		public double y;
		public double z;
	}

	/* defining an enum */
	enum WindowType {
		TOPLEVEL,
		POPUP
	}

	public signal void sig_1(int a);

	private int _age = 32;  // underscore prefix to avoid name clash with property

	/* Property */
	public int age {
		get { return _age; }
		set { _age = value; }
	}

	public static int main(string[] args) {
		stdout.printf("Hello, World\n");

		return 0;
	}
}

public interface ITest : GLib.Object {
    public abstract int data_1 { get; set; }
    public abstract void method_1();
}

namespace NameSpaceName {
    class Foo {}
}