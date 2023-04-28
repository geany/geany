static if (is(typeof(__traits(getMember, a, name)) == function))
	T conditional;

static if (1) {
	T conditional_block; // FIXME
}

struct Struct
{
	static if (1) {
		T conditional_member; // FIXME
	}
}

