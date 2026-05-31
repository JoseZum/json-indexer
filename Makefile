CC       := gcc
CXX      := g++
AR       := ar

CFLAGS   := -Wall -Wextra -Wpedantic -O2 -std=c11   -Iinclude
CXXFLAGS := -Wall -Wextra -Wpedantic -O2 -std=c++11 -Iinclude
LDFLAGS  :=

DIR_SRC      := src
DIR_INCLUDE  := include
DIR_BUILD    := build
DIR_BIN      := bin
DIR_TESTS    := tests

CABECERA      := $(DIR_INCLUDE)/libjsonindex.h
OBJ_LIB       := $(DIR_BUILD)/libjsonindex.o
OBJ_CLI       := $(DIR_BUILD)/jqindex.o
LIB_ESTATICA  := $(DIR_BUILD)/libjsonindex.a
BIN_CLI       := $(DIR_BIN)/jqindex

.PHONY: all clean test directorios

all: directorios $(LIB_ESTATICA) $(BIN_CLI)

directorios:
	@mkdir -p $(DIR_BUILD) $(DIR_BIN)

$(OBJ_LIB): $(DIR_SRC)/libjsonindex.c $(CABECERA)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_ESTATICA): $(OBJ_LIB)
	$(AR) rcs $@ $^

$(OBJ_CLI): $(DIR_SRC)/jqindex.cpp $(CABECERA)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_CLI): $(OBJ_CLI) $(LIB_ESTATICA)
	$(CXX) $(CXXFLAGS) $(OBJ_CLI) $(LIB_ESTATICA) -o $@ $(LDFLAGS)

test: all
	@bash $(DIR_TESTS)/ejecutar_pruebas.sh

clean:
	@rm -rf $(DIR_BUILD) $(DIR_BIN)
	@rm -f  $(DIR_TESTS)/*.jnx
	@echo "Limpieza completada."
