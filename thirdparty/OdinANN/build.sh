export ADDITIONAL_DEFINITIONS="-DBG_IO_THREAD -DDELTA_PRUNING"

# Compile and install liburing first
echo "Compiling liburing..."
cd third_party/liburing
./configure --prefix=$(pwd)/install --libdir=$(pwd)/install/lib
make -j
make install
cd ../../

LIBURING_INCLUDE_DIR=$(pwd)/third_party/liburing/install/include
LIBURING_LIB_DIR=$(pwd)/third_party/liburing/install/lib

rm -rf build
mkdir build
cd build
cmake -DCMAKE_C_FLAGS="-I$LIBURING_INCLUDE_DIR" -DCMAKE_CXX_FLAGS="-I$LIBURING_INCLUDE_DIR" -DCMAKE_EXE_LINKER_FLAGS="-L$LIBURING_LIB_DIR -Wl,-rpath,$LIBURING_LIB_DIR" -DCMAKE_SHARED_LINKER_FLAGS="-L$LIBURING_LIB_DIR -Wl,-rpath,$LIBURING_LIB_DIR" ..
make -j