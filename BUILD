load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "main",
    srcs = [
        "literal_remover.cc",
        "literal_remover.h",
        "main.cc",
    ],
    deps = [
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_zetasql//zetasql/public:parse_helpers",
    ],
)
