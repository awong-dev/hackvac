HackVac
=======
[![Build Status](https://travis-ci.org/awong-dev/hackvac.svg?branch=master)](https://travis-ci.org/awong-dev/hackvac)

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

Unittests
```
TEST_COMPONENTS=". esp_cxx" make -f Makefile.host V=1 host_build/hackvac_host 
```

TODOs
=====
1. Wire up logging to udp port.
2. Sort out `main/event_log.h`
3. Sort out representation of device view of settings vs what is being pushed.
4. Unittest HalfDuplexChannel timings.
5. Deploy to device. Design real smoke and burn-in test.
6. Deploy for real.
