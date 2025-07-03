./prepare.sh
export CMAKE_PREFIX_PATH=$PREFIX
./configure PKG_CONFIG_PATH=./vcpkg/installed/x64-linux/lib/pkgconfig:$CONDA_PREFIX/lib/pkgconfig/ CFLAGS="-g -Wall -fPIC" --prefix=$PREFIX
make
make install

