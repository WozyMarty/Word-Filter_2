#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

//Function to separate the linear text into "parts", as if a cin, needed to compare each full word with the user input
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
	
	int choice;
	cout << "Display options:\n";
	cout << "1. Full line\n";
	cout << "2. Only first two words\n";
	cout << "Choose: ";
	cin >> choice;
	cin.ignore(); 									// clear newline
	
	std::ifstream inFile(file);						// READ-Only the file (format needed to specify like "text.txt")
	if (!inFile){
		std::cerr << "File not found\n";
		return 1;
	}
	
	std::string line;
	int lineNumber = 0;
	int hits = 0;
	
	while (std::getline(inFile, line)){				// First parameter is the file with data insenting into the var "line"
		++lineNumber;
		
		if(containsWord(line, word)){
			int res = line.find("!"); 				// Remove " ! " (ABB comentary)
			if(res == string::npos){ 				// When not find " ! ", a error will appear and go to npos, the IF is to let the code runs
	
				if (choice == 1) {
					cout << lineNumber << ". " << line;
				} else if (choice == 2) {
					for (char &c : line) {			// FOR to change " , " with a blank space
						if (c == ',') c = ' ';
					}
					
					std::istringstream iss(line); 	// Change the full (not-truly) static text and endline after each blank space, separating the words
					std::string word1, word2;
					iss >> word1 >> word2; 			// Store the first word that endl, and then the second

					std::cout << lineNumber << ". " << word1;
					if (!word2.empty()) {
						std::cout << " " << word2;	// If there isn`t second word, just output blank, memory purposes
					}
				}
				std::cout << "\n";
				++hits;								// Total counter
			}
		}
	}
	
	std::cout << "Total " << hits << " line(s)\n";

	return 0;
}