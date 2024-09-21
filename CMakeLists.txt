cmake_minimum_required(VERSION 3.29)
project(Proyecto2)

set(CMAKE_CXX_STANDARD 20)

add_executable(Proyecto2 main.cpp)

# Buscar e incluir SFML
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Enlazar las bibliotecas de SFML
target_link_libraries(Proyecto2 sfml-graphics sfml-window sfml-system)