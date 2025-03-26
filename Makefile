CC = gcc
CFLAGS = -Wall -g -Werror -std=c99
TARGET = myMonitoringTool
SRC = myMonitoringTool.c
OBJ = myMonitoringTool.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ): $(SRC) myMonitoringTool.h
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: help
help:
	@echo "all: Compile the default target"
	@echo "clean: Remove auto-generated files"
