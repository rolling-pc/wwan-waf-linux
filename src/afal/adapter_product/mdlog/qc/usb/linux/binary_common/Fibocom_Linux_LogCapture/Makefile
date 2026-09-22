# If use the arm gcc, please set the arm gcc path.
# ARM_GCC = ~/ARM_Linux_GCC/bin/arm-none-linux-gnueabi-

CC = $(ARM_GCC)gcc
CXX = $(ARM_GCC)g++
AR = $(ARM_GCC)ar
CFLAGS = -Wall

#If the customer only wants to compile a tool for a specific platform,
#set the macro for the corresponding platform to yes, and the macro for other platforms to no,
#the tool for a specific platform will be compiled separately.
config_unisoc_x = no
config_unisoc = no
config_qcom = yes
config_zte = no
config_eigencomm = no
config_sl8563_x = no
config_qcom_localdump = yes

ifeq ($(config_unisoc_x), yes)
CFLAGS += -DCONFIG_UNISOC_X
endif

ifeq ($(config_sl8563_x), yes)
CFLAGS += -DCONFIG_SL8563_X
endif

ifeq ($(config_unisoc), yes)
CFLAGS += -DCONFIG_UNISOC
endif

ifeq ($(config_qcom), yes)
CFLAGS += -DCONFIG_QCOM
endif

ifeq ($(config_zte), yes)
CFLAGS += -DCONFIG_ZTE
endif

ifeq ($(config_eigencomm), yes)
CFLAGS += -DCONFIG_EIGENCOMM
endif

ifeq ($(config_qcom_localdump), yes)
CFLAGS += -DCONFIG_QCOM_DUMP
endif


ifeq ($(config_unisoc_x), yes)
LD_LIB = -lpthread -lxlog
else
LD_LIB = -lpthread
endif

ifeq ($(config_qcom_localdump), yes)
LD_LIB += -lz
endif

ifeq ($(config_sl8563_x), yes)
LD_LIB += -lxlog8563
endif

INCLUDE = -I./misc_code -I./zte_code -I./qcom_code -I./unisoc_code -I./zte_code/config -I./qcom_code/config
INCLUDE += -I./unisoc_x_code/xlog/inc -I./unisoc_x_code/xlog/lib/inc -I./sl8563_x_code/xlog/sour
LIB_PATH = -L./unisoc_x_code/xlog/lib -L./sl8563_x_code/xlog/lib
SRC_FILES = $(wildcard ./misc_code/*.c ./*.c)

ifeq ($(config_unisoc_x), yes)
SRC_FILES += $(wildcard ./unisoc_x_code/*.c)
endif

ifeq ($(config_sl8563_x), yes)
SRC_FILES += $(wildcard ./sl8563_x_code/*.c)
endif

ifeq ($(config_unisoc), yes)
SRC_FILES += $(wildcard ./unisoc_code/*.c)
endif

ifeq ($(config_qcom), yes)
SRC_FILES += $(wildcard ./qcom_code/*.c)
endif

ifeq ($(config_zte), yes)
SRC_FILES += $(wildcard ./zte_code/*.c)
endif

ifeq ($(config_eigencomm), yes)
SRC_FILES += $(wildcard ./eigencomm_code/*.c)
endif

SUBDIRS := ./unisoc_x_code/xlog
SUBDIRS2 := ./sl8563_x_code/xlog

BIN_TARGET = logtool

OBJECTS = $(patsubst %.c,%.o,$(SRC_FILES))

$(BIN_TARGET) : $(OBJECTS)
ifeq ($(config_unisoc_x), yes)
	make -C $(SUBDIRS) CC=$(CC) CXX=$(CXX) AR=$(AR)
endif
ifeq ($(config_sl8563_x), yes)
	make -C $(SUBDIRS2) CC=$(CC) CXX=$(CXX) AR=$(AR)
endif
	$(CXX) $^ -o $@ $(LD_LIB) $(LIB_PATH)
	@rm -f ./misc_code/*.o ./qcom_code/*.o ./zte_code/*.o ./unisoc_code/*.o ./eigencomm_code/*.o ./*.o
	@rm -f ./unisoc_x_code/*.o ./sl8563_x_code/*.o

$(OBJECTS) : %.o : %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

.PHONY: clean

clean:
	@rm -rvf $(BIN_TARGET) ./misc_code/*.o ./qcom_code/*.o ./zte_code/*.o ./eigencomm_code/*.o ./unisoc_code/*.o ./*.o ./unisoc_x_code/xlog/lib/*.a ./unisoc_x_code/xlog/obj ./sl8563_x_code/xlog/lib/*.a ./sl8563_x_code/xlog/obj
	@rm -rvf $(OBJECTS)
