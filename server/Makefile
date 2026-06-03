CC := gcc
CFLAGS := -std=c11 -Wpedantic -Wall -Wextra -Wmissing-prototypes -Wstrict-prototypes -Wconversion -Wshadow -Wcast-qual -Wnested-externs -Wformat=2
LDFLAGS := -lorbis -lsqlite3

ifdef ALLOW_INSECURE_TOKEN
CFLAGS += -DALLOW_INSECURE_TOKEN
endif

SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
DEP_DIR := $(OBJ_DIR)/.deps
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
DEP := $(patsubst $(SRC_DIR)/%.c,$(DEP_DIR)/%.d,$(SRC))
TARGET := server

all: release

debug: CFLAGS += -DDEBUG -g
debug: $(TARGET)

release: CFLAGS += -DNDEBUG -O2
release: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(DEP_DIR)
	$(CC) -I$(INC_DIR) $(CFLAGS) -MMD -MP -MF $(DEP_DIR)/$*.d -c -o $@ $<

$(OBJ_DIR) $(DEP_DIR):
	@mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(DEP)

.PHONY: all debug release clean

