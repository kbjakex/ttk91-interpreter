clang++ src/main.cpp src/compiler.cpp src/instructions.cpp src/interpreter.cpp -std=c++2a -Wall -Wextra -Wpedantic -Wno-gnu-label-as-value -O3 -march=native