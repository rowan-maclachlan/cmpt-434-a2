########################################
# Rowan MacLachlan
# rdm695
# 11165820
# January 15th 2019 
# CMPT 332 Makaroff
# Makefile
########################################
# Fancy variable debugging tool
print-%  : ; @echo $* = $($*)
########################################
# OS and architecture macros
CURR_OS := $(shell uname -s)
ARCH := $(shell uname -m)
MAC_OS="Darwin"
LINUX_OS="Linux"
########################################
TARGET = listener sender 
########################################
# Directories
OBJ = ./obj/
INC = ./include/
SRC = ./src/
LST_DIR = ./listener_dir/
SND_DIR = ./sender_dir/
$(shell mkdir -p $(OBJ))
$(shell mkdir -p $(LST_DIR))
$(shell mkdir -p $(SND_DIR))
########################################
# Compiler and linker options
CC = gcc
AR_OPTIONS = cr
C_FLAGS = -Wall -pedantic -g
INC_FLAGS = -I$(INC)
########################################

# Filename macroes
# server 
LST_OBJ = $(OBJ)listener.o 
SND_OBJ = $(OBJ)sender.o 
CMN_H = $(INC)common.h
CMN_OBJ = $(OBJ)common.o
# all
ALL_OBJ = $(SND_OBJ) $(LST_OBJ) $(CMN_OBJ)
ALL_H = $(CMN_H)
EXEC = $(LST_DIR)listener $(SND_DIR)sender 
########################################
# Recipes
.PHONY: listener sender all clean

all: $(TARGET)

# SERVER 
listener : $(LST_OBJ) $(CMN_OBJ)
	$(CC) $^ -o $(LST_DIR)$@ 
# CLIENT
sender : $(SND_OBJ) $(CMN_OBJ)
	$(CC) $^ -o $(SND_DIR)$@ 

# SERVER OBJ FILES
$(LST_OBJ) : $(SRC)listener.c $(CMN_H)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# CLIENT OBJ FILES
$(SND_OBJ) : $(SRC)sender.c $(CMN_H)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# COMMON OBJ FILES
$(CMN_OBJ) : $(SRC)common.c $(CMN_H)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
clean:
	rm -f $(ALL_OBJ) 
	rmdir $(OBJ)
	rm -f $(EXEC) test


