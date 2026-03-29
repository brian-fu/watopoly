# --- OS Detection & Flag Assignment ---
UNAME_S := $(shell uname -s)

CXX = g++-14
BASE_CXXFLAGS = -std=c++20 -Wall -Wextra -g -fmodules-ts

ifeq ($(UNAME_S),Darwin)
	# macOS specific flags (Your friend's environment)
	SDKROOT = $(shell xcrun --show-sdk-path)
	OS_CXXFLAGS = -isysroot $(SDKROOT) -I/opt/X11/include -I/usr/X11/include
else ifeq ($(UNAME_S),Linux)
	# WSL2 / Linux specific flags (Your environment)
	OS_CXXFLAGS = 
endif

CXXFLAGS = $(BASE_CXXFLAGS) $(OS_CXXFLAGS)

# --- Object Definitions ---
OBJ_IFACE = src/types.o src/core.o src/board.o src/dice.o src/display.o src/game.o
OBJ_IMPL = src/core-impl.o src/board-impl.o src/dice-impl.o src/display-impl.o src/game-impl.o
OBJ = $(OBJ_IFACE) $(OBJ_IMPL) src/main.o
HEADER_GCM = .header_units_built

# --- Build Targets ---
all: watopoly

watopoly: $(HEADER_GCM) $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

$(HEADER_GCM):
	$(CXX) $(CXXFLAGS) -xc++-system-header -c iostream vector memory algorithm map random sstream fstream string utility functional
	touch $(HEADER_GCM)

src/types.o: src/types.cc $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/core.o: src/core.cc src/types.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/board.o: src/board.cc src/types.o src/core.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/dice.o: src/dice.cc src/types.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/display.o: src/display.cc src/types.o src/core.o src/board.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/game.o: src/game.cc src/types.o src/core.o src/board.o src/dice.o src/display.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/core-impl.o: src/core-impl.cc src/core.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/board-impl.o: src/board-impl.cc src/board.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/dice-impl.o: src/dice-impl.cc src/dice.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/display-impl.o: src/display-impl.cc src/display.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/game-impl.o: src/game-impl.cc src/game.o $(HEADER_GCM)
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/main.o: src/main.cc src/game.o
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ) watopoly gcm.cache $(HEADER_GCM)