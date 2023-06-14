SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/main
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

CPPFLAGS := -Iinclude -MMD -MP -g
CXXFLAGS := $(shell llvm-config --cxxflags) -Wall -Wextra
LDFLAGS  := -g
LDLIBS   := -lstdc++ -lm $(shell llvm-config --ldflags --system-libs --libs core)

.PHONY: all clean doc

all: $(EXE) doc

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

-include $(OBJ:.o=.d)

doc: doc/syntax.md

doc/syntax.md: doc/syntax
	java -jar rr/rr.war -md -out:$@ $^
