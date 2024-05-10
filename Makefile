CC=gcc

SRC_DIR=src
INC_DIR=include
BUILD_DIR=build
TARGET=$(BUILD_DIR)/fq

CFLAGS=-I$(INC_DIR) -Wall
LDFLAGS=

OBJ=$(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SRC = $(wildcard src/*.c)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	mkdir -p ~/.local/bin
	cp $(TARGET) ~/.local/bin/fq
