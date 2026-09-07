CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
TARGET := language3d_mvp
PC_CXXFLAGS := $(CXXFLAGS) -DL3D_PC_PROFILE
GENERATED_ASSET_SRC := build/generated_assets.cpp
GENERATED_DIALOGUE_SRC := build/generated_dialogue.cpp
GENERATED_ITEM_SRC := build/generated_items.cpp
SRC := engine/game.cpp engine/math.cpp engine/renderer.cpp engine/assets.cpp engine/api.cpp engine/world3.cpp $(GENERATED_ASSET_SRC) platform/sdl2/main.cpp platform/sdl2/platform_sdl2.cpp
INC := -Iengine -Iplatform/api
CORE_SRC := engine/game.cpp engine/math.cpp engine/renderer.cpp engine/assets.cpp engine/api.cpp

.PHONY: all clean run pc rp2040 rp2040-clean test-all core-test renderer-test asset-test generated-assets-test map-path-test world3-test platform-test entity-test npc-test schedule-test npc-dispatch-test interact-test dialogue-test item-test validate-content
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

world3-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/world3.cpp engine/math.cpp tests/world3_test.cpp -o /tmp/l3d_world3_test
	/tmp/l3d_world3_test

platform-test:
	$(CXX) $(CXXFLAGS) -Iplatform/api -Iplatform/null -Itests tests/platform_test.cpp platform/null/platform_null.cpp -o /tmp/l3d_platform_test
	/tmp/l3d_platform_test

entity-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/entity.cpp engine/math.cpp tests/entity_test.cpp -o /tmp/l3d_entity_test
	/tmp/l3d_entity_test

npc-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/npc.cpp engine/entity.cpp engine/math.cpp tests/npc_test.cpp -o /tmp/l3d_npc_test
	/tmp/l3d_npc_test

schedule-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/schedule.cpp tests/schedule_test.cpp -o /tmp/l3d_schedule_test
	/tmp/l3d_schedule_test

npc-dispatch-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/npc_dispatch.cpp tests/npc_dispatch_test.cpp -o /tmp/l3d_npc_dispatch_test
	/tmp/l3d_npc_dispatch_test

interact-test:
	$(CXX) $(CXXFLAGS) -Iengine engine/interact.cpp engine/math.cpp tests/interact_test.cpp -o /tmp/l3d_interact_test
	/tmp/l3d_interact_test

$(GENERATED_DIALOGUE_SRC): $(wildcard content/dialogues/*.dlg) tools/build_dialogue.py
	mkdir -p build
	python3 tools/build_dialogue.py content/dialogues $@

dialogue-test: $(GENERATED_DIALOGUE_SRC)
	$(CXX) $(CXXFLAGS) -Iengine engine/dialogue.cpp $(GENERATED_DIALOGUE_SRC) tests/dialogue_test.cpp -o /tmp/l3d_dialogue_test
	/tmp/l3d_dialogue_test

$(GENERATED_ITEM_SRC): $(wildcard content/items/*.item) tools/build_items.py
	mkdir -p build
	python3 tools/build_items.py content/items $@

item-test: $(GENERATED_ITEM_SRC)
	$(CXX) $(CXXFLAGS) -Iengine engine/item.cpp $(GENERATED_ITEM_SRC) tests/item_test.cpp -o /tmp/l3d_item_test
	/tmp/l3d_item_test

validate-content:
	python3 tools/build_dialogue.py --check content/dialogues
	python3 tools/build_items.py --check content/items

test-all: map-path-test core-test renderer-test asset-test generated-assets-test world3-test platform-test entity-test npc-test schedule-test npc-dispatch-test interact-test dialogue-test item-test
	@echo "CORE HOST TESTS PASSED"
