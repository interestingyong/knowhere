export ADDITIONAL_DEFINITIONS="-DBG_IO_THREAD -DDELTA_PRUNING"

rm -rf build
mkdir build
cd build
cmake ..
make -j
