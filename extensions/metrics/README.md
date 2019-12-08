This compiles an example filter for envoy WASM.

# build filter
build with
```
bazel build :filter.wasm
```

Filter will be in:
```
./bazel-bin/filter.wasm
```

# build config descriptors

build descriptors with:
```
bazel build :filter_proto
```

Descriptors will be in:
```
./bazel-bin/filter_proto-descriptor-set.proto.bin
```

Note: 
on a mac, please run
```
xcode-select --install
```

and Potentially:
```
brew install python@2
```
as the python bundled with catalina may have issues with ssl certs.



Push:
```
./extend-envoy push gcr.io/solo-public/example-filter:v1 example/cpp/bazel-bin/filter.wasm example/cpp/bazel-bin/filter_proto-descriptor-set.proto.bin
```

load in to gloo:
```
kubectl edit -n gloo-system gateways.gateway.solo.io.v2 gateway-proxy-v2
```

vs
```
apiVersion: gateway.solo.io/v1
kind: VirtualService
metadata:
  name: default
  namespace: gloo-system
spec: 
  virtualHost:
    domains:
    - '*'
    routes:
    - matchers:
      - prefix: /
      routeAction:
        single:
          upstream:
            name: default-petstore-8080
            namespace: gloo-system
      options:
        prefixRewrite: /api/pets

```

load in to gloo:
```
kubectl edit -n gloo-system gateways.gateway.solo.io.v2 gateway-proxy-v2
```

set the httpGateway field like so:
```
  httpGateway:
    plugins:
      virtualServices:
        name: default
        namespace: gloo-system  
      extensions:
        configs:
          wasm:
            config: yuval
            image: gcr.io/solo-wasm/eitanya/example-filter:v1
            name: yuval
            root_id: add_header_root_id
```



# emscripten sdk
If you change the emscripten SDK, an sdk with PR merged is needed:
https://github.com/emscripten-core/emscripten/pull/9812/files