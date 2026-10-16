#include <fstream>
#include <iostream>
#include <string>
int main() {
    int count;
    if (!(std::cin >> count)) return 1;
    for (int i = 0; i < count; ++i) {
        std::string text, extra;
        if (!(std::cin >> text >> extra)) return 1;
        { std::ofstream file("roundtrip.txt"); file << text << "\n\n"; if (!file) return 1; }
        { std::ofstream file("roundtrip.txt", std::ios::app); file << extra << '\n'; if (!file) return 1; }
        std::ifstream file("roundtrip.txt");
        if (!file) return 1;
        std::string line, combined;
        int lines = 0;
        while (std::getline(file, line)) { combined += '[' + line + ']'; ++lines; }
        if (!file.eof()) return 1;
        std::cout << lines << ':' << combined << '\n';
    }
}
