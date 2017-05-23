#include "GridinSolodovnikov.h"
#include <fann.h>
#include <iostream>
#include <fstream>
#include <vector>

GridinSolodovnikov::byte * GridinSolodovnikov::Point::getBytes() const {
	int size = sizeof(unsigned short);
	byte * bytes = new byte[size];
	bytes[0] = x;
	bytes[1] = (x >> 8);
	return bytes;
}

GridinSolodovnikov::Cluster GridinSolodovnikov::Cluster::newInstance(Cluster ** clusterMapping, int lastI, int lastJ, int * clusterCount, unsigned char maxRadius) {
	Cluster cluster;
	bool isWrong;
	do {
		isWrong = false;
		cluster.center.x = rand();
		cluster.radius = 1 + rand() % maxRadius;
		for (int i = 0; i <= lastI; ++i)
			for (int j = 0; j <= ((i < lastI || lastJ < 0) ? clusterCount[i] - 1 : lastJ); ++j)
				if (cluster.hasInterception(clusterMapping[i][j]))
					isWrong = true;
	} while (isWrong);
	return cluster;
}

bool GridinSolodovnikov::Cluster::hasInterception(const Cluster& that) const {
	short distance = abs(this->center.x - that.center.x);
	return distance <= (this->radius + that.radius);
}

GridinSolodovnikov::Point GridinSolodovnikov::Cluster::randomInnerPoint() const {
	Point result;
	char shift = ((rand() % UCHAR_MAX) - CHAR_MAX) % this->radius;
	result.x = this->center.x + shift;
	return result;
}

void GridinSolodovnikov::train(std::string directoryPath, std::string fileName)
{
	// скажем, что класс - байт (всего 256 классов)
	// 1. частотный анализ файла по классам
	int * freq = countByteFrequency(directoryPath + fileName);
	// 2. ставим в соответсвие число областей и генерируем области
	unsigned char maxClusterCount = 10; // should be less than 64
	int * clusterCount = rangeClusterCount(freq, 1, maxClusterCount);
	delete[] freq;
	int sum = 0;
	for (int i = 0; i < BYTE_MAX + 1; ++i)
		sum += clusterCount[i];
	unsigned char maxRadius = USHRT_MAX / (2 * sum);
	Cluster ** clusters = new Cluster*[BYTE_MAX + 1];
	for (int i = 0; i < BYTE_MAX + 1; ++i) {
		int clusterCountI = clusterCount[i];
		std::cout << "BYTE " << i << " (" << clusterCountI << " clusters)" << std::endl;
		clusters[i] = new Cluster[clusterCountI];
		for (int j = 0; j < clusterCountI; ++j) {
			int lastJ = j - 1;
			int lastI = (lastJ < 0) ? i - 1 : i;
			clusters[i][j] = Cluster::newInstance(clusters, lastI, lastJ, clusterCount, maxRadius);
			std::cout << "\t(" << clusters[i][j].center.x - clusters[i][j].radius << ", " << clusters[i][j].center.x + clusters[i][j].radius << ")" << std::endl;
		}
	}
	// 3. ставим в соответствие байту из входного файла случайную точку из случайной области, к нему относящейся
	std::ifstream inputFile(directoryPath + fileName, std::ios::binary);
	std::vector<Point> points;
	std::vector<byte> bytes;
	byte b;
	while (!inputFile.eof()) {
		inputFile >> std::noskipws >> b;
		bytes.push_back(b);
		int clusterNum = rand() % clusterCount[b];
		Point cipherPoint = clusters[b][clusterNum].randomInnerPoint();
		points.push_back(cipherPoint);
	}
	delete[] clusterCount;
	delete[] clusters;
	inputFile.close();
	// 4. формируем сет тренировочных данных
	struct fann_train_data * encryptor_train_data = fann_create_train(points.size(), 1, 1);
	struct fann_train_data * decryptor_train_data = fann_create_train(points.size(), 1, 1);
	std::ofstream test_data_file("D:/data.dat");
	test_data_file << points.size() << " 2 1" << std::endl;
	for (int i = points.size() - 1; i >= 0; --i) {
		decryptor_train_data->input[i][0] = points[i].x / static_cast<float>(USHRT_MAX);
		encryptor_train_data->output[i][0] = points[i].x / static_cast<float>(USHRT_MAX);
		test_data_file << decryptor_train_data->input[i][0] << ' ';
		points.pop_back();
		decryptor_train_data->output[i][0] = bytes[i] / 255.0f;
		encryptor_train_data->input[i][0] = bytes[i] / 255.0f;
		test_data_file << decryptor_train_data->output[i][0] << std::endl;
		bytes.pop_back();
	}
	points.clear();
	bytes.clear();
	test_data_file.close();
	// 5. тренируем шифратор и дешифратор
	struct fann *encryptor = fann_create_standard(3, 1, 256, 1);
	struct fann *decryptor = fann_create_standard(3, 1, 256, 1);
	fann_set_activation_function_hidden(encryptor, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(encryptor, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_hidden(decryptor, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(decryptor, FANN_SIGMOID_SYMMETRIC);
	fann_train_on_data(encryptor, encryptor_train_data, 50000, 1, 0.0000001f);
	fann_train_on_data(decryptor, decryptor_train_data, 50000, 1, 0.0000001f);
	
	fann_save(encryptor, (directoryPath + ".encryptor").c_str());
	fann_save(decryptor, (directoryPath + ".decryptor").c_str());

	fann_destroy(encryptor);
	fann_destroy(decryptor);
}

void GridinSolodovnikov::encrypt(std::string directoryPath, std::string fileName)
{
	struct fann * encryptor = fann_create_from_file((directoryPath + ".encryptor").c_str());
	std::ifstream inputFile(directoryPath + fileName, std::ios::binary);
	std::ofstream outputFile(directoryPath + fileName + ".crypto", std::ios::binary);
	byte b;
	while (!inputFile.eof()) {
		inputFile >> std::noskipws >> b;
		fann_type * output = fann_run(encryptor, (fann_type *)&b);
		float x = output[0] * USHRT_MAX;
		Point cipherPoint;
		cipherPoint.x = static_cast<unsigned short>(roundf(x));
		byte * bytes = cipherPoint.getBytes();
		outputFile.write((char *)bytes, sizeof(unsigned short));
		delete[] bytes;
		//delete[] output;
	}
}

void GridinSolodovnikov::decrypt(std::string directoryPath, std::string fileName)
{
	struct fann * decryptor = fann_create_from_file((directoryPath + ".decryptor").c_str());
	std::ifstream inputFile(directoryPath + fileName, std::ios::binary);
	std::ofstream outputFile(directoryPath + fileName + ".decrypted.txt", std::ios::binary);
	while (!inputFile.eof()) {
		Point p = getNextPoint(inputFile);
		fann_type input = (fann_type) (p.x / static_cast<float>(USHRT_MAX));
		fann_type * output = fann_run(decryptor, &input);
		byte b = static_cast<byte>(output[0] * 255.0f);
		outputFile.write((char *)&b, 1);
	}
}

int * GridinSolodovnikov::countByteFrequency(std::string filePath)
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

int * GridinSolodovnikov::rangeClusterCount(const int * frequencies, int minClusterValue, int maxClusterValue)
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
		clusters[i] = (int) roundf(k * frequencies[i] + minClusterValue);
	return clusters;
}

/*std::vector<GridinSolodovnikov::byte> GridinSolodovnikov::getFileBytes(std::string filePath)
{
	std::vector<byte> bytes;
	std::ifstream file(filePath, std::ios::binary);
	byte b;
	while (!file.eof()) {
		file >> std::noskipws >> b;
		bytes.push_back(b);
	}
	file.close();
	return bytes;
}*/

GridinSolodovnikov::Point GridinSolodovnikov::getNextPoint(std::ifstream & file) {
	byte b;
	Point point;
	point.x = 0;
	for (int i = 0; i < sizeof(unsigned short); ++i) {
		file >> std::noskipws >> b;
		unsigned short byteAsUShort = (unsigned short)b;
		int shift = 8 * (sizeof(unsigned short) - i - 1);
		point.x |= (byteAsUShort << shift);
	}
	return point;
}

std::vector<GridinSolodovnikov::Point> GridinSolodovnikov::getFilePoints(std::string filePath)
{
	std::vector<Point> points;
	std::ifstream file(filePath, std::ios::binary);
	
	byte b;
	while (!file.eof()) {
		Point point = getNextPoint(file);
		points.push_back(point);
	}
	file.close();
	points.pop_back();
	return points;
}
