cmake_minimum_required(VERSION 3.20)
project(terpiece LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

# glm: reuse the copy cubeworld-mac already downloaded when it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/../cubeworld-mac/build/_deps/glm-src/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_GLM "${CMAKE_SOURCE_DIR}/../cubeworld-mac/build/_deps/glm-src")
endif()
FetchContent_Declare(glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# glTF 2.0 parser (single header, MIT)
FetchContent_Declare(cgltf
  URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.14.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# EXT_meshopt_compression decoder for the character .glb files (MIT)
FetchContent_Declare(meshoptimizer
  URL https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.21.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# JSON for manifest / voxel columns / save files (MIT)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# audio output + mp3 decoding (public domain / MIT-0)
FetchContent_Declare(miniaudio
  URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(glm json)
FetchContent_Populate(cgltf)
FetchContent_Populate(meshoptimizer)
FetchContent_Populate(miniaudio)

find_package(glfw3 3.4 REQUIRED)
find_package(OpenGL REQUIRED)

# every .cpp / .mm under src/ (re-globbed when files are added)
file(GLOB_RECURSE TERPIECE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.mm)
add_executable(terpiece
  ${TERPIECE_SOURCES}
  ${meshoptimizer_SOURCE_DIR}/src/vertexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/indexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/vertexfilter.cpp
)
target_include_directories(terpiece PRIVATE src)
target_include_directories(terpiece SYSTEM PRIVATE
  ${cgltf_SOURCE_DIR} ${meshoptimizer_SOURCE_DIR}/src ${miniaudio_SOURCE_DIR})
target_link_libraries(terpiece PRIVATE glfw glm::glm OpenGL::GL nlohmann_json::nlohmann_json
  "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText" "-framework ImageIO"
  "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
target_compile_options(terpiece PRIVATE $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall -Wextra -Wno-missing-field-initializers>)
set_source_files_properties(
  ${meshoptimizer_SOURCE_DIR}/src/vertexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/indexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/vertexfilter.cpp
  PROPERTIES COMPILE_OPTIONS "-w")
set_source_files_properties(src/gfx/image.mm src/ui/canvas2d.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_compile_definitions(terpiece PRIVATE GLFW_INCLUDE_NONE GL_SILENCE_DEPRECATION
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders"
  ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
  WEB_DIR="${CMAKE_SOURCE_DIR}/../patty-project/xmas-app/web")
