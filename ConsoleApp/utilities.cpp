#include "utilities.hpp"

#include <fstream>
#include <iostream>

std::vector<uint8_t> readBCode(const std::string& file)
{
  size_t length;
  std::fstream t{ file, std::ios::binary | std::ios::in | std::ios::ate };
  if (t) {
    length = t.tellg();
    t.close();
  } else {
    throw std::ios_base::failure{ "could not open " + file };
  }
  std::ifstream fin{ file, std::ios::in | std::ios::binary };
  if (!fin.is_open()) {
    throw std::ios_base::failure{ "could not open " + file };
  }
  std::vector<uint8_t> bCode(length);
  fin.read(reinterpret_cast<char*>(bCode.data()), length);
  if (!fin.eof() && fin.fail()) {
    throw std::ios_base::failure{ "failure while reading from " + file };
  }
  return bCode;
}

void manual()
{
  std::cout << "/e %file_path%    -    execute file_path\n"
            << "/a %src% %dst%    -    assembly src to dst\n";
}
