#pragma once
#include <string>

class Ostrava
{
	using byte = unsigned char;
	static const unsigned int BYTE_MIN = 0;
	static const unsigned int BYTE_MAX = UCHAR_MAX;

	struct EncryptionParameters {
		byte blockSize; // размер шифруемого блока в битах
		byte appendBytes; // количество дописываемых в конец файла случайных байтов (не считая завершающего k-байта)
	};

	struct EncryptionTable {
		byte * data = nullptr;
		byte size;
		byte& operator[](const byte key) const { return data[key]; }
		EncryptionTable() {}
		~EncryptionTable() { if (data != nullptr) delete[] data; }
		EncryptionTable(const EncryptionTable& rhs) { *this = rhs; }
		EncryptionTable& operator=(const EncryptionTable& rhs) {
			if (data != nullptr) delete[] data;
			size = rhs.size;
			data = new byte[size];
			for (int i = 0; i < size; ++i)
				data[i] = rhs.data[i];
			return *this;
		}
	};

public:
	static void train(std::string directoryPath, std::string fileName);
	static std::string encrypt(std::string directoryPath, std::string fileName);
	static std::string decrypt(std::string directoryPath, std::string fileName);
private:
	static EncryptionParameters analyzeFileSize(unsigned long fileSizeBytes, byte blockSizeBits = 0);
	static EncryptionTable buildEncTable(byte blockSizeBits);
};

