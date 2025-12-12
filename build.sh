rm -rf build
mkdir build
cd build
cmake ..
make
chmod +x ./amalyzer
./amalyzer