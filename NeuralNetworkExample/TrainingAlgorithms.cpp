#include "TrainingAlgorithms.h"
#include <fann.h>
#include <iostream>
#include <fstream>

const float TrainingAlgorithms::desired_error = 0.001f;

TrainingAlgorithms::byte * TrainingAlgorithms::Point::getBytes() const {
	int size = sizeof(int);
	byte * bytes = new byte[2 * size];
	for (int i = 0; i < sizeof(int); ++i) {
		bytes[size - 1 - i] = (x >> (i * 8));
		bytes[2 * size - 1 - i] = (y >> (i * 8));
	}
	return bytes;
}

TrainingAlgorithms::Cluster TrainingAlgorithms::Cluster::newInstance(Cluster ** clusterMapping, int lastI, int lastJ, int * clusterCount, int maxRadius) {
	Cluster cluster;
	bool isWrong;
	do {
		isWrong = false;
		cluster.center.x = rand();
		cluster.center.y = rand();
		cluster.radius = rand() % maxRadius;
		for (int i = 0; i <= lastI; ++i)
			for (int j = 0; j <= ((i < lastI || lastJ < 0) ? clusterCount[i] - 1 : lastJ); ++j)
				if (cluster.hasInterception(clusterMapping[i][j]))
					isWrong = true;
	} while (isWrong);
	return cluster;
}

bool TrainingAlgorithms::Cluster::hasInterception(const Cluster& that) const {
	int subX = this->center.x - that.center.x;
	int subY = this->center.y - that.center.y;
	float distance = sqrt(subX * subX + subY * subY);
	return distance <= static_cast<float>(this->radius + that.radius);
}

TrainingAlgorithms::Point TrainingAlgorithms::Cluster::randomInnerPoint() const {
	Point result;
	float angle = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / 3.14f));
	int r = rand() % this->radius;
	result.x = this->center.x + r * cosf(angle);
	result.y = this->center.y + r * sinf(angle);
	return result;
}

void TrainingAlgorithms::GridinSolodovnikov(std::string filePath)
{
	// скажем, что класс - байт (всего 256 классов)
	// 1. частотный анализ файла по классам
	int * freq = countByteFrequency(filePath);
	// 2. ставим в соответсвие число областей и генерируем области
	int * clusterCount = rangeClusterCount(freq, 1, 100);
	int sum = 0;
	for (int i = 0; i < BYTE_MAX + 1; ++i)
		sum += clusterCount[i];
	long maxRadius = 2 * static_cast<int> (INT_MAX / sqrt(sum)); // sum >= 256, then maxRadius < 2^29 < INT_MAX (2^31)
	Cluster ** clusters = new Cluster*[BYTE_MAX + 1];
	for (int i = 0; i < BYTE_MAX + 1; ++i) {
		int clusterCountI = clusterCount[i];
		std::cout << "BYTE " << i << " (" << clusterCountI << " clusters)" << std::endl;
		clusters[i] = new Cluster[clusterCountI];
		for (int j = 0; j < clusterCountI; ++j) {
			int lastJ = j - 1;
			int lastI = (lastJ < 0) ? i - 1 : i;
			clusters[i][j] = Cluster::newInstance(clusters, lastI, lastJ, clusterCount, maxRadius);
			std::cout << "\t(" << clusters[i][j].center.x << ", " << clusters[i][j].center.y << "), R=" << clusters[i][j].radius << std::endl;
		}
	}
	// 3. ставим в соответсвие байту из входного файла случайную точку из случайной области, к нему относящейся
	std::ifstream inputFile(filePath, std::ios::binary);
	std::ofstream outputFile(filePath + ".crypto", std::ios::binary);
	byte b;
	while (!inputFile.eof()) {
		inputFile >> std::noskipws >> b;
		int clusterNum = rand() % clusterCount[b];
		Point cipherPoint = clusters[b][clusterNum].randomInnerPoint();
		outputFile.write((char *)cipherPoint.getBytes(), 2 * sizeof(int));
	}
	inputFile.close();
	outputFile.close();

	/*struct fann *ann = fann_create_standard(num_layers, num_input,
		num_neurons_hidden, num_output);

	fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);

	fann_train_on_file(ann, filePath.c_str(), max_epochs,
		epochs_between_reports, desired_error);

	fann_save(ann, "xor_float.net");

	fann_destroy(ann);*/

	delete[] freq;
	delete[] clusterCount;
	delete[] clusters;
}

int * TrainingAlgorithms::countByteFrequency(std::string filePath)
{
	int * freq = new int[BYTE_MAX + 1];
	for (int i = 0; i < BYTE_MAX + 1; ++i)
		freq[i] = 0;

	byte b = 0;
	std::ifstream file(filePath, std::ios::binary);
	while (!file.eof()) {
		file >> std::noskipws >> b;
		++freq[b];
	}
	file.close();

	return freq;
}

int * TrainingAlgorithms::rangeClusterCount(const int * frequencies, int minClusterValue, int maxClusterValue)
{
	if (minClusterValue <= 0) minClusterValue = 1;
	if (maxClusterValue <= 0) maxClusterValue = 1;
	if (maxClusterValue < minClusterValue) maxClusterValue = minClusterValue;
	int maxFreq = 0;
	for (int i = 0; i < BYTE_MAX + 1; ++i)
		if (frequencies[i] > maxFreq) maxFreq = frequencies[i];
	// linear function: clusterCount = k * frequency + minClusterValue
	float k = (maxClusterValue - minClusterValue) / (float) maxFreq;
	int * clusters = new int[BYTE_MAX + 1];
	for (int i = 0; i < BYTE_MAX + 1; ++i)
		clusters[i] = (int) floorf(k * frequencies[i] + minClusterValue);
	return clusters;
}
