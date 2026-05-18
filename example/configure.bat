rmdir /s /q build
mkdir build
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE:STRING=Release -G "MinGW Makefiles"
