
const volatile unsigned int func1();
const volatile unsigned int var1 = 0;

struct type1 {
	unsigned int memb1;
	struct type1 *next;
};

const struct type1 func2();
const struct type1 var2 = { 0, 0 };

typedef type1 type1_t;

const type1_t func3();
const type1_t var3 = { 0, 0 };

struct type1 func4();
struct type1 var4 = { 0, 0 };

type1_t func5();
type1_t var5 = { 0, 0 };

typedef unsigned long int type2_t;

/* scoped stuff */

#include <string>

const std::string func6();
const std::string var6 = "hello";

std::string func7();
std::string var7 = "hello";

/* this shows a different bug in the parser, adding scope std to the symbol.
 * ignore this for now.
std::string const func8();
std::string const var8 = "hello";
*/
