#include <chrono>
#include "GridinSolodovnikov.h"
#include "Ostrava.h"

int main() {
		
	//std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
	//Ostrava::train("D:/", "test.txt");
	Ostrava::decrypt("D:/", "test1.txt.crypto");
	//std::string encName = GridinSolodovnikov::cipher("C:/dump/", "test.txt");
	//GridinSolodovnikov::decipher("C:/dump/", encName);
	
	//std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	//std::chrono::duration<double, std::milli> time_span = t2 - t1;
	//printf("ANN built in %lf milliseconds\n", time_span.count());

	return 0;
}