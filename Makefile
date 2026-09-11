#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := PokeBank-CFW
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := include

APP_TITLE       := PokeBank-CFW
APP_DESCRIPTION := Local Gen 1-7 Pokemon storage
APP_AUTHOR      := SHGliscor
ICON            := assets/icon.png
BANNER          := assets/banner.png
BANNER_AUDIO    := assets/banner.wav

APP_PRODUCT_CODE := CTR-H-PBCF
APP_UNIQUE_ID    := 0xBCF01

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS   += $(INCLUDE) -D__3DS__
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lctru -lm
LIBDIRS  := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                 $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_ICON := $(TOPDIR)/$(ICON)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh

.PHONY: all clean cia package

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia $(TARGET).lst $(TARGET).map dist

cia: all
	@command -v bannertool >/dev/null || (echo "ERROR: bannertool not found" && exit 1)
	@command -v makerom >/dev/null || (echo "ERROR: makerom not found" && exit 1)
	@test -f "$(ICON)" || (echo "ERROR: missing $(ICON)" && exit 1)
	@test -f "$(BANNER)" || (echo "ERROR: missing $(BANNER)" && exit 1)
	@test -f "$(BANNER_AUDIO)" || (echo "ERROR: missing $(BANNER_AUDIO)" && exit 1)
	@mkdir -p $(BUILD)
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i "$(ICON)" -o "$(BUILD)/icon.icn"
	@bannertool makebanner -i "$(BANNER)" -a "$(BANNER_AUDIO)" -o "$(BUILD)/banner.bnr"
	@makerom -f cia -target t -exefslogo \
		-o "$(TARGET).cia" \
		-elf "$(OUTPUT).elf" \
		-rsf "$(TOPDIR)/cia.rsf" \
		-icon "$(BUILD)/icon.icn" \
		-banner "$(BUILD)/banner.bnr" \
		-DAPP_TITLE="$(APP_TITLE)" \
		-DAPP_PRODUCT_CODE="$(APP_PRODUCT_CODE)" \
		-DAPP_UNIQUE_ID="$(APP_UNIQUE_ID)" \
		-DAPP_VERSION_MAJOR="0"
	@echo built ... $(TARGET).cia

package: cia
	@mkdir -p dist
	@cp $(TARGET).3dsx $(TARGET).smdh $(TARGET).cia dist/
	@cp README-TESTING.txt dist/
	@sha256sum dist/$(TARGET).3dsx dist/$(TARGET).cia > dist/SHA256SUMS.txt

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OFILES_SOURCES): $(HFILES)
$(OUTPUT).elf: $(OFILES)

%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

endif
#---------------------------------------------------------------------------------
