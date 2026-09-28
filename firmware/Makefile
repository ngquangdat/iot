# GCC build for the AESL0213C (nRF52811 + S112 7.3.0) firmware.
# Run ./fetch-upstream.sh once to get the Nordic SDK.

UP      := upstream
SDK     := $(UP)/SDK/17.1.0_ddde560
BUILD   := build
TARGET  := aesl0213c
APP_VER ?= 1006

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size

SRC_APP := src/main.c \
  src/EPD/EPD_config.c src/EPD/EPD_driver.c src/EPD/EPD_service.c src/EPD/SSD16xx.c \
  src/GUI/GUI.c src/GUI/font_data.c

SRC_SDK := $(addprefix $(SDK)/, \
  components/ble/ble_advertising/ble_advertising.c \
  components/ble/ble_services/ble_dfu/ble_dfu.c \
  components/ble/ble_services/ble_dfu/ble_dfu_unbonded.c \
  components/ble/common/ble_advdata.c \
  components/ble/common/ble_conn_params.c \
  components/ble/common/ble_srv_common.c \
  components/ble/nrf_ble_gatt/nrf_ble_gatt.c \
  components/libraries/atomic/nrf_atomic.c \
  components/libraries/atomic_fifo/nrf_atfifo.c \
  components/libraries/atomic_flags/nrf_atflags.c \
  components/libraries/balloc/nrf_balloc.c \
  components/libraries/bootloader/dfu/nrf_dfu_svci.c \
  components/libraries/experimental_section_vars/nrf_section_iter.c \
  components/libraries/fds/fds.c \
  components/libraries/fstorage/nrf_fstorage.c \
  components/libraries/fstorage/nrf_fstorage_sd.c \
  components/libraries/log/src/nrf_log_frontend.c \
  components/libraries/log/src/nrf_log_str_formatter.c \
  components/libraries/memobj/nrf_memobj.c \
  components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
  components/libraries/ringbuf/nrf_ringbuf.c \
  components/libraries/scheduler/app_scheduler.c \
  components/libraries/sortlist/nrf_sortlist.c \
  components/libraries/strerror/nrf_strerror.c \
  components/libraries/timer/app_timer2.c \
  components/libraries/timer/drv_rtc.c \
  components/libraries/util/app_error.c \
  components/libraries/util/app_error_handler_gcc.c \
  components/libraries/util/app_error_weak.c \
  components/libraries/util/app_util_platform.c \
  components/libraries/util/nrf_assert.c \
  components/softdevice/common/nrf_sdh.c \
  components/softdevice/common/nrf_sdh_ble.c \
  components/softdevice/common/nrf_sdh_soc.c \
  external/fprintf/nrf_fprintf.c \
  external/fprintf/nrf_fprintf_format.c \
  integration/nrfx/legacy/nrf_drv_clock.c \
  integration/nrfx/legacy/nrf_drv_spi.c \
  modules/nrfx/drivers/src/nrfx_clock.c \
  modules/nrfx/drivers/src/nrfx_gpiote.c \
  modules/nrfx/drivers/src/nrfx_spi.c \
  modules/nrfx/drivers/src/nrfx_spim.c \
  modules/nrfx/drivers/src/nrfx_wdt.c \
  modules/nrfx/drivers/src/prs/nrfx_prs.c \
  modules/nrfx/mdk/system_nrf52811.c \
  modules/nrfx/soc/nrfx_atomic.c)

ASM := $(SDK)/modules/nrfx/mdk/gcc_startup_nrf52811.S

INC := src src/EPD src/GUI $(addprefix $(SDK)/, . \
  components/ble/common components/ble/ble_advertising components/ble/nrf_ble_gatt \
  components/ble/ble_services/ble_dfu components/libraries/atomic components/libraries/atomic_fifo \
  components/libraries/atomic_flags components/libraries/balloc components/libraries/bootloader \
  components/libraries/bootloader/ble_dfu components/libraries/bootloader/dfu components/libraries/delay \
  components/libraries/fstorage components/libraries/fds components/libraries/experimental_section_vars \
  components/libraries/log components/libraries/log/src components/libraries/memobj \
  components/libraries/mutex components/libraries/pwr_mgmt components/libraries/ringbuf \
  components/libraries/sortlist components/libraries/scheduler components/libraries/strerror \
  components/libraries/svc components/libraries/timer components/libraries/util \
  components/softdevice/common components/softdevice/s112/headers components/softdevice/s112/headers/nrf52 \
  components/toolchain/cmsis/include external/fprintf integration/nrfx integration/nrfx/legacy \
  modules/nrfx modules/nrfx/mdk modules/nrfx/drivers/include modules/nrfx/hal)

DEFS := -DAPP_TIMER_V2 -DAPP_TIMER_V2_RTC1_ENABLED -DCONFIG_GPIO_AS_PINRESET -DFLOAT_ABI_SOFT \
  -DNRF52811_XXAA -DNRFX_COREDEP_DELAY_US_LOOP_CYCLES=3 -DNRF_DFU_SVCI_ENABLED -DNRF_DFU_TRANSPORT_BLE=1 \
  -DNRF_SD_BLE_API_VERSION=7 -DS112 -DSOFTDEVICE_PRESENT -D__HEAP_SIZE=0 -D__STACK_SIZE=2048

CPU    := -mcpu=cortex-m4 -mthumb -mabi=aapcs -mfloat-abi=soft
CFLAGS := $(CPU) $(DEFS) $(addprefix -I,$(INC)) -Os -g3 -std=gnu11 -Wall -Wno-unused-function \
  -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin -fshort-enums
ASFLAGS := $(CPU) $(DEFS) -D__HEAP_SIZE=0 -D__STACK_SIZE=2048
LDFLAGS := $(CPU) -Lsrc -L$(SDK)/modules/nrfx/mdk -Tapp.ld -Wl,--gc-sections --specs=nano.specs \
  -Wl,-Map=$(BUILD)/$(TARGET).map

OBJS := $(patsubst %.c,$(BUILD)/obj/%.o,$(SRC_APP) $(SRC_SDK)) $(BUILD)/obj/startup.o

all: $(BUILD)/$(TARGET).hex

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/startup.o: $(ASM)
	@mkdir -p $(dir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) src/app.ld
	@$(CC) $(LDFLAGS) $(OBJS) -lc -lnosys -lm -o $@
	@$(SIZE) $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	@$(OBJCOPY) -O ihex $< $@

# Signed Secure-DFU package for OTA over Bluetooth (nRF Connect / nRF DFU app).
ota: $(BUILD)/$(TARGET).hex
	python3 tools/make_ota.py $(BUILD)/$(TARGET).hex $(UP)/tools/priv.pem $(APP_VER) $(BUILD)/$(TARGET)-ota.zip

clean:
	rm -rf $(BUILD)

.PHONY: all ota clean
