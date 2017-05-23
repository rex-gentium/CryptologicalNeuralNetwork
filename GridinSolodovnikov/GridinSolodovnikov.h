#pragma once
#include <vector>
#include <string>

class GridinSolodovnikov
{
	/* в качестве шифруемого класса используем байт (всего 256 классов), шифруем класс одномерной областью от 0 до 65535 (размер фалй авозрастёт вдвое)*/
	using byte = unsigned char;
	static const unsigned int BYTE_MIN = 0;
	static const unsigned int BYTE_MAX = UCHAR_MAX;

	struct Point {
		unsigned short x; // 0..65535
		byte * getBytes() const;
	};

	struct Cluster {
		Point center;
		unsigned char radius; // 0..127
		static Cluster newInstance(Cluster ** clusterMapping, int lastI, int lastJ, int * clusterCount, unsigned char maxRadius);
		bool hasInterception(const Cluster& that) const;
		Point randomInnerPoint() const;
	};

public:
	static void train(std::string directoryPath, std::string fileName);
	static void encrypt(std::string directoryPath, std::string fileName);
	static void decrypt(std::string directoryPath, std::string fileName);
private:
	static int * countByteFrequency(std::string filePath);
	static int * rangeClusterCount(const int * frequencies, int minClusterValue, int maxClusterValue);
	static Point getNextPoint(std::ifstream & file);
	//static std::vector<byte> getFileBytes(std::string filePath);
	static std::vector<Point> getFilePoints(std::string filePath);
};

