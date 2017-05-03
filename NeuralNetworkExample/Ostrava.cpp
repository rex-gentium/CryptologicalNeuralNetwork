﻿#include "Ostrava.h"
#include <fann.h>
#include <fstream>
#include <vector>
#include <queue>
#include <bitset>

void Ostrava::train(std::string directoryPath, std::string fileName)
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
	for (int i = 0; i < encTable.size; ++i) {
		std::bitset<8> block(i);
		std::bitset<8> cryptogram(encTable[i]); // заполнен ведущими нулями, не будем их учитывать
		// битовые маски хранят биты по возрастанию порядков (reverse order)
		for (int j = 0; j < encInfo.blockSize; ++j) {
			enc_data->input[i][encInfo.blockSize - 1 - j] = block[j];
			dec_data->output[i][encInfo.blockSize - 1 - j] = block[j];
			enc_data->output[i][encInfo.blockSize - 1 - j] = cryptogram[j];
			dec_data->input[i][encInfo.blockSize - 1 - j] = cryptogram[j];
		}
	}
	// 5. обучаем сеть
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
	fann_train_on_data(decryptor, dec_data, 200000, 1000, 0.00005f);
	fann_save(decryptor, (directoryPath + "decryptor.net").c_str());
	fann_destroy(decryptor);
	fann_destroy_train(dec_data);
}

std::string Ostrava::encrypt(std::string directoryPath, std::string fileName)
{
	struct stat filestatus;
	stat((directoryPath + fileName).c_str(), &filestatus);
	unsigned long fileSize = static_cast<unsigned long>(filestatus.st_size);
	
	struct fann * encryptor = fann_create_from_file((directoryPath + "encryptor.net").c_str());
	EncryptionParameters encInfo = analyzeFileSize(fileSize, encryptor->num_input);
	// дописываем необходимое число случайных байтов и байт-счётчик
	std::ofstream inpFile(directoryPath + fileName, std::ios::binary | std::ios::app);
	inpFile.seekp(std::ios::end);
	for (int i = 0; i < encInfo.appendBytes; ++i) {
		byte randomByte = rand() % (BYTE_MAX + 1);
		inpFile.write((char *)&randomByte, 1);
	}
	inpFile.write((char *)&encInfo.appendBytes, 1);
	inpFile.close();
	// считываем файл поблочно
	std::ifstream ifile(directoryPath + fileName, std::ios::binary);
	std::ofstream ofile(directoryPath + fileName + ".crypto", std::ios::binary);
	std::queue<byte> buffer; // очередь битов на ввод
	std::queue<byte> outputBuffer; // очередь битов на вывод
	byte b;
	std::vector<byte> currentBlock; // будем хранить блок как массив битов
	while (!ifile.eof()) {
		currentBlock.reserve(encInfo.blockSize);
		// сначала проталкиваем в блок остаток от предыдущего байта (буфер может не поустшиться полностью)
		while (!buffer.empty() && currentBlock.size() < encInfo.blockSize) {
			currentBlock.push_back(buffer.front());
			buffer.pop();
		}
		// потом по необходимости считываем следующий байт
		if (currentBlock.size() < encInfo.blockSize) {
			ifile >> std::noskipws >> b;
			std::bitset<8> currentByte(b);
			int offset = 8 - 1;
			while (currentBlock.size() < encInfo.blockSize) {
				byte nextBit = currentByte[offset--];
				currentBlock.push_back(nextBit);
			}
			// остаток байта уходит в буфер для следующей итерации
			while (offset >= 0)
				buffer.push(currentByte[offset--]);
		}
		// текущий блок отправляется на зашифровку
		fann_type * input = new fann_type[encInfo.blockSize];
		for (int i = 0; i < encInfo.blockSize; ++i)
			input[i] = static_cast<fann_type>(currentBlock[i]);
		fann_type * output = fann_run(encryptor, input);
		delete[] input;
		currentBlock.clear();
		// зашифрованный блок по битам пишется в буфер вывода
		for (int i = 0; i < encInfo.blockSize; ++i) {
			byte bit = static_cast<byte>(roundf(output[i]));
			outputBuffer.push(bit);
			if (outputBuffer.size() == 8) {
				// если буфер переполняется, выводим готовый байт в файл
				std::bitset<8> byteBits;
				for (int i = 0; i < 8; ++i) {
					byteBits[7 - i] = outputBuffer.front();
					outputBuffer.pop();
				}
				byte byteValue = byteBits.to_ulong();
				ofile.write((char*)&byteValue, 1);
			}
		}
	}
	ifile.close();
	ofile.close();
	return directoryPath + fileName + ".crypto";
}

std::string Ostrava::decrypt(std::string directoryPath, std::string fileName)
{
	struct fann * decryptor = fann_create_from_file((directoryPath + "decryptor.net").c_str());
	EncryptionParameters encInfo;
	encInfo.blockSize = decryptor->num_input;
	// считываем файл поблочно
	std::ifstream ifile(directoryPath + fileName, std::ios::binary);
	std::ofstream ofile(directoryPath + fileName + ".temp", std::ios::binary);
	std::queue<byte> buffer; // очередь битов на ввод
	std::queue<byte> outputBuffer; // очередь битов на вывод
	byte b;
	std::vector<byte> currentBlock; // будем хранить блок как массив битов
	while (!ifile.eof()) {
		currentBlock.reserve(encInfo.blockSize);
		// сначала проталкиваем в блок остаток от предыдущего байта
		while (!buffer.empty() && currentBlock.size() < encInfo.blockSize) {
			currentBlock.push_back(buffer.front());
			buffer.pop();
		}
		// потом по необходимости считываем следующий байт
		if (currentBlock.size() < encInfo.blockSize) {
			ifile >> std::noskipws >> b;
			std::bitset<8> currentByte(b);
			int offset = 8 - 1;
			while (currentBlock.size() < encInfo.blockSize) {
				byte nextBit = currentByte[offset--];
				currentBlock.push_back(nextBit);
			}
			// остаток байта уходит в буфер для следующей итерации
			while (offset >= 0)
				buffer.push(currentByte[offset--]);
		}
		// текущий блок отправляется на расшифровку
		fann_type * input = new fann_type[encInfo.blockSize];
		for (int i = 0; i < encInfo.blockSize; ++i)
			input[i] = static_cast<fann_type>(currentBlock[i]);
		fann_type * output = fann_run(decryptor, input);
		delete[] input;
		currentBlock.clear();
		// зашифрованный блок по битам пишется в буфер вывода
		for (int i = 0; i < encInfo.blockSize; ++i) {
			byte bit = static_cast<byte>(roundf(output[i]));
			outputBuffer.push(bit);
			if (outputBuffer.size() == 8) {
				// если буфер переполняется, выводим готовый байт в файл
				std::bitset<8> byteBits;
				for (int i = 0; i < 8; ++i) {
					byteBits[7 - i] = outputBuffer.front();
					outputBuffer.pop();
				}
				byte byteValue = byteBits.to_ulong();
				ofile.write((char*)&byteValue, 1);
			}
		}
	}
	ifile.close();
	ofile.close();
	// из конца файла узнаём, сколько байтов с конца нужно отрезать
	std::ifstream tmpFile(directoryPath + fileName + ".temp");
	tmpFile.seekg(-1, std::ios_base::end);
	byte backOffset;
	tmpFile >> backOffset;
	tmpFile.close();
	// переписываем содержимое файла в чистовик, не захватывая последние байты
	struct stat filestatus;
	stat((directoryPath + fileName + ".temp").c_str(), &filestatus);
	unsigned long fileSize = static_cast<unsigned long>(filestatus.st_size);
	unsigned long byteCount = 0;
	std::ifstream tempFile(directoryPath + fileName + ".temp", std::ios::binary);
	std::ofstream outFile(directoryPath + fileName + ".decrypted", std::ios::binary);
	while (byteCount < fileSize - backOffset - 1) {
		tempFile >> std::noskipws >> b;
		outFile.write((char*)&b, 1);
		++byteCount;
	}
	tempFile.close();
	outFile.close();
	std::remove((directoryPath + fileName + ".temp").c_str());
	return directoryPath + fileName + ".decrypted";
}

Ostrava::EncryptionParameters Ostrava::analyzeFileSize(unsigned long fileSizeBytes, byte blockSizeBits)
{
	EncryptionParameters result;
	result.appendBytes = BYTE_MAX;
	if (blockSizeBits == 0) {
		byte possibleBlockSizes[] = { 3, 5, 6, 7 };
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

Ostrava::EncryptionTable Ostrava::buildEncTable(byte blockSizeBits)
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