#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

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

int main(){
	std::string word;
	std::string file;
	
	std::cout << "Enter the file to search: ";
	std::getline(std::cin, file);
	std::cout << "Enter the word to search: ";
	std::getline(std::cin, word);
	
	std::ifstream inFile(file);
	if (!inFile){
		std::cerr << "File not found\n";
		return 1;
	}
	
	std::string line;
	int lineNumber = 0;
	int hits = 0;
	
	while (std::getline(inFile, line)){
		++lineNumber;
		
		if(containsWord(line, word)){
			int res = line.find("!");
			if(res == string::npos){
				std::cout << lineNumber << ". " << line << "\n";
				++hits;
			}
		}
	}
	
	std::cout << "Total " << hits << " line(s)\n";

	return 0;
}