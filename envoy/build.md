# Envoy daily command set

## Test a single use case

```
bazel test //test/integration:http2_upstream_integration_test \
--test_arg=--gtest_filter="IpVersions/Http2UpstreamIntegrationTest.RouterRequestAndResponseWithBodyNoBuffer/IPv6" --test_arg="-l trace"
```

## Install pre-commit

```
./support/bootstrap
```

## Print stacktrace

```
bazel test -c dbg //test/server:backtrace_test
--run_under=`pwd`/tools/stack_decode.py --strategy=TestRunner=standalone
--cache_test_results=no --test_output=all
```

## Gdb single test

```
tools/bazel-test-gdb //test/common/http:async_client_impl_test -c dbg
```

## Release build

```
bazel --bazelrc=/dev/null build -c opt //source/exe:envoy-static.stripped.stamped
```


