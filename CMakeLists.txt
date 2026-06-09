cmake_minimum_required(VERSION 3.16)
project(ParallelImageScaler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Автоматична генерація Qt moc/uic/rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Шлях до Qt6 та OpenCV в MSYS2 UCRT64
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
endif()

# Пошук пакетів
find_package(Qt6 COMPONENTS Widgets Core Gui REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenMP REQUIRED)

# Збір вихідних файлів (Краще уникати GLOB_RECURSE в реальних проєктах, 
# але якщо використовуєте, додаємо CONFIGURE_DEPENDS, щоб CMake бачив нові файли)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.hpp"
)

add_executable(ParallelImageScaler WIN32 ${SOURCES})

# Сучасний підхід до додавання інкудів (замість застарілого include_directories)
target_include_directories(ParallelImageScaler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# --- МАКСИМАЛЬНА ОПТИМІЗАЦІЯ ДЛЯ GCC / MinGW ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ParallelImageScaler PRIVATE
        -O3                  # Максимальний рівень оптимізації швидкості
        -march=native        # Генерація інкудів під процесор, на якому йде компіляція (AVX2, AVX-512 тощо)
        -ffast-math          # Агресивна оптимізація математики (швидкі float-обчислення)
        -fomit-frame-pointer # Звільняє додатковий регістр для обчислень
        -flto                # Link-Time Optimization (оптимізація на етапі лінкування між файлами)
        -Wall -Wextra
    )
    
    # Прапорець LTO для лінкера
    target_link_options(ParallelImageScaler PRIVATE -flto)
endif()

# Лінківка бібліотек
target_link_libraries(ParallelImageScaler PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    ${OpenCV_LIBS}
    OpenMP::OpenMP_CXX
)