TARGET   := mini-shell
SRC_DIR  := src
BUILD    := build

# CFLAGS can be set on the command line (e.g. make CFLAGS="-O0 -g -Werror");
# the flags the project needs are always added.
CFLAGS   ?= -O2 -g
override CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -D_GNU_SOURCE -pthread -I$(SRC_DIR) -MMD -MP
override LDFLAGS += -pthread

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean test valgrind

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	bash tests/run_tests.sh ./$(TARGET)

valgrind: $(TARGET)
	bash tests/run_valgrind.sh ./$(TARGET)

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)
