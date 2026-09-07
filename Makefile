CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
TARGET := language3d_mvp
PC_CXXFLAGS := $(CXXFLAGS) -DL3D_PC_PROFILE
GENERATED_ASSET_SRC := build/generated_assets.cpp
SRC := engine/game.cpp engine/math.cpp engine/renderer.cpp engine/assets.cpp engine/api.cpp $(GENERATED_ASSET_SRC) platform/sdl2/main.cpp
INC := -Iengine
CORE_SRC := engine/game.cpp engine/math.cpp engine/renderer.cpp engine/assets.cpp engine/api.cpp

.PHONY: all clean run pc rp2040 rp2040-clean test-all core-test renderer-test asset-test generated-assets-test map-path-test
all: $(TARGET)

$(GENERATED_ASSET_SRC): assets/map.txt assets/textures.raw tools/embed_assets.py
	mkdir -p build
	python3 tools/embed_assets.py assets/map.txt assets/textures.raw $@

$(TARGET): $(SRC)
	$(CXX) $(PC_CXXFLAGS) $(INC) $(SDL_CFLAGS) $(SRC) $(SDL_LIBS) -o $@

run: $(TARGET)
	./$(TARGET)

pc: run

rp2040:
	@test -n "$(PICO_SDK_PATH)" || (echo "Set PICO_SDK_PATH, e.g. export PICO_SDK_PATH=$$HOME/pico-sdk"; exit 2)
	cmake -S . -B build-rp2040 -DLANGUAGE3D_PLATFORM=rp2040 -DPICO_SDK_PATH="$(PICO_SDK_PATH)" -DPICO_BOARD=pico
	cmake --build build-rp2040 -j2

rp2040-clean:
	rm -rf build-rp2040

clean:
	rm -f $(TARGET) /tmp/l3d_core_test
	rm -rf build

core-test:
	$(CXX) $(CXXFLAGS) -Iengine -DL3D_CORE_TEST $(CORE_SRC) tests/core_test.cpp -o /tmp/l3d_core_test
	/tmp/l3d_core_test

renderer-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/game.cpp engine/math.cpp engine/renderer.cpp engine/assets.cpp tests/renderer_test.cpp -o /tmp/l3d_renderer_test
	/tmp/l3d_renderer_test

asset-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/assets.cpp tests/asset_test.cpp -o /tmp/l3d_asset_test
	/tmp/l3d_asset_test

generated-assets-test: $(GENERATED_ASSET_SRC)
	$(CXX) $(CXXFLAGS) -Iengine engine/assets.cpp $(GENERATED_ASSET_SRC) tests/generated_assets_test.cpp -o /tmp/l3d_generated_assets_test
	/tmp/l3d_generated_assets_test

map-path-test:
	python3 tools/validate_map_path.py assets/map.txt 4 4 18 18

test-all: map-path-test core-test renderer-test asset-test generated-assets-test
	@echo "CORE HOST TESTS PASSED"
