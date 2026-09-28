# AESL0213C firmware (work in progress)

Based on [tsl0922/EPD-nRF5](https://github.com/tsl0922/EPD-nRF5) (GPL-3.0, commit 7e31961) for nRF52811 + S112 7.3.0.
`src/` currently holds the unmodified upstream application sources; the 2.13" SSD1680 driver and the
navigation command are not written yet.

Build (needs `arm-none-eabi-gcc`):

```sh
./fetch-upstream.sh   # Nordic SDK + tools
make
```

App flash region is 0x19000–0x27000 (FDS pages and the bootloader follow), so the upstream
feature set must be trimmed to fit.
