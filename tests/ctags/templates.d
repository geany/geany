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
void f(Foo!x);

template each(alias fun = "a")
{
    template child(){}
    void tmethod()(){}
}

// FIXME
int VT(alias a) = 0; // parsed as function
/+
// parsed as T:
alias AT(T) = T;
enum ET(T) = T.init;
+/

mixin ImplementLength!source; // source too!

// declaration templates
interface IT(T){}
struct ST(T){}
union UT(T){}

