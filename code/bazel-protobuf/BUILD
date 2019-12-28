load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_proto_library")

# For each .proto file, a proto_library target should be defined. This target
# is not bound to any particular language. Instead, it defines the dependency
# graph of the .proto files (i.e., proto imports) and serves as the provider
# of .proto source files to the protocol compiler.
#
# Remote repository "com_google_protobuf" must be defined to use this rule.
proto_library(
    name = "addressbook_proto",
    srcs = ["addressbook.proto"],
    deps = [
      "@com_google_protobuf//:timestamp_proto",
      "@com_google_protobuf//:struct_proto"
      ],
)


# The cc_proto_library rule generates C++ code for a proto_library rule. It
# must have exactly one proto_library dependency. If you want to use multiple
# proto_library targets, create a separate cc_proto_library target for each
# of them.
#
# Remote repository "com_google_protobuf_cc" must be defined to use this rule.
cc_proto_library(
    name = "addressbook_cc_proto",
    deps = [":addressbook_proto"],
)

# cc_library/cc_binary targets can depend on cc_proto_library targets.
cc_binary(
    name = "add_person_cpp",
    srcs = ["add_person.cc"],
    deps = [":addressbook_cc_proto"],
)

cc_binary(
    name = "list_people_cpp",
    srcs = ["list_people.cc"],
    deps = [":addressbook_cc_proto"],
)