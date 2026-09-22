# If use the arm gcc, please set the arm gcc path.
# ARM_GCC = ~/ARM_Linux_GCC/bin/arm-none-linux-gnueabi-

CC = $(ARM_GCC)gcc

CFLAGS = -Wall -g
LD_LIBRARY =

INCLUDE =
SRC_FILES = $(wildcard ./*.c)

BIN_TARGET = config_to_h_tool

OBJECTS = $(patsubst %.c,%.o, $(SRC_FILES))

$(BIN_TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@ $(LD_LIBRARY)
	@rm -vf ./*.o

$(OBJECTS) : %.o : %.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@  $(LD_LIBRARY)

.PHONY: clean
clean:
	@rm -rvf $(BIN_TARGET) ./*.o

