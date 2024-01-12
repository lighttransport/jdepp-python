rmdir /s /q build_32
mkdir build_32

cmake -G "Visual Studio 17 2022" -A Win32 -Bbuild_32 -H.
