OBJ_DIR=obj
OUTPUT=bin

.PHONY: all

all: $(OUTPUT)/shell $(OUTPUT)/shell-debug

$(OUTPUT)/shell: shell.c $(OBJ_DIR)/util.o $(OBJ_DIR)/commands.o
	clang -o $(OUTPUT)/shell shell.c $(OBJ_DIR)/util.o $(OBJ_DIR)/commands.o

$(OUTPUT)/shell-debug: shell.c $(OBJ_DIR)/util.o $(OBJ_DIR)/commands.o
	clang -o $(OUTPUT)/shell-debug shell.c $(OBJ_DIR)/util.o $(OBJ_DIR)/commands.o -DDEBUG

$(OBJ_DIR)/commands.o: commands.h commands.c
	clang -c commands.c -o $(OBJ_DIR)/commands.o

$(OBJ_DIR)/util.o: util.h util.c
	clang -c util.c -o $(OBJ_DIR)/util.o