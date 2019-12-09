load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "filter_proto",
    srcs = [
        "filter.proto",
    ],
)

cc_proto_library(
    name = "filter_cc_proto",
    deps = [":filter_proto"],
)

cc_library(
    name = "utils_lib",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
    ],
)

cc_library(
    name = "matcher_lib",
    srcs = [
        "matcher.cc",
    ],
    hdrs = [
        "matcher.h",
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
        ":filter_cc_proto",
        ":utils_lib",
    ],
)

cc_library(
    name = "transformation_lib",
    srcs = [
        "transformation.cc",
    ],
    hdrs = [
        "transformation.h",
        "inja.hpp",
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
        ":matcher_lib",
        ":utils_lib",
        ":filter_cc_proto",
        "@json//:json-lib",
        "@inja//:inja-lib",
    ],
)

cc_binary(
    name = "filter.wasm",
    srcs = [
        "filter.cc",
    ],
    additional_linker_inputs = ["@envoy_wasm_api//:jslib"],
    linkopts = [
        "--js-library",
        "external/envoy_wasm_api/proxy_wasm_intrinsics.js",
    ],
    deps = [
        ":filter_cc_proto",
        ":transformation_lib",
        ":matcher_lib",
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
    ],
)
