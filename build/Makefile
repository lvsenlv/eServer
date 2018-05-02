TARGET_BASE_NAME    = eServer
CC                  = gcc
LIBS                = -static -lpthread 
CFLAGS              = -g -Wall -Wno-unused-function -Werror
SRC_DIR             = ../src
OBJ_DIR             = ./object
LIB_DIR             = ../lib
OBJS                = common.o crc.o log.o completion_code.o hex_str_to_dec.o message.o server.o main.o
DEF                 = -D__LINUX -D__64BIT -D__SERVER -D__DEBUG -D__LOG_CLEAR -D__CRC16 -D__CRC32 
INC                 = -I../include 
TARGET              = $(TARGET_BASE_NAME)

$(shell mkdir -p $(OBJ_DIR))

all : $(TARGET) 

$(TARGET) : $(OBJS)
	$(CC) $(wildcard $(OBJ_DIR)/*.o) -o $@ $(LIBS)

%.o : $(SRC_DIR)/%.c
	$(CC) -c $<  -o $(OBJ_DIR)/$@ $(DEF) $(INC) $(CFLAGS) 

tools :
	@$(MAKE) -f Makefile_eTools

clean :
	rm -rf $(TARGET_DIR) $(OBJ_DIR) $(TARGET_BASE_NAME)* 
	@$(MAKE) clean -f Makefile_eTools

