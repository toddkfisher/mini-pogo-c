.RECIPEPREFIX==

CC=clang
CFLAGS=-g -Wall -DDEBUG
LINKFLAGS=-ltkf

ROOT_DIR=/home/tkf/mini-pogo-c
SRC_DIR=$(ROOT_DIR)/src
O_DIR=$(ROOT_DIR)/obj
BIN_DIR=$(ROOT_DIR)/bin

# mpc: "compiler" (m)ini (p)ogo (c)ompiler
MPC=$(BIN_DIR)/mpc
MPC_OBJS=mini-pogo.o binary-header.o compile.o parse.o lex.o symbol-table.o

# Header printer (m)ini (p)ogo (h)eader
MPH=$(BIN_DIR)/mph
MPH_OBJS=binary-header.o header-print.o

.PHONY : clean
.PHONY : all

all : $(MPC) $(MPH)

$(MPC) : $(foreach ofile, $(MPC_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS)

$(MPH) : $(foreach ofile, $(MPH_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS)

$(O_DIR)/mini-pogo.o : $(SRC_DIR)/mini-pogo.c
= $(CC) $(CFLAGS) -DPROGRAM_NAME="mpc" -o $@ -c $<

$(O_DIR)/header-print.o : $(SRC_DIR)/header-print.c
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/binary-header.o: $(SRC_DIR)/binary-header.c $(SRC_DIR)/binary-header.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/compile.o: $(SRC_DIR)/compile.c $(SRC_DIR)/compile.h $(SRC_DIR)/instruction.h $(SRC_DIR)/parse-node-enum.txt $(SRC_DIR)/opcode-enums.txt
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/parse.o: $(SRC_DIR)/parse.c $(SRC_DIR)/parse.h $(SRC_DIR)/lex-enums.txt
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/lex.o: $(SRC_DIR)/lex.c $(SRC_DIR)/lex.h $(SRC_DIR)/lex-enums.txt
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/symbol-table.o: $(SRC_DIR)/symbol-table.c $(SRC_DIR)/symbol-table.h
= $(CC) $(CFLAGS) -o $@ -c $<
