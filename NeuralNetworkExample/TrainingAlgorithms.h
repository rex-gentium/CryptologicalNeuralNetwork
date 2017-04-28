#pragma once
#include <vector>
#include <string>

class TrainingAlgorithms
{
	using byte = unsigned char;
	static const unsigned int BYTE_MIN = 0;
	static const unsigned int BYTE_MAX = UCHAR_MAX;
	struct Point {
		int x, y;
		byte * getBytes() const;
	};
	struct Cluster {
		Point center;
		int radius;
		static Cluster newInstance(Cluster ** clusterMapping, int lastI, int lastJ, int * clusterCount, int maxRadius);
		bool hasInterception(const Cluster& that) const;
		Point randomInnerPoint() const;
	};
public:
	static const unsigned int num_input = 2;
	static const unsigned int num_output = 1;
	static const unsigned int num_layers = 5;
	static const unsigned int num_neurons_hidden = 8;
	static const float desired_error;
	static const unsigned int max_epochs = 500000;
	static const unsigned int epochs_between_reports = 1000;

public:
	static void GridinSolodovnikov(std::string filePath);
private:
	static int * countByteFrequency(std::string filePath);
	static int * rangeClusterCount(const int * frequencies, int minClusterValue, int maxClusterValue);
	static std::vector<byte> getFileBytes(std::string filePath);
	static std::vector<Point> getFilePoints(std::string filePath);
};

