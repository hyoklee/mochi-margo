./prepare.sh
export CMAKE_PREFIX_PATH=$PREFIX
git clone --single-branch https://github.com/hyoklee/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install argobots
./vcpkg install mercury
./vcpkg integrate install
cd ..
./configure PKG_CONFIG_PATH=./vcpkg/installed/x64-linux/lib/pkgconfig:$CONDA_PREFIX/lib/pkgconfig/ CFLAGS="-g -Wall" --prefix=$PREFIX
make
make install

