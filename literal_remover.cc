// Forked from
// https://github.com/google/zetasql/blob/master/zetasql/analyzer/literal_remover.cc
//
// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Implements ReplaceLiteralsByParameters method declared in analyzer.h.
// Tested by parse_locations.test.

// modified

#include "literal_remover.h"
#include "absl/container/node_hash_set.h"
#include "zetasql/parser/parser.h"
#include "zetasql/public/type.h"
#include <algorithm>
#include <iostream>
#include <memory>

typedef std::map<const zetasql::ASTExpression *, std::string>
    LiteralReplacementMap;
typedef std::map<std::string, std::string> GeneratedParameterMap;

bool isLiteralRecursive(const zetasql::ASTNode *node) {
  switch (node->node_kind()) {
  case zetasql::AST_STRING_LITERAL:
  case zetasql::AST_BYTES_LITERAL:
  case zetasql::AST_INT_LITERAL:
  case zetasql::AST_FLOAT_LITERAL:
  case zetasql::AST_NUMERIC_LITERAL:
  case zetasql::AST_DATE_OR_TIME_LITERAL:
  case zetasql::AST_BOOLEAN_LITERAL:
    return true;
  case zetasql::AST_ARRAY_CONSTRUCTOR: {
    auto range = node->GetAsOrDie<zetasql::ASTArrayConstructor>()->elements();
    return std::all_of(range.begin(), range.end(), isLiteralRecursive);
  }
  case zetasql::AST_STRUCT_CONSTRUCTOR_WITH_KEYWORD: {
    auto range =
        node->GetAsOrDie<zetasql::ASTStructConstructorWithKeyword>()->fields();
    return std::all_of(range.begin(), range.end(), [](auto v) {
      return isLiteralRecursive(v->expression());
    });
  }
  case zetasql::AST_STRUCT_CONSTRUCTOR_WITH_PARENS: {
    auto range = node->GetAsOrDie<zetasql::ASTStructConstructorWithParens>()
                     ->field_expressions();
    return std::all_of(range.begin(), range.end(), isLiteralRecursive);
  }
  default:
    return false;
    break;
  }
}
static zetasql::TypeKind ASTNodeToTypeKind(const zetasql::ASTExpression *expr) {
  zetasql::TypeKind type_kind;
  switch (expr->node_kind()) {
  case zetasql::AST_STRING_LITERAL:
    return zetasql::TYPE_STRING;
  case zetasql::AST_BYTES_LITERAL:
    return zetasql::TYPE_BYTES;
  case zetasql::AST_INT_LITERAL:
    return zetasql::TYPE_INT64;
  case zetasql::AST_FLOAT_LITERAL:
    return zetasql::TYPE_DOUBLE;
  case zetasql::AST_NUMERIC_LITERAL:
    return zetasql::TYPE_NUMERIC;
  case zetasql::AST_BOOLEAN_LITERAL:
    return zetasql::TYPE_BOOL;
  case zetasql::AST_DATE_OR_TIME_LITERAL:
    return expr->GetAsOrDie<zetasql::ASTDateOrTimeLiteral>()->type_kind();
  case zetasql::AST_ARRAY_CONSTRUCTOR:
    return zetasql::TYPE_ARRAY;
  case zetasql::AST_STRUCT_CONSTRUCTOR_WITH_KEYWORD:
  case zetasql::AST_STRUCT_CONSTRUCTOR_WITH_PARENS:
    return zetasql::TYPE_STRUCT;
  default:
    return zetasql::TYPE_UNKNOWN;
  }
}
static std::string
GenerateParameterName(const zetasql::ASTExpression *literal,
                      const zetasql::LanguageOptions *options, int *index) {

  std::string type_name = zetasql::Type::TypeKindToString(
      ASTNodeToTypeKind(literal), options->product_mode());
  std::string param_name;
  param_name = absl::StrCat("_p", (*index)++, "_", type_name);
  return param_name;
}

struct ASTNodeParseLocationComparator {
  bool operator()(const zetasql::ASTNode *l1, const zetasql::ASTNode *l2) {
    return l1->GetParseLocationRange().start() <
           l2->GetParseLocationRange().start();
  }
};

// Returns true if the given literals occur at the same location and have the
// same value (and hence type). Such literals are created in analytical
// functions. The ParseLocationRange must be set on all compared literals.
static bool IsSameNode(const zetasql::ASTExpression *a,
                       const zetasql::ASTExpression *b) {
  const zetasql::ParseLocationRange &location_a = a->GetParseLocationRange();
  const zetasql::ParseLocationRange &location_b = b->GetParseLocationRange();
  return location_a == location_b && a->DebugString() == b->DebugString();
}

absl::Status ReplaceLiteralsByParameters(
    const std::string &sql, const zetasql::ParserOptions &options,
    const zetasql::ASTStatement *stmt,

    LiteralReplacementMap *literal_map,
    GeneratedParameterMap *generated_parameters, std::string *result_sql) {
  ZETASQL_CHECK(stmt != nullptr);
  literal_map->clear();
  generated_parameters->clear();
  result_sql->clear();

  // Collect all <literals> that are for options (hints) that have parse
  // locations.
  std::vector<const zetasql::ASTNode *> option_nodes;
  stmt->GetDescendantsWithKinds({zetasql::AST_HINT_ENTRY}, &option_nodes);
  std::vector<const zetasql::ASTExpression *> ignore_options_literals;
  for (const zetasql::ASTNode *node : option_nodes) {
    const zetasql::ASTHintEntry *option = node->GetAs<zetasql::ASTHintEntry>();
    ignore_options_literals.push_back(option->value());
  }

  // Collect all <literals> that have a parse location.
  std::vector<const zetasql::ASTNode *> literal_nodes;
  stmt->GetDescendantsWithKinds(
      {zetasql::AST_STRING_LITERAL, zetasql::AST_BYTES_LITERAL,
       zetasql::AST_INT_LITERAL, zetasql::AST_FLOAT_LITERAL,
       zetasql::AST_NUMERIC_LITERAL, zetasql::AST_DATE_OR_TIME_LITERAL,
       zetasql::AST_ARRAY_CONSTRUCTOR, zetasql::AST_BOOLEAN_LITERAL,
       zetasql::AST_STRUCT_CONSTRUCTOR_WITH_KEYWORD,
       zetasql::AST_STRUCT_CONSTRUCTOR_WITH_PARENS},
      &literal_nodes);
  std::vector<const zetasql::ASTExpression *> literals;
  for (const zetasql::ASTNode *node : literal_nodes) {
    const zetasql::ASTExpression *literal =
        node->GetAs<zetasql::ASTExpression>();
    if (std::find_if(ignore_options_literals.begin(),
                     ignore_options_literals.end(),
                     [&](auto v) { return IsSameNode(v, literal); }) ==
            ignore_options_literals.end() &&
        isLiteralRecursive(literal)) {
      literals.push_back(literal);
    }
  }
  std::sort(literals.begin(), literals.end(), ASTNodeParseLocationComparator());

  // <literals> are ordered by parse location. The loop below constructs
  // <result_sql> by appending the <replacement> string for each encountered
  // literal.
  zetasql::ParseLocationTranslator translator(sql);
  int prefix_offset = 0;   // Offset in <sql> of the text preceding the literal.
  int parameter_index = 0; // Index used to generate unique parameter names.
  std::string parameter_name; // Most recently used parameter name.
  for (int i = 0; i < literals.size(); ++i) {
    const zetasql::ASTExpression *literal = literals[i];
    const zetasql::ParseLocationRange *location =
        &literal->GetParseLocationRange();
    ZETASQL_RET_CHECK(location != nullptr);
    const int first_offset = location->start().GetByteOffset();
    const int last_offset = location->end().GetByteOffset();
    ZETASQL_RET_CHECK(first_offset >= 0 && last_offset > first_offset &&
                      last_offset <= sql.length());
    // Since literals are ordered by location, literals representing the same
    // input location are guaranteed to be consecutive.
    if (i > 0 && IsSameNode(literal, literals[i - 1])) {
      // Each occurrence of a literal maps to the same parameter name.
      ZETASQL_RET_CHECK(zetasql_base::InsertIfNotPresent(literal_map, literal,
                                                         parameter_name));
      continue;
    }
    if (!(prefix_offset == 0 || prefix_offset < last_offset))
      continue;
    // STRUCT literals can overlap in parser AST
    // ZETASQL_RET_CHECK(prefix_offset == 0 || prefix_offset < last_offset)
    //    << "Parse locations of literals are broken:"
    //    << "\nQuery: " << sql << "\nResolved AST: " << stmt->DebugString();
    parameter_name = GenerateParameterName(literal, options.language_options(),
                                           &parameter_index);

    absl::StrAppend(result_sql,
                    sql.substr(prefix_offset, first_offset - prefix_offset),
                    "@", parameter_name);
    // Add a space after the parameter name if the original literal was followed
    // by a character that can occur in an identifier or begin of a hint. This
    // is required in expressions like x='foobar'AND <other condition>.
    if (last_offset < sql.size()) {
      char ch = sql[last_offset];
      if (absl::ascii_isalnum(ch) || ch == '_' || ch == '@') {
        absl::StrAppend(result_sql, " ");
      }
    }

    ZETASQL_RET_CHECK(
        zetasql_base::InsertIfNotPresent(literal_map, literal, parameter_name));
    ZETASQL_RET_CHECK(zetasql_base::InsertIfNotPresent(
        generated_parameters, parameter_name, literal->DebugString()))
        << parameter_name;
    prefix_offset = last_offset;
  }
  absl::StrAppend(result_sql, sql.substr(prefix_offset));
  return absl::OkStatus();
}

absl::Status ReplaceLiteralsByParameters(std::string sql,
                                         const zetasql::ParserOptions &options,
                                         std::string *result_sql) {
  LiteralReplacementMap lremap;
  GeneratedParameterMap gpmap;

  std::unique_ptr<zetasql::ParserOutput> output;

  auto status = zetasql::ParseStatement(sql, options, &output);
  if (!status.ok()) {
    std::cout << "Error " << status.message() << std::endl;
    return status;
  }
  return ReplaceLiteralsByParameters(sql, options, output.get()->statement(),
                                     &lremap, &gpmap, result_sql);
}