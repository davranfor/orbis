# C library for unixes

## Compile and install
```
make
sudo make install
# On linux
sudo ldconfig
```

## Test examples
```
cd examples/whatever
CFLAGS="-std=c11 -Wpedantic -Wall -Wextra -O2" LDLIBS="-lorbis" make demo && ./demo
```
