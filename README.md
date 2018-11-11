HackVac
=======

Hacking my heatpump for fun and...likely cold and ruins... (actually probably not cause this is going to be done super carefully but it's a fun image).

`third_party` contains the xtensa-esp32-elf gcc toolchain and the esp-idf download.
```
wget https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz
tar xzf xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz
git clone --recursive https://github.com/espressif/esp-idf.git
```

Next setup python virtualenv:
```
virtualenv venv
source venv/bin/activate
pip install -r
```

To build, from the `src` directory:
```
make -j5 all
```

Or to use the `Makefile`'s little serial monitor/flash interface run
```
make monitor
```

The initial configuration will have the esp32 create a config wifi network
called `hackvac_setup` with setup `cn105rulez`.
