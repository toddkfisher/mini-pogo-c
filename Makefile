TARGET=mp
CC=clang
CFLAGS=-g -Wall -DDEBUG
LINKFLAGS=-ltkf
ROOT_DIR:= /home/tkf/Projects/mini-pogo-c
SRC_DIR:= $(ROOT_DIR)/src
O_DIR:= $(ROOT_DIR)/obj
BIN_DIR=$(ROOT_DIR)/bin
SRC_FILES=$(wildcard $(SRC_DIR)/*.c)
H_FILES=$(wildcard $(SRC_DIR)/*.h)
O_FILES=$(patsubst $(SRC_DIR)/%.c, $(O_DIR)/%.o, $(SRC_FILES))
.RECIPEPREFIX==

.PHONY : clean

$(BIN_DIR)/$(TARGET) : $(O_FILES)
= cd $(O_DIR) ; $(CC) -o $(BIN_DIR)/$(TARGET) *.o $(LINKFLAGS)

$(O_FILES) : $(SRC_FILES) $(H_FILES)
= cd $(O_DIR) ; $(CC) -c $(SRC_FILES) $(CFLAGS) ; cd ..

clean :
= rm -rf $(BIN_DIR)/* $(O_DIR)/*
