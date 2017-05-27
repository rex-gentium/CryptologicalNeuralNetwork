#pragma once
#include <fann.h>
#include <string>

class CryptoANN
{
public:
	using byte = unsigned char;
	static const unsigned int BYTE_MIN = 0;
	static const unsigned int BYTE_MAX = UCHAR_MAX;

	struct EncryptionParameters {
		byte blockSize; // размер шифруемого блока в битах
		byte appendBytes; // количество дописываемых в конец файла случайных байтов (не считая завершающего k-байта)
	};

	struct EncryptionTable {
		byte * data = nullptr; // массив, где индексами выступают шифруемые числа, а значениями их криптограммы
		byte size;
		byte& operator[](const byte key) const;
		EncryptionTable() {}
		~EncryptionTable();
		EncryptionTable(const EncryptionTable& rhs);
		EncryptionTable& operator=(const EncryptionTable& rhs);
		void toTrainData(fann_train_data * trainData, int blockSizeBits) const;
		void toReversedTrainData(fann_train_data * trainData, int blockSizeBits) const;
	};

	/* тренирует шифрующую и дешифрующую нейронные сети с использованием файла, сохраняет конфигурацию сетей в директорию */
	static void train(std::string directoryPath, std::string fileName);
	/* восстанавливает шифрующую сеть из конфигурационного файла в директории, шифрует файл, сохраняет результат в директории */
	static void encrypt(std::string directoryPath, std::string fileName);
	/* восстанавливает дешифрующую сеть из конфигурационного файла в директории, дешифрует файл, сохраняет результат в директории */
	static void decrypt(std::string directoryPath, std::string fileName);

private:
	/* возвращает параметры шифрования, соответствующие размеру файла (и размеру блока)*/
	static EncryptionParameters analyzeFileSize(unsigned long fileSizeBytes, byte blockSizeBits = 0);
	/* возвращает случайную таблицу шифрования для блоков заданной длины */
	static EncryptionTable buildEncTable(byte blockSizeBits);
	/* дозаписывает в конец файла заданное количество байтов */
	static void appendBytes(std::string filePath, int byteCount);
	/**/
	static void processBuffer(byte * buffer, int bufferSize, int blockSizeBits, std::string encryptorPath);
	static void processBufferFast(byte * buffer, int bufferSize, int blockSizeBits, std::string encryptorPath);
};

