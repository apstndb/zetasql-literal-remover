#include "literal_remover.h"

int main(int argc, char *argv[]) try {
  zetasql::LanguageOptions langopts;
  langopts.EnableMaximumLanguageFeatures();
  langopts.set_product_mode(zetasql::PRODUCT_EXTERNAL);

  zetasql::ParserOptions parser_options;
  parser_options.set_language_options(&langopts);

  std::string result_sql;
  if (auto status = ReplaceLiteralsByParameters(
          std::string(std::istreambuf_iterator<char>(std::cin >> std::noskipws),
                      {}),
          parser_options, &result_sql);
      !status.ok()) {
    std::cout << "Error " << status.message() << std::endl;
    return 1;
  }
  std::cout << result_sql;
} catch (std::exception e) {
  std::cout << e.what() << std::endl;
}