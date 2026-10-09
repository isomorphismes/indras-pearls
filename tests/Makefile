ICK ?= ick
ICK_OPTIONS ?=
ICK_LINK_FLAGS ?= -fno-link-libatomic
BUILD ?= build/host
C_SOURCE_DIR = android/app/src/main/cpp
HEADERS = $(wildcard $(C_SOURCE_DIR)/*.h)
HOST_FLAGS = -std=c17 -Wall -Wextra -Werror -pedantic -O2 -I$(C_SOURCE_DIR)

.PHONY: test arm-boundary
test: $(BUILD)/mobius-math $(BUILD)/original-kleinian $(BUILD)/original-trace-controls $(BUILD)/parameter-controls.o
	$(BUILD)/mobius-math
	$(BUILD)/original-kleinian
	$(BUILD)/original-trace-controls

$(BUILD):
	mkdir -p "$@"

$(BUILD)/mobius-math: $(C_SOURCE_DIR)/mobius_math.c $(C_SOURCE_DIR)/renderer_packet.c tests/mobius_math_test.c $(HEADERS) | $(BUILD)
	$(ICK) $(ICK_OPTIONS) $(HOST_FLAGS) $(filter %.c,$^) $(ICK_LINK_FLAGS) -lm -o "$@"

$(BUILD)/original-kleinian: $(C_SOURCE_DIR)/original_kleinian.c tests/original_kleinian_test.c $(HEADERS) | $(BUILD)
	$(ICK) $(ICK_OPTIONS) $(HOST_FLAGS) $(filter %.c,$^) $(ICK_LINK_FLAGS) -lm -o "$@"

$(BUILD)/original-trace-controls: $(C_SOURCE_DIR)/original_kleinian.c $(C_SOURCE_DIR)/original_trace_controls.c tests/original_trace_controls_test.c $(HEADERS) | $(BUILD)
	$(ICK) $(ICK_OPTIONS) $(HOST_FLAGS) $(filter %.c,$^) $(ICK_LINK_FLAGS) -lm -o "$@"

# Retained historical controls are compiled independently; they are not restored
# to the production application's two-trace interaction path.
$(BUILD)/parameter-controls.o: $(C_SOURCE_DIR)/parameter_controls.c $(HEADERS) | $(BUILD)
	$(ICK) $(ICK_OPTIONS) $(HOST_FLAGS) -c "$<" -o "$@"

ICK_STAGE_ROOT ?= build/ick
ICK_ARMV7 ?= $(ICK_STAGE_ROOT)/armeabi-v7a/bin/arm-linux-gnueabi-gcc
ICK_ARMV7_OPTIONS ?=
ARM_BUILD ?= build/ick/armeabi-v7a
NDK ?= $(ANDROID_HOME)/ndk/29.0.14206865
NDK_BIN = $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
ARM_NDK_CC = $(NDK_BIN)/armv7a-linux-androideabi21-clang
ARM_TARGET = -march=armv7-a -mthumb -mfpu=neon -mfloat-abi=softfp
ARM_BUILTIN_INCLUDE = $(shell "$(ICK_ARMV7)" $(ICK_ARMV7_OPTIONS) -print-file-name=include)
ARM_FLAGS = $(ARM_TARGET) -std=c11 -O2 -fPIC -nostdinc -isystem "$(ARM_BUILTIN_INCLUDE)" -I$(C_SOURCE_DIR)
ARM_PROBE_FLAGS = --sysroot=$(NDK)/toolchains/llvm/prebuilt/linux-x86_64/sysroot -isystem $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/arm-linux-androideabi -isystem $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include -D__ANDROID__ -D__ANDROID_API__=21 -D__ANDROID_MIN_SDK_VERSION__=21 -DBIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD

arm-boundary: $(ARM_BUILD)/libindras-original-kleinian.so

$(ARM_BUILD):
	mkdir -p "$@"

$(ARM_BUILD)/original_kleinian.o: $(C_SOURCE_DIR)/original_kleinian.c $(HEADERS) | $(ARM_BUILD)
	$(ICK_ARMV7) $(ICK_ARMV7_OPTIONS) $(ARM_FLAGS) -ffreestanding -DINDRAS_FREESTANDING_MATH -S "$<" -o "$@.s"
	$(ARM_NDK_CC) $(ARM_TARGET) -c "$@.s" -o "$@"

$(ARM_BUILD)/original_kleinian_ndk_probe.o: tests/original_kleinian_ndk_probe.c $(HEADERS) | $(ARM_BUILD)
	$(ICK_ARMV7) $(ICK_ARMV7_OPTIONS) $(ARM_FLAGS) $(ARM_PROBE_FLAGS) -S "$<" -o "$@.s"
	$(ARM_NDK_CC) $(ARM_TARGET) -c "$@.s" -o "$@"

$(ARM_BUILD)/libindras-original-kleinian.so: $(ARM_BUILD)/original_kleinian.o $(ARM_BUILD)/original_kleinian_ndk_probe.o
	$(ARM_NDK_CC) -shared -Wl,-z,defs -Wl,-z,text -Wl,--fatal-warnings $^ -lm -o "$@"
