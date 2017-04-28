#include <chrono>
#include "TrainingAlgorithms.h"

int main() {
		
	//std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

	TrainingAlgorithms::GridinSolodovnikov("D:/test_graph.txt");

	//std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	//std::chrono::duration<double, std::milli> time_span = t2 - t1;
	//printf("ANN built in %lf milliseconds\n", time_span.count());

	return 0;
}