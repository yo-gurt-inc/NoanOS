#include <cstdio>

class Greeter {
    const char* name;
public:
    Greeter(const char* n) : name(n) {}
    void greet() const { printf("Hello from C++, %s!\n", name); }
};

int main() {
    Greeter g("NoanOS");
    g.greet();
    return 0;
}
