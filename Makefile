
# Project Hollow (3DS)
# Requires devkitARM, libctru, citro2d and citro3d

# Targets:
# make     -> project_hollow.3dsx
# make cia -> project_hollow.cia

.SECONDARY:

ifeq ($(strip $(DEVKITARM)),)
$(error "Set DEVKITARM, e.g. export DEVKITARM=/opt/devkitpro/devkitARM")
endif

include $(DEVKITARM)/3ds_rules

TARGET := project_hollow
TITLE  := Project Hollow
DESC   := Forsaken-like 3D survival game
AUTHOR := Hollow Dev

BUILD := build
SRC   := $(wildcard source/*.c)
OBJ   := $(patsubst source/%.c,$(BUILD)/%.o,$(SRC))

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS := -g -Wall -O2 -mword-relocations -ffunction-sections -fdata-sections $(ARCH) \
          -I$(CTRULIB)/include -I$(PORTLIBS)/include -D__3DS__ -std=gnu11

LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections -L$(CTRULIB)/lib -L$(PORTLIBS)/lib

LIBS := -lcitro2d -lcitro3d -lctru -lm

all: $(TARGET).3dsx

cia: $(TARGET).cia

$(BUILD):
	@mkdir -p $@

$(BUILD)/%.o: source/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

$(TARGET).smdh: assets/icon.png
	smdhtool --create "$(TITLE)" "$(DESC)" "$(AUTHOR)" assets/icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	3dsxtool $< $@ --smdh=$(TARGET).smdh

$(BUILD)/banner.bnr: assets/banner.png assets/audio.wav | $(BUILD)
	bannertool makebanner -i assets/banner.png -a assets/audio.wav -o $@

$(BUILD)/icon.icn: assets/icon.png | $(BUILD)
	bannertool makesmdh -s "$(TITLE)" -l "$(DESC)" -p "$(AUTHOR)" -i assets/icon.png -o $@

$(TARGET).cia: $(TARGET).elf $(BUILD)/banner.bnr $(BUILD)/icon.icn cia.rsf
	makerom -f cia -o $@ -elf $(TARGET).elf -rsf cia.rsf -icon $(BUILD)/icon.icn -banner $(BUILD)/banner.bnr -exefslogo -target t

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia

.PHONY: all cia clean
