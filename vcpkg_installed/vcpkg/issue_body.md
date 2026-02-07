Package: rapidjson:arm64-osx@2025-02-26

**Host Environment**

- Host: arm64-osx
- Compiler: AppleClang 17.0.0.17000603
- CMake Version: 4.2.3
-    vcpkg-tool version: 2025-12-16-2025.12.16
    vcpkg-scripts version: aa2d37682e 2026-02-06 (13 hours ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://github.com/Tencent/rapidjson/archive/24b5e7a8b27f42fa16b96fc70aade9106cf7102f.tar.gz -> Tencent-rapidjson-24b5e7a8b27f42fa16b96fc70aade9106cf7102f-2.tar.gz
Successfully downloaded Tencent-rapidjson-24b5e7a8b27f42fa16b96fc70aade9106cf7102f-2.tar.gz
-- Extracting source /Users/hassano/vcpkg/downloads/Tencent-rapidjson-24b5e7a8b27f42fa16b96fc70aade9106cf7102f-2.tar.gz
-- Using source at /Users/hassano/vcpkg/buildtrees/rapidjson/src/106cf7102f-b0877c68bd.clean
-- Configuring arm64-osx
-- Building arm64-osx-dbg
-- Building arm64-osx-rel
-- Fixing pkgconfig file: /Users/hassano/vcpkg/packages/rapidjson_arm64-osx/lib/pkgconfig/RapidJSON.pc
CMake Error at scripts/cmake/vcpkg_find_acquire_program.cmake:201 (message):
  Could not find pkg-config.  Please install it via your package manager:

      brew install pkg-config
Call Stack (most recent call first):
  scripts/cmake/vcpkg_fixup_pkgconfig.cmake:193 (vcpkg_find_acquire_program)
  ports/rapidjson/portfile.cmake:27 (vcpkg_fixup_pkgconfig)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "dependencies": [
    "rapidjson"
  ]
}

```
</details>
