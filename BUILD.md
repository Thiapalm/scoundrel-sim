# scoundrel-sim Build Instructions

This project is a simulator executable that depends on `scoundrel-core` for game logic.

## Build with CMake

1. Open a shell in this folder:
   ```powershell
   cd f:\repos\software\thiago\cpp\scoundrelProj\scoundrel-sim
   ```
2. Create a build directory and configure CMake:
   ```powershell
   cmake -B build -S .
   ```
3. Build the simulator executable:
   ```powershell
   cmake --build build --config Release -- /m
   ```

## How it links against `scoundrel-core`

The `CMakeLists.txt` in this folder includes `scoundrel-core` as a sibling subdirectory:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../scoundrel-core scoundrel-core)
```

The simulator target then links directly against the `ScoundrelCore` library:

```cmake
target_link_libraries(ScoundrelSimulator PRIVATE ScoundrelCore)
```

## Required files when `scoundrel-core` is not present

This project currently depends on the `scoundrel-core` source tree or an equivalent prebuilt library and headers. The easiest working setup is to keep the sibling source tree in place and preserve the relative path used by CMake.

Required items:

- `scoundrel-core/CMakeLists.txt`
- `scoundrel-core/include/...` public `scoundrel` headers
- `scoundrel-core/src/...` core sources and implementation files
- any additional `scoundrel-core` resources required by the library

If `scoundrel-core` is missing, CMake will fail during configuration because this project currently uses `add_subdirectory(...)`. To build without the source tree, you would need a compatible built `ScoundrelCore` static library plus matching headers, and you would also need to update the CMakeLists to find and link that library instead of using `add_subdirectory(...)`.

## Notes

- `scoundrel-core` is built automatically when you build this project.
- Keep the sibling directory structure intact for CMake to find `scoundrel-core`.
- If you move the folders, update the path in `add_subdirectory(...)` accordingly.
