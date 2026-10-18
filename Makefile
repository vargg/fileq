CC=gcc

SRC_DIR=src
INC_DIR=include
BUILD_DIR=build
TARGET=$(BUILD_DIR)/fq

CFLAGS=-I$(INC_DIR) -Wall -MMD -MP
LDFLAGS=

OBJ=$(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
SRC = $(wildcard src/*.c)

TEST_DIR=tests
TEST_BUILD_DIR=$(BUILD_DIR)/tests
TEST_CFLAGS=-I$(INC_DIR) -Wall -Wextra -g -fsanitize=address,undefined
TESTS=$(TEST_BUILD_DIR)/test_args $(TEST_BUILD_DIR)/test_processing

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(TEST_BUILD_DIR):
	mkdir -p $@

# each test binary #includes the source it tests, so it depends on all of them
$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(TEST_DIR)/test.h $(SRC) $(wildcard $(INC_DIR)/*.h) | $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $< -o $@

test: $(TESTS) $(TARGET)
	@for t in $(TESTS); do $$t || exit 1; done
	@sh $(TEST_DIR)/cli.sh $(TARGET)

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET)
	mkdir -p ~/.local/bin
	cp $(TARGET) ~/.local/bin/fq

-include $(OBJ:.o=.d)
