#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

string Rname;

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
	
	file = "PRG_MVT.mod";
	word = "SR_SOUDER";
	
	std::ifstream inFile(file);						// READ-Only the file (format needed to specify like "text.txt")
	if (!inFile){
		std::cerr << "File not found\n";
		return 1;
	}
	std::ofstream outFile("ponto_de_solda.csv");	// WRITE-only a new file

	std::string line;
	int lineNumber = 0;
	int hits = 0;
	
	while (std::getline(inFile, line)){				// First parameter is the file with data insenting into the var "line"
		++lineNumber;
		
		if(containsWord(line, word)){
			int res = line.find("!"); 				// Remove " ! " (ABB comentary)
				if(res == string::npos){ 				// When not find " ! ", a error will appear and go to npos, the IF is to let the code runs
					for (char &c : line) {			// FOR to change " , " with a blank space
						if (c == ',') c = ' ';
					}
						
					std::istringstream iss(line); 	// Change the full (not-truly) static text and endline after each blank space, separating the words
					std::string word1, word2;
					iss >> word1 >> word2; 			// Store the first word that endl, and then the second
				
					outFile << lineNumber << ". " << "," << word1 << ","; 	// Print the filtered search
					if (!word2.empty()) {
						outFile << " " << word2 << endl;;	// If there isn`t second word, just output blank, memory purposes
					}
				}
				++hits;								// Total counter
		}
	}
	return 0;
}
	
