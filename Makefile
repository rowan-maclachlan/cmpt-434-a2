########################################
# Rowan MacLachlan
# rdm695
# 11165820
# February 11th 2019 
# CMPT 434 Eager 
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
TARGET = listener sender test
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
C_FLAGS = -g -Wall -pedantic -g
INC_FLAGS = -I$(INC)
########################################

# Filename macroes
# server 
LST_OBJ = $(OBJ)listener.o 
SND_OBJ = $(OBJ)sender.o 
CMN_H = $(INC)common.h
CMN_OBJ = $(OBJ)common.o
MSG_Q_H = $(INC)msg_queue.h
MSG_Q_OBJ = $(OBJ)msg_queue.o
TEST_OBJ = $(OBJ)test.o
# all
ALL_H = $(CMN_H) $(MSG_Q_H)
ALL_OBJ = $(CMN_OBJ) $(MSG_Q_OBJ) $(TEST_OBJ) $(SND_OBJ) $(LST_OBJ)
EXEC = $(LST_DIR)listener $(SND_DIR)sender 
########################################
# Recipes
.PHONY: listener sender all clean

all: $(TARGET)

# SERVER 
listener : $(LST_OBJ) $(CMN_OBJ) $(MSG_Q_OBJ)
	$(CC) $^ -o $(LST_DIR)$@ 
# CLIENT
sender : $(SND_OBJ) $(CMN_OBJ) $(MSG_Q_OBJ)
	$(CC) $^ -o $(SND_DIR)$@ 
# TEST
test : $(TEST_OBJ) $(CMN_OBJ) $(MSG_Q_OBJ)
	$(CC) $^ -o $@

# SERVER OBJ FILES
$(LST_OBJ) : $(SRC)listener.c $(CMN_H) $(MSG_Q_OBJ)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# CLIENT OBJ FILES
$(SND_OBJ) : $(SRC)sender.c $(CMN_H) $(MSG_Q_OBJ)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# TEST OBJ FILES
$(TEST_OBJ) : $(SRC)test.c $(CMN_H) $(MSG_Q_OBJ)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# COMMON OBJ FILES
$(CMN_OBJ) : $(SRC)common.c $(CMN_H)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
# MSG QUEUE OBJ FILES
$(MSG_Q_OBJ) : $(SRC)msg_queue.c $(MSG_Q_H)
	$(CC) $(INC_FLAGS) $(C_FLAGS) -c $< -o $@
clean:
	rm -f $(ALL_OBJ) 
	rmdir $(OBJ)
	rm -f $(EXEC) test


