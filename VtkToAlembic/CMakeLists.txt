cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 11)
 
include_directories(SYSTEM "/usr/include/OpenEXR")
include_directories(SYSTEM "/home/curran/GitHub/alembic/alembic_build3/lib" "/home/curran/GitHub/alembic/alembic/lib")
link_directories("/home/curran/GitHub/alembic/alembic_build3/lib/Alembic" "/home/curran/local/ilmbase/lib")

PROJECT(PolyDataGetPoint)
 
find_package(VTK REQUIRED NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_BUILDS_PATH NO_CMAKE_PACKAGE_REGISTRY NO_CMAKE_SYSTEM_PATH PATHS /home/curran/local/VTK-7.0)
include(${VTK_USE_FILE})
 
SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
SET(BUILD_SHARED_LIBRARIES OFF)
SET(CMAKE_EXE_LINKER_FLAGS "-static")
add_executable(PolyDataGetPoint PolyDataGetPoint)
 
if(VTK_LIBRARIES)
  target_link_libraries(PolyDataGetPoint ${VTK_LIBRARIES})
else()
  target_link_libraries(PolyDataGetPoint vtkHybrid vtkWidgets)
endif()

target_link_libraries(PolyDataGetPoint Alembic Half Imath Iex)
