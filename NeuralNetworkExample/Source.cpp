#include "Ostrava.h"
#include <string>

/// <summary>
/// This method should be called with execution parameters
/// <para>argv[1] - directory, where the file and network config files are to be located, e.g. "C:\dump/hello" or "C:/dump\hello\"</para>
/// <para>argv[2] - file name, e.g. "myFile.txt"</para>
/// <para>argv[3] - neural network mode, e.g. "train", "encrypt", "decrypt"</para>
/// <para>Upon "train" completion, files "encryptor.net" and "decryptor.net" will appear in the same directory where the file is.</para>
/// <para>Upon "encrypt" completion, file with the same name and .crypto extension will appear in the same directory, e.g. "myFile.txt" -> "myFile.txt.crypto".</para>
/// <para>Upon "decrypt" completion, file with the same name and .decrypted extension will appear in the same directory, e.g. "myFile.txt.crypto" -> "myFile.txt.crypto.decrypted".</para>
/// </summary>
int main(int argc, const char * argv[])
{
	/*if (argc < 4) exit(1);

	std::string directoryPath = argv[1];
	if (directoryPath.back() != '\\' && directoryPath.back() != '/')
		directoryPath += '\\';

	std::string fileName = argv[2];

	std::string mode = argv[3];
	if (mode == "train")
		CryptoANN::train(directoryPath, fileName);
	else if (mode == "encrypt")
		CryptoANN::encrypt(directoryPath, fileName);
	else if (mode == "decrypt")
		CryptoANN::decrypt(directoryPath, fileName);
	else exit(-1);*/
	CryptoANN::train("D:/", "test.txt");
	return 0;
}