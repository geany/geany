typedef int T;

int func1(\
int var1, int var2, ...);

int func2(int var1, \
int var2, ...);

int func3(T \
          var1, T var2, ...);
int func4(T var1, \
          T var2, ...);

int func5(int \
          var1, int var2, ...);

// check continuations in macros are properly handled
#define MACRO1 \
  (
int func6(void);
#define MACRO2(x) \
  int x(void);
#define MACRO3(y) \
  int \
  y \
  ( \
    void \
  );
int func7(int a);
