cmake_minimum_required(VERSION 3.16)
project(ParallelImageScaler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic Qt moc/uic/rcc compilation
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Tell CMake where to find Qt6 and OpenCV on Windows MSYS2
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
endif()

# Find packages
find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenMP REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Sources
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "include/*.hpp"
)

add_executable(ParallelImageScaler WIN32 ${SOURCES})

# Target compiler options
target_compile_options(ParallelImageScaler PRIVATE -Wall -Wextra)

# Link libraries
target_link_libraries(ParallelImageScaler PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${OpenCV_LIBS}
    OpenMP::OpenMP_CXX
)
