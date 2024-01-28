.RECIPEPREFIX==
.PHONY : clean
.PHONY : all

#--------------------------------------------------------------------------------
# Compiler, linker, flags etc.
CC=clang
CFLAGS=-g -Wall -Werror -Wpedantic -DDEBUG
LINKFLAGS=-ltkf

#--------------------------------------------------------------------------------
# File locations.
ROOT_DIR=/home/tkf/Projects/mini-pogo-c
SRC_DIR=$(ROOT_DIR)/src
O_DIR=$(ROOT_DIR)/obj
BIN_DIR=$(ROOT_DIR)/bin

#--------------------------------------------------------------------------------
# Executables

# mpc: "compiler" (m)ini (p)ogo (c)ompiler
MPC=$(BIN_DIR)/mpc
MPC_OBJS=mini-pogo.o binary-header.o compile.o parse.o lex.o symbol-table.o

# mpd: "disassembler" (m)ini (p)ogo (d)isassembler
MPD=$(BIN_DIR)/mpd
MPD_OBJS=disasm.o binary-header.o module.o

# mph: header printer (m)ini (p)ogo (h)eader
MPH=$(BIN_DIR)/mph
MPH_OBJS=binary-header.o header-print.o

# mpr: run compiled module (m)ini (p)ogo (r)un
MPR=$(BIN_DIR)/mpr
MPR_OBJS=binary-header.o module.o exec.o

#--------------------------------------------------------------------------------

all : $(MPC) $(MPH) $(MPD) $(MPR)

$(MPC) : $(foreach ofile, $(MPC_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS)

$(MPH) : $(foreach ofile, $(MPH_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS)

$(MPD) : $(foreach ofile, $(MPD_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS)

$(MPR) : $(foreach ofile, $(MPR_OBJS), $(O_DIR)/$(ofile))
= $(CC) -o $@ $^ $(LINKFLAGS) -lpthread

#--------------------------------------------------------------------------------

# files that have text (not source code) file dependencies.

$(SRC_DIR)/disasm.c : $(SRC_DIR)/opcode-enums.txt
= touch $@

$(SRC_DIR)/instruction.h : $(SRC_DIR)/opcode-enums.txt
= touch $@

$(SRC_DIR)/lex.c : $(SRC_DIR)/lex-enums.txt
= touch $@

$(SRC_DIR)/lex.h : $(SRC_DIR)/lex-enums.txt
= touch $@

$(SRC_DIR)/parse.c : $(SRC_DIR)/parse-node-enum.txt
= touch $@

$(SRC_DIR)/parse.h : $(SRC_DIR)/parse-node-enum.txt
= touch $@

# Ordinary compiles

$(O_DIR)/mini-pogo.o : $(SRC_DIR)/mini-pogo.c $(SRC_DIR)/lex.h $(SRC_DIR)/parse.h $(SRC_DIR)/compile.h $(SRC_DIR)/binary-header.h
= $(CC) $(CFLAGS) -DPROGRAM_NAME="mpc" -o $@ -c $<

$(O_DIR)/header-print.o : $(SRC_DIR)/header-print.c
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/disasm.o : $(SRC_DIR)/disasm.c $(SRC_DIR)/instruction.h $(SRC_DIR)/binary-header.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/binary-header.o: $(SRC_DIR)/binary-header.c $(SRC_DIR)/binary-header.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/compile.o: $(SRC_DIR)/compile.c $(SRC_DIR)/compile.h $(SRC_DIR)/instruction.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/parse.o: $(SRC_DIR)/parse.c $(SRC_DIR)/parse.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/lex.o: $(SRC_DIR)/lex.c $(SRC_DIR)/lex.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/symbol-table.o: $(SRC_DIR)/symbol-table.c $(SRC_DIR)/symbol-table.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/module.o: $(SRC_DIR)/module.c $(SRC_DIR)/module.h
= $(CC) $(CFLAGS) -o $@ -c $<

$(O_DIR)/exec.o: $(SRC_DIR)/exec.c $(SRC_DIR)/exec.h
= $(CC) $(CFLAGS) -o $@ -c $<
