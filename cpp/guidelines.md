
* Don't define a default constructor that only initializes data members; use in-class member initializers instead
* Prefer in-class initializers to member initializers in constructors for constant initializers


// Bad

```cpp
class Simple
{
public:
    Simple() : a(1), b(2), c(3) {}
    Simple(int aa, int bb, int cc=-1) : a(aa), b(bb), c(cc) {}
    Simple(int aa) { a = aa; b = 0; c=0; }
private:
    int a;
    int b;
    int c;
};
```

// Good

```cpp
class Simple
{
public:
    Simple() = default;    Simple(int aa, int bb, int cc) : a(aa), b(bb), c(cc) {}
    Simple(int aa) : a(aa) {}
private:
    int a = -1;
    int b = -1;
    int c = -1;
};

```

* No arguing about “equivalent” ways to do it
* May prevent some bugs
* May put you back in “compiler generates constructors” land
* Potentially marginally faster in some circumstances

* Where there is a choice, prefer default arguments over overloading.

```cpp

```