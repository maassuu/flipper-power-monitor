# Host unit tests for the hardware independent modules.
# Usage: make -C tests                       (build and run)
#        make -C tests EXTRA_CFLAGS=-O2      (add flags, required ones stay)
#        make -C tests clean

CC ?= cc
# Required flags are set with '=' so that a CFLAGS variable from the
# environment cannot silently drop -Werror or the sanitizers.
CFLAGS = -std=c11 -O1 -g -Wall -Wextra -Werror -Wdouble-promotion -Wshadow \
         -Wconversion -Wno-sign-conversion -fsanitize=address,undefined $(EXTRA_CFLAGS)
LDLIBS = -lm

SRC_DIR = ..
BUILD = build

APP_SRC = $(SRC_DIR)/pm_energy.c $(SRC_DIR)/pm_stats.c $(SRC_DIR)/pm_history.c \
          $(SRC_DIR)/pm_charge.c $(SRC_DIR)/pm_health.c $(SRC_DIR)/pm_format.c \
          $(SRC_DIR)/pm_csv.c $(SRC_DIR)/pm_settings.c
TEST_SRC = test_main.c test_energy.c test_stats.c test_history.c test_charge.c \
           test_health.c test_format_csv_settings.c

.PHONY: all run clean

all: run

$(BUILD)/pm_tests: $(APP_SRC) $(TEST_SRC) $(wildcard $(SRC_DIR)/*.h) test_framework.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -o $@ $(APP_SRC) $(TEST_SRC) $(LDLIBS)

run: $(BUILD)/pm_tests
	./$(BUILD)/pm_tests

clean:
	rm -rf $(BUILD)
