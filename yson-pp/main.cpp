#include <fstream>
#include <iostream>

#define YSON_IMPLEMENTATION
// #define YSON_NOTHROW
#include "yson.hpp"

using namespace std;
using namespace yson;

int main(int argc, char* argv[]) {
  if (1 == argc) {
    cout << "Usage: " << argv[0] << " <file> [<idk...>]\n";
    return EXIT_FAILURE;
  }
  char const* file = '-' == argv[1][0] && '\0' == argv[1][1] ? NULL : argv[1];

  value val;

  try {
    if (!file) cin >> val;
    else std::ifstream(file) >> val;
  }

  catch (std::runtime_error const& err) {
    cerr << err.what();
  }

  cout << "---\n";
  cout << val << "\n";
  cout << "---\n";

  return EXIT_SUCCESS;
}
