#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
namespace fs = std::filesystem;

std::string extract_tlsh(const std::string &json_string) {
  const std::string tlsh_key = "\"tlsh\":";
  size_t pos = json_string.find(tlsh_key);
  if (pos == std::string::npos)
    return "";

  pos += tlsh_key.length();
  size_t start = json_string.find("\"", pos);
  if (start == std::string::npos)
    return "";

  start++;
  size_t end = json_string.find("\"", start);
  if (end == std::string::npos)
    return "";

  return json_string.substr(start, end - start);
}

std::unordered_map<std::string, std::string>
process_json_files(const std::string &directory) {
  std::unordered_map<std::string, std::string> tlsh_map;
  for (const auto &entry : fs::directory_iterator(directory)) {
    if (entry.path().extension() == ".json") {
      std::string filename = entry.path().filename().string();
      std::ifstream file(entry.path());
      if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        std::string file_tlsh = extract_tlsh(content);
        if (!file_tlsh.empty()) {
          std::cout << "File: " << filename << ", TLSH: " << file_tlsh
                    << std::endl;
          tlsh_map[file_tlsh] = filename;
        } else {
          std::cout << "File: " << filename << ", TLSH not found" << std::endl;
        }
      } else {
        std::cerr << "Unable to open file: " << filename << std::endl;
      }
    }
  }
  return tlsh_map;
}
void print_tlsh_map(
    const std::unordered_map<std::string, std::string> &tlsh_map) {
  for (const auto &[tlsh, filename] : tlsh_map) {
    std::cout << "TLSH: " << tlsh << ", File: " << filename << std::endl;
  }
}

int main() {
  std::string directory = "../signature_db";

  std::cout << "Processing JSON files in directory: " << directory << std::endl;
  auto tlsh_map = std::move(process_json_files(directory));
  /*
    for (const auto &pair : *mymap) {
      std::cout << pair.first << ": " << pair.second << std::endl;
    }
  */
  print_tlsh_map(tlsh_map);
  return 0;
}
