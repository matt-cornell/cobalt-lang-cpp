#include "test.hpp"
#include "tokenizer.hpp"
int main() {
  using namespace test::test_builders;
  test::tester {"cobalt", {
    {"tokenizer", {
      {"identifiers", mktest(&tests::tokenizer::identifiers)-finish},
      {"strings", mktest(&tests::tokenizer::strings)-finish},
      {"macros", mktest(&tests::tokenizer::macros)-finish}
    }},
    {"parser"},
    {"codegen"},
    {"JIT"}
  }}();
}