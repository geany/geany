template Template(alias a, T...)
if (is(typeof(a)))
{
private:
    // no parent:
    alias TemplateAlias = a!T;
    int memb;
}

Foo!x b;
Foo!(x) c; // FIXME
Foo!(x < 2) d; // FIXME

template each(alias fun = "a")
{
    // FIXME
    int zero(alias a) = 0; // should be var not func
    alias foo(T) = T; // T
    enum isRangeUnaryIterable(R) = // R
        is(typeof(unaryFun!fun(R.init.front)));

    template child(){}
    void tmethod()(){}
}

mixin ImplementLength!source; // source too!
