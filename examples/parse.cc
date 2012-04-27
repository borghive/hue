// Parses a program into an Abstract Syntax Tree (AST).
// clang++ -std=c++11 -o parse parse.cc && ./parse
#include "../src/parse/FileInput.h"
#include "../src/parse/Tokenizer.h"
#include "../src/parse/TokenBuffer.h"
#include "../src/parse/Parser.h"

#include <iostream>

using namespace rsms;

int main(int argc, char **argv) {

  // A FileInput reads from a file, feeding bytes to a Tokenizer
  FileInput<> input(argc > 1 ? argv[1] : "program1.txt");
  if (input.failed()) {
    std::cerr << "Failed to open input." << std::endl;
    return 1;
  }
  
  // A tokenizer produce tokens parsed from a ByteInput
  Tokenizer tokenizer(&input);
  
  // A TokenBuffer reads tokens from a Tokenizer and maintains limited history
  TokenBuffer tokens(tokenizer);
  
  // A parser reads the token buffer and produce an AST
  Parser parser(tokens);
  
  // Parse the input into an AST
  ast::Function *module = parser.parse();
  
  // Check for errors
  if (!module) return 1;
  if (parser.errors().size() != 0) {
    std::cerr << parser.errors().size() << " errors." << std::endl;
    return 1;
  }
  
  // Print a textual representation of the AST
  std::cout << "\e[32;1mParsed module:\e[0m " << module->toString() << std::endl;

  return 0;
}