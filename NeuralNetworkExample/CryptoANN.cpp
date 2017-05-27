#include "CryptoANN.h"
#include <fstream>
#include <vector>
#include <queue>
#include <bitset>
#include <thread>

void CryptoANN::train(std::string directoryPath, std::string fileName)
{
	// 1. анализируем размер файла
	struct stat filestatus;
	stat((directoryPath + fileName).c_str(), &filestatus);
	unsigned long fileSize = static_cast<unsigned long>(filestatus.st_size);
	EncryptionParameters encInfo = analyzeFileSize(fileSize);
	// 3. заполняем таблицу соответсвий по блокам
	EncryptionTable encTable = buildEncTable(encInfo.blockSize);
	// 4. формируем тренировочные данные по таблице
	struct fann_train_data * enc_data = fann_create_train(encTable.size, encInfo.blockSize, encInfo.blockSize);	
	struct fann_train_data * dec_data = fann_create_train(encTable.size, encInfo.blockSize, encInfo.blockSize);
	encTable.toTrainData(enc_data, encInfo.blockSize);
	encTable.toReversedTrainData(dec_data, encInfo.blockSize);
	// 5. обучаем сети
	struct fann * encryptor = fann_create_standard(3, encInfo.blockSize, encInfo.blockSize * encInfo.blockSize, encInfo.blockSize);
	fann_set_activation_function_hidden(encryptor, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(encryptor, FANN_SIGMOID_SYMMETRIC);
	fann_train_on_data(encryptor, enc_data, 200000, 1000, 0.0001f);
	fann_save(encryptor, (directoryPath + "encryptor.net").c_str());
	fann_destroy(encryptor);
	fann_destroy_train(enc_data);

	struct fann * decryptor = fann_create_standard(3, encInfo.blockSize, encInfo.blockSize * encInfo.blockSize, encInfo.blockSize);
	fann_set_activation_function_hidden(decryptor, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(decryptor, FANN_SIGMOID_SYMMETRIC);
	fann_train_on_data(decryptor, dec_data, 200000, 1000, 0.0001f);
	fann_save(decryptor, (directoryPath + "decryptor.net").c_str());
	fann_destroy(decryptor);
	fann_destroy_train(dec_data);
}

std::bitset<8> popByte(std::queue<CryptoANN::byte> &bitQueue) {
	std::bitset<8> res;
	for (int i = 0; !bitQueue.empty() && i < 8; ++i) {
		res[7 - i] = bitQueue.front();
		bitQueue.pop();
	}
	return std::bitset<8>(res);
}

void CryptoANN::processBuffer(byte * buffer, int bufferSize, int blockSizeBits, std::string encryptorPath) {
	struct fann * encryptor = fann_create_from_file(encryptorPath.c_str());
	std::queue<fann_type> remnantBits;
	std::vector<fann_type> inputBits;
	std::queue<byte> outputBits;
	int i_inp = 0, i_out = 0;
	while (i_inp < bufferSize || !remnantBits.empty()) {
		// заносим во ввод оставшиеся биты прошлого байта
		while (inputBits.size() < blockSizeBits && !remnantBits.empty()) {
			inputBits.push_back(remnantBits.front());
			remnantBits.pop();
		}
		// потом по необходимости считываем следующий байт
		if (inputBits.size() < blockSizeBits) {
			std::bitset<8> inpByte(buffer[i_inp++]);
			int j = 8;
			while (inputBits.size() < blockSizeBits)
				inputBits.push_back(inpByte[--j]);
			while (j > 0)
				remnantBits.push(inpByte[--j]);
		}
		// шифруем блок
		fann_type * input = inputBits.data();
		fann_type * output = fann_run(encryptor, input);
		for (int j = 0; j < blockSizeBits; ++j) {
			outputBits.push(static_cast<byte>(roundf(output[j])));
			if (outputBits.size() >= 8) {
				// пишем криптограмму в массив, если набрался байт
				std::bitset<8> outByte = popByte(outputBits);
				buffer[i_out++] = outByte.to_ulong();
			}
		}
		inputBits.clear();
	}
	fann_destroy(encryptor);
}

void CryptoANN::processBufferFast(byte * buffer, int bufferSize, int blockSizeBits, std::string encryptorPath) {
	byte * data = new byte[8 * bufferSize];
	for (int i = 0; i < bufferSize; ++i) {
		std::bitset<8> inpByte(buffer[i]);
		for (int j = 0; j < 8; ++j)
			data[8 * i + j] = inpByte[7 - j];
	}
	struct fann * encryptor = fann_create_from_file(encryptorPath.c_str());
	fann_type * input = new fann_type[blockSizeBits], *output;
	for (int i = 0; i < 8 * bufferSize / blockSizeBits; ++i) {
		for (int j = 0; j < blockSizeBits; ++j)
			input[j] = static_cast<fann_type>(data[i * blockSizeBits + j]);
		output = fann_run(encryptor, input);
		for (int j = 0; j < blockSizeBits; ++j)
			data[i * blockSizeBits + j] = static_cast<byte>(roundf(output[j]));
	}
	fann_destroy(encryptor);
	for (int i = 0; i < bufferSize; ++i) {
		std::bitset<8> inpByte;
		for (int j = 0; j < 8; ++j)
			inpByte[7 - j] = data[8 * i + j];
		buffer[i] = inpByte.to_ulong();
	}
	delete[] data;
}

void CryptoANN::encrypt(std::string directoryPath, std::string fileName)
{
	// 1. восстанавливаем шифратор и анализируем размер файла
	struct stat filestatus;
	stat((directoryPath + fileName).c_str(), &filestatus);
	unsigned long fileSize = static_cast<unsigned long>(filestatus.st_size);
	struct fann * encryptor = fann_create_from_file((directoryPath + "encryptor.net").c_str());
	EncryptionParameters encInfo = analyzeFileSize(fileSize, encryptor->num_input);
	fann_destroy(encryptor);
	// 2. дописываем необходимое число случайных байтов и байт-счётчик
	appendBytes(directoryPath + fileName, encInfo.appendBytes);
	fileSize += encInfo.appendBytes + 1;
	// 3. делим файл на t частей и шифруем их параллельно
	int threadCount = 4;
	std::ifstream ifile(directoryPath + fileName, std::ios::binary);
	int m = fileSize / encInfo.blockSize;
	int bufferSize = m * encInfo.blockSize / threadCount;
	while (bufferSize < encInfo.blockSize)
		bufferSize = m * encInfo.blockSize / --threadCount;
	int lastBufferSize = fileSize - bufferSize * threadCount;
	byte ** buffers = new byte*[threadCount];
	byte * lastBuffer = (lastBufferSize > 0) ? new byte[lastBufferSize] : nullptr;
	std::vector<std::thread> encThreads;
	for (int i = 0; i < threadCount; ++i) {
		buffers[i] = new byte[bufferSize];
		ifile.read((char*)buffers[i], bufferSize);
		encThreads.push_back(std::thread(processBufferFast, buffers[i], bufferSize, encInfo.blockSize, directoryPath + "encryptor.net"));
	}
	if (lastBuffer != nullptr) {
		ifile.read((char*)lastBuffer, lastBufferSize);
		encThreads.push_back(std::thread(processBufferFast, lastBuffer, lastBufferSize, encInfo.blockSize, directoryPath + "encryptor.net"));
	}
	ifile.close();
	for (auto & t : encThreads) 
		t.join();
	// 4. выводим результат
	std::ofstream ofile(directoryPath + fileName, std::ios::binary);
	for (int i = 0; i < threadCount; ++i)
		ofile.write((char*)buffers[i], bufferSize);
	if (lastBuffer != nullptr)
		ofile.write((char*)lastBuffer, lastBufferSize);
	ofile.close();
	for (int i = 0; i < threadCount; ++i)
		delete[] buffers[i];
	if (lastBuffer != nullptr)
		delete[] lastBuffer;
	delete[] buffers;
}

void CryptoANN::decrypt(std::string directoryPath, std::string fileName)
{
	struct stat filestatus;
	stat((directoryPath + fileName).c_str(), &filestatus);
	unsigned long fileSize = static_cast<unsigned long>(filestatus.st_size);
	struct fann * decryptor = fann_create_from_file((directoryPath + "decryptor.net").c_str());
	int blockSize = decryptor->num_input;
	fann_destroy(decryptor);

	int threadCount = 4;
	std::ifstream ifile(directoryPath + fileName, std::ios::binary);
	int m = fileSize / blockSize;
	int bufferSize = m * blockSize / threadCount;
	while (bufferSize < blockSize)
		bufferSize = m * blockSize / --threadCount;
	int lastBufferSize = fileSize - bufferSize * threadCount;
	byte ** buffers = new byte*[threadCount];
	byte * lastBuffer = (lastBufferSize > 0) ? new byte[lastBufferSize] : nullptr;
	std::vector<std::thread> encThreads;
	for (int i = 0; i < threadCount; ++i) {
		buffers[i] = new byte[bufferSize];
		ifile.read((char*)buffers[i], bufferSize);
		encThreads.push_back(std::thread(processBufferFast, buffers[i], bufferSize, blockSize, directoryPath + "decryptor.net"));
	}
	if (lastBuffer != nullptr) {
		ifile.read((char*)lastBuffer, lastBufferSize);
		encThreads.push_back(std::thread(processBufferFast, lastBuffer, lastBufferSize, blockSize, directoryPath + "decryptor.net"));
	}
	ifile.close();
	for (auto & t : encThreads)
		t.join();
	// из конца массива узнаём, сколько байтов с конца нужно отрезать
	byte offset = 1 + ((lastBuffer != nullptr)
		? lastBuffer[lastBufferSize - 1] 
		: buffers[threadCount - 1][bufferSize - 1]);
	// выводим файл, не захватывая последние байты
	std::ofstream ofile(directoryPath + fileName, std::ios::binary);
	for (int i = 0; i < threadCount - 1; ++i)
		ofile.write((char*)buffers[i], bufferSize);
	if (lastBuffer == nullptr)
		ofile.write((char*)buffers[threadCount - 1], bufferSize - offset);
	else {
		ofile.write((char*)buffers[threadCount - 1], bufferSize);
		ofile.write((char*)lastBuffer, lastBufferSize - offset);
	}
	ofile.close();

	for (int i = 0; i < threadCount; ++i)
		delete[] buffers[i];
	if (lastBuffer != nullptr)
		delete[] lastBuffer;
	delete[] buffers;
}

CryptoANN::EncryptionParameters CryptoANN::analyzeFileSize(unsigned long fileSizeBytes, byte blockSizeBits)
{
	EncryptionParameters result;
	result.appendBytes = BYTE_MAX;
	if (blockSizeBits == 0) {
		byte possibleBlockSizes[] = { 5, 6, 7 };
		for (const byte & blockSize : possibleBlockSizes) {
			byte appendBytes = (blockSize - (fileSizeBytes + 1) % blockSize) % blockSize;
			if (appendBytes <= result.appendBytes) {
				result.appendBytes = appendBytes;
				result.blockSize = blockSize;
			}
		}
	}
	else {
		result.blockSize = blockSizeBits;
		result.appendBytes = (blockSizeBits - (fileSizeBytes + 1) % blockSizeBits) % blockSizeBits;
	}
	return result;
}

CryptoANN::EncryptionTable CryptoANN::buildEncTable(byte blockSizeBits)
{
	EncryptionTable table;
	table.size = 1;
	for (int i = 0; i < blockSizeBits; ++i)
		table.size *= 2;
	table.data = new byte[table.size];
	std::vector<byte> associations;
	for (byte b = BYTE_MIN; b < table.size; ++b)
		associations.push_back(b);
	for (byte b = BYTE_MIN; b < table.size; ++b) {
		byte associate = rand() % associations.size();
		table[b] = associations[associate];
		std::swap(associations[associate], associations.back());
		associations.pop_back();
	}
	associations.clear();
	return table;
}

void CryptoANN::appendBytes(std::string filePath, int byteCount)
{
	std::ofstream file(filePath, std::ios::binary | std::ios::app);
	file.seekp(std::ios::end);
	for (int i = 0; i < byteCount; ++i) {
		byte randomByte = rand() % (BYTE_MAX + 1);
		file.write((char *)&randomByte, 1);
	}
	file.write((char *)&byteCount, 1);
	file.close();
}

CryptoANN::byte & CryptoANN::EncryptionTable::operator[](const byte key) const
{
	return data[key];
}

CryptoANN::EncryptionTable::~EncryptionTable()
{
	if (data != nullptr) delete[] data;
}

CryptoANN::EncryptionTable::EncryptionTable(const EncryptionTable & rhs)
{
	*this = rhs;
}

CryptoANN::EncryptionTable & CryptoANN::EncryptionTable::operator=(const EncryptionTable & rhs)
{
	if (data != nullptr) delete[] data;
	size = rhs.size;
	data = new byte[size];
	for (int i = 0; i < size; ++i)
		data[i] = rhs.data[i];
	return *this;
}

void CryptoANN::EncryptionTable::toTrainData(fann_train_data * trainData, int blockSizeBits) const
{
	for (int i = 0; i < size; ++i) {
		std::bitset<8> block(i); // шифруемый блок
		std::bitset<8> cryptogram(data[i]); // результат шифрования
		// блоки заполнены ведущими нулями, не будем их учитывать
		// битовые маски хранят биты по возрастанию порядков (reverse order)
		for (int j = 0; j < blockSizeBits; ++j) {
			trainData->input[i][blockSizeBits - 1 - j] = block[j];
			trainData->output[i][blockSizeBits - 1 - j] = cryptogram[j];
		}
	}
}

void CryptoANN::EncryptionTable::toReversedTrainData(fann_train_data * trainData, int blockSizeBits) const
{
	for (int i = 0; i < size; ++i) {
		std::bitset<8> block(i); // шифруемый блок
		std::bitset<8> cryptogram(data[i]); // результат шифрования
		// блоки заполнены ведущими нулями, не будем их учитывать
		// битовые маски хранят биты по возрастанию порядков (reverse order)
		for (int j = 0; j < blockSizeBits; ++j) {
			trainData->output[i][blockSizeBits - 1 - j] = block[j];
			trainData->input[i][blockSizeBits - 1 - j] = cryptogram[j];
		}
	}
}
