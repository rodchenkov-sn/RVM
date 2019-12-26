#include <iostream>
#include <fstream>

#include "utilities.hpp"
#include "rasmTranslator.hpp"

#define RVM_NOEXCEPT
#include "rvm.hpp"

int main(int argc, char* argv[])
{
  try {
    switch (argc) {
    case 3: if (strcmp(argv[1], "/e") == 0) {
      auto program = readBCode(argv[2]);
      Rvm vm{};
      auto s = vm.run(program);
      if (!s.ok) {
        std::cerr << s.message << "\n";
        return 1;
      }
      break;
    }
    case 4: if (strcmp(argv[1], "/a") == 0) {
      std::ifstream src{ argv[2],  };
      std::ofstream dst{ argv[3], std::ofstream::out | std::ofstream::binary };
      RasmTranslator translator;
      auto s = translator.translate(src, dst);
      std::cout << s;
      break;
    }
    default:
      manual();
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
