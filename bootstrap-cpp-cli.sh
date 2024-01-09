# llvm-mingw cross compile
# Assume Ninja is installed on your system
curdir=`pwd`

builddir=${curdir}/build_cli

rm -rf ${builddir}
mkdir ${builddir}

cd ${builddir} && CC=clang CXX=clang++ cmake \
  -DCMAKE_VERBOSE_MAKEFILE=1 \
  ..

cd ${curdir}
