cc_test(
    name = "test_mam",
    srcs = ["test_mam.c"],
    linkopts = ["-lpthread"],
    deps =
        [
            "//common/trinary:trit_tryte",
            "//mam/prototype:mam",
            "@unity",
        ],
)

cc_test(
    name = "test_mask",
    srcs = ["test_mask.c"],
    deps =
        [
            "//common/trinary:trit_tryte",
            "//mam/prototype:mask",
            "@unity",
        ],
)
