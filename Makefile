# Makefile для сборки VGIK

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O2
LDFLAGS = -lm -lpthread

# Директории
SRC_DIR = core/base
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Исходные файлы
CONFIG_SRC = $(SRC_DIR)/config/config.c
EVENTS_SRC = $(SRC_DIR)/events/events.c
LOGGING_SRC = $(SRC_DIR)/logging/logging.c
SERVICE_MGR_SRC = $(SRC_DIR)/service_manager/service_manager.c

ALL_SRC = $(CONFIG_SRC) $(EVENTS_SRC) $(LOGGING_SRC) $(SERVICE_MGR_SRC)

# Объектные файлы
CONFIG_OBJ = $(OBJ_DIR)/config.o
EVENTS_OBJ = $(OBJ_DIR)/events.o
LOGGING_OBJ = $(OBJ_DIR)/logging.o
SERVICE_MGR_OBJ = $(OBJ_DIR)/service_manager.o

ALL_OBJ = $(CONFIG_OBJ) $(EVENTS_OBJ) $(LOGGING_OBJ) $(SERVICE_MGR_OBJ)

# Заголовочные файлы
INCLUDES = -I$(SRC_DIR)/config \
           -I$(SRC_DIR)/events \
           -I$(SRC_DIR)/logging \
           -I$(SRC_DIR)/service_manager

# Цели
.PHONY: all clean dirs test

all: dirs $(BIN_DIR)/libvgik_core.a

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Компиляция объектных файлов
$(OBJ_DIR)/config.o: $(CONFIG_SRC) $(SRC_DIR)/config/config.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/events.o: $(EVENTS_SRC) $(SRC_DIR)/events/events.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/logging.o: $(LOGGING_SRC) $(SRC_DIR)/logging/logging.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/service_manager.o: $(SERVICE_MGR_SRC) $(SRC_DIR)/service_manager/service_manager.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Создание статической библиотеки
$(BIN_DIR)/libvgik_core.a: $(ALL_OBJ)
	ar rcs $@ $^
	@echo "Built: $@"

clean:
	rm -rf $(BUILD_DIR)

test:
	@echo "Tests not yet implemented"

# Информация о проекте
info:
	@echo "VGIK Core Library Build System"
	@echo "Source files: $(words $(ALL_SRC))"
	@echo "Object files: $(words $(ALL_OBJ))"

