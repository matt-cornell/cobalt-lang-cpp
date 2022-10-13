#include "test.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
int main() {
  using namespace test::test_builders;
  test::tester {"cobalt", {
    {"tokenizer", {
      {"identifiers", mktest(&tests::tokenizer::identifiers)-finish},
      {"strings", mktest(&tests::tokenizer::strings)-finish},
      {"macros", mktest(&tests::tokenizer::macros)-finish}
    }},
    {"parser", {
      {"modules", mktest(&tests::parser::modules)-finish}
    }},
    {"codegen"},
    {"JIT"}
  }}();
}
