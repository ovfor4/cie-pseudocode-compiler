#include <cstdint>
#include <iostream>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        std::int64_t a, b;
        if (!(std::cin >> a >> b) || b == 0) return 1;
        auto small = a % 1000001;
        std::cout << (a < b ? "TRUE" : "FALSE") << ':' << a / b << ':' << a % b
                  << ':' << small + b << ':' << small - b << ':' << small * b << '\n';
    }
}
