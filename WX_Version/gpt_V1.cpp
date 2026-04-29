#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

bool containsWord(const std::string& line, const std::string& word) {
    std::istringstream iss(line);
    std::string token;

    while (iss >> token) {
        if (token == word) {
            return true;
        }
    }
    return false;
}

int main() {
    std::string word;

    std::cout << "Enter word to search: ";
    std::cin >> word;

    std::ifstream in("car.txt");
    if (!in) {
        std::cerr << "Error: could not open car.txt\n";
        return 1;
    }

    std::string line;
    int lineNo = 0;
    int hits = 0;

    while (std::getline(in, line)) {
        ++lineNo;

        if (containsWord(line, word)) {
            std::cout << lineNo << ": " << line << "\n";
            ++hits;
        }
    }

    std::cout << "Total " << hits << " line(s)\n";

    return 0;
}