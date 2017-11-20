HackVac
=======

Hacking my heatpump for fun and...likely cold and ruins... (actually probably not cause this is going to be done super carefully but it's a fun image).

`third_party` contains the xtensa-esp32-elf gcc toolchain and the esp-idf download.
```
wget https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz
tar xzf xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz
git clone --recursive https://github.com/espressif/esp-idf.git
```

To build:
```
EXTRA_CPPFLAGS='-DWIFI_SSID="\"your_ssid\"" -DWIFI_PASSWORD="\"your_password\""' make -j5 all
```
