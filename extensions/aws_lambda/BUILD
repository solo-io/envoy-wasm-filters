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

cc_binary(
    name = "filter.wasm",
    srcs = [
        "aws_lambda_wasm_filter.cc",
        "aws_lambda_wasm_filter.h",
    ],
    additional_linker_inputs = ["@envoy_wasm_api//:jslib"],
    linkopts = [
        "--js-library",
        "external/envoy_wasm_api/proxy_wasm_intrinsics.js",
    ],
    deps = [
        ":filter_cc_proto",
        ":aws_authenticator_lib",
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
    ],
)

cc_library(
    name = "aws_authenticator_lib",
    srcs = [
        "aws_authenticator.cc",
        "aws_authenticator.h"
    ],
    deps = [
        "@envoy_wasm_api//:proxy_wasm_intrinsics",
        "@openssl//:ssl",
    ],
)