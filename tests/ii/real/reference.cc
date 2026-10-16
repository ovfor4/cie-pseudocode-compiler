#include <iostream>
const char *boolean(bool value) { return value ? "TRUE" : "FALSE"; }
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        int a, b;
        if (!(std::cin >> a >> b)) return 1;
        double x = a / 8.0, y = b / 8.0;
        std::cout << boolean(x < y) << ':' << boolean(x == y) << ':'
                  << boolean(2.0 * x == a / 4.0) << ':'
                  << boolean(x + y == (a + b) / 8.0) << ':'
                  << boolean(x * y == (a * b) / 64.0) << '\n';
    }
}
