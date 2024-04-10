#include <iostream>
#include <fstream>
#include <string>
#include <vector>

void distributeLines(const std::string& inputFilename, const std::string& outputLocation, int numOutputFiles, char delimiter=' ') {
  // Open input file for reading
  std::ifstream inputFile(inputFilename);
  if (!inputFile.is_open()) {
    std::cerr << "Error: Unable to open input file." << std::endl;
    return;
  }

  // Create output files
  std::vector<std::ofstream> outputFiles;
  for (int i = 0; i < numOutputFiles; ++i) {
    std::string outputFilename = outputLocation + "/output_" + std::to_string(i+1) + ".txt";
    outputFiles.emplace_back(outputFilename);
  }

  // Distribute lines from input file to output files
  std::string line;
  int fileIndex = 0;
  while (std::getline(inputFile, line)) {
    std::size_t pos = line.find(delimiter);
    std::string firstColumn = line.substr(0, pos);

    outputFiles[static_cast<unsigned long>(fileIndex % numOutputFiles)] << firstColumn << std::endl;
    ++fileIndex;
  }

  // Close all files
  for (auto& outputFile : outputFiles) {
    outputFile.close();
  }

  // Close input file
  inputFile.close();
}

int main(int argc, char* argv[]) {
  // Check command line arguments
  if (argc < 4 || argc > 5) {
    std::cout << argc << "\n";
    std::cerr << "Usage: " << argv[0] << " inputFilename outputLocation numOutputFiles" << std::endl;
    return 1;
  }

  // Parse command line arguments
  std::string inputFilename = argv[1];
  std::string outputLocation = argv[2];
  int numOutputFiles = std::stoi(argv[3]);
  char delimiter = ' ';
  if(argc == 5) {
    delimiter = argv[4][0];
  }
  // Distribute lines from input file to output files
  distributeLines(inputFilename, outputLocation, numOutputFiles, delimiter);

  std::cout << "Lines distributed evenly among " << numOutputFiles << " output files in " << outputLocation << "." << std::endl;

  return 0;
}
