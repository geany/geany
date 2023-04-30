module test.simple;

import std.stdio;

alias AliasInt = int;

struct Struct
{
	union Union
	{
		bool quxx;
		int qar;
	}
}

enum Enum : int
{
	foo,
	bar,
}

interface Interface
{
	public AliasInt bar();
}

class Class : Interface
{
	private AliasInt _bar;

	public this(AliasInt x)
	{
		this._bar = x;
	}

	public AliasInt bar()
	{
		return this._bar;
	}
	
	protected:
	auto tfun(T)(T v)
	{
		return v;
	}
}

public
{
	int modulevar;
}

Object obj;

private:
int i;

/+ 
int error;
 +/

@attr(i) int attr_decl = 1;
@attr(i) attr_decl_infer = 1; // FIXME
@(obj) T attr_anon;
void attr_post() @attr(obj); // FIXME

static assert( num < TL.length, "Name '"~name~"' is not found");

__gshared int globalVar;

T out_contract()
out(r; r > 0) {} do {}

void main(string[] args)
in(args.length > 0)
{
	auto foo = new Class(1337);

	alias string AliasString;
	AliasString baz = "Hello, World!";

	writefln("%s", foo.bar());
}

extern (C++, name) extern_func();
