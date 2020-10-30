#ifndef LITERAL_REMOVER_H
#define LITERAL_REMOVER_H

#include "absl/status/status.h"
#include "zetasql/parser/parser.h"

absl::Status ReplaceLiteralsByParameters(std::string sql,
                                         const zetasql::ParserOptions &options,
                                         std::string *result_sql);
#endif