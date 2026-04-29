#include <iostream>
#include <string>
#include <fstream>

using namespace std;

int main(){
	
	ifstream myFile("car.txt");
	if(myFile.is_open()){
		string line;
		int lineNumber = 1;
		while (getline(myFile, line)) {
			cout << lineNumber++ << ". " << line << endl;
		}
	}
	
	myFile.close();

	return 0;
}