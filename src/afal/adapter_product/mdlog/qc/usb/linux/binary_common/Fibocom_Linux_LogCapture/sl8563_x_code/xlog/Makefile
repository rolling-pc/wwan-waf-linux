########################################################################
# SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
# SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
# 
# Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
# Licensed under the Unisoc General Software License, version 1.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
# Software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OF ANY KIND, either express or implied.
# See the Unisoc General Software License, version 1.0 for more details.
########################################################################

####################################
#  Makefile - install              #
####################################

#CC = $(ARM_GCC)gcc
#CXX = $(ARM_GCC)g++
#AR = $(ARM_GCC)ar
#LD = $(ARM_GCC)g++

INC = 
CFLAGS = -Wall -fexceptions -std=c++11
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = -pthread
SOURDIR = sour/

INC_RELEASE = $(INC)
CFLAGS_RELEASE = $(CFLAGS) -O2
RESINC_RELEASE = $(RESINC)
RCFLAGS_RELEASE = $(RCFLAGS)
LIBDIR_RELEASE = $(LIBDIR)
LIB_RELEASE = $(LIB)
LDFLAGS_RELEASE = $(LDFLAGS) -s
OBJDIR_RELEASE = obj
OUTDIR = lib/
DEP_RELEASE = 
OUT_RELEASE = $(OUTDIR)libxlog8563.a

OBJ_RELEASE = $(OBJDIR_RELEASE)/SmpPackage.o $(OBJDIR_RELEASE)/SpLog.o $(OBJDIR_RELEASE)/TTYComm2.o $(OBJDIR_RELEASE)/UserInputMonitor.o $(OBJDIR_RELEASE)/SmpChannel.o $(OBJDIR_RELEASE)/confile.o $(OBJDIR_RELEASE)/crc.o $(OBJDIR_RELEASE)/ChannelDataHandler.o $(OBJDIR_RELEASE)/DiagChannel.o $(OBJDIR_RELEASE)/DiagPackage.o $(OBJDIR_RELEASE)/BaseChan.o $(OBJDIR_RELEASE)/MemoryMgr.o $(OBJDIR_RELEASE)/ProtocalChannelFactory.o $(OBJDIR_RELEASE)/timercontrl.o $(OBJDIR_RELEASE)/NETComm2.o $(OBJDIR_RELEASE)/xlog_wrapper.o

all: release

clean: clean_release

before_release: 
	test -d ./ || mkdir -p ./
	test -d $(OBJDIR_RELEASE) || mkdir -p $(OBJDIR_RELEASE)

after_release: 

release: before_release out_release after_release

out_release: before_release $(OBJ_RELEASE) $(DEP_RELEASE)
	$(AR) rc $(LIBDIR_RELEASE) -o $(OUT_RELEASE) $(OBJ_RELEASE) $(LIB_RELEASE)

$(OBJDIR_RELEASE)/SmpPackage.o: $(SOURDIR)SmpPackage.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)SmpPackage.cpp -o $(OBJDIR_RELEASE)/SmpPackage.o

$(OBJDIR_RELEASE)/SpLog.o: $(SOURDIR)SpLog.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)SpLog.cpp -o $(OBJDIR_RELEASE)/SpLog.o

$(OBJDIR_RELEASE)/TTYComm2.o: $(SOURDIR)TTYComm2.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)TTYComm2.cpp -o $(OBJDIR_RELEASE)/TTYComm2.o

$(OBJDIR_RELEASE)/UserInputMonitor.o: $(SOURDIR)UserInputMonitor.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)UserInputMonitor.cpp -o $(OBJDIR_RELEASE)/UserInputMonitor.o

$(OBJDIR_RELEASE)/SmpChannel.o: $(SOURDIR)SmpChannel.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)SmpChannel.cpp -o $(OBJDIR_RELEASE)/SmpChannel.o

$(OBJDIR_RELEASE)/confile.o: $(SOURDIR)confile.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)confile.c -o $(OBJDIR_RELEASE)/confile.o

$(OBJDIR_RELEASE)/crc.o: $(SOURDIR)crc.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)crc.c -o $(OBJDIR_RELEASE)/crc.o

#$(OBJDIR_RELEASE)/main.o: $(SOURDIR)main.cpp
#	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -std=c++11 -w -c $(SOURDIR)main.cpp -o $(OBJDIR_RELEASE)/main.o

$(OBJDIR_RELEASE)/ChannelDataHandler.o: $(SOURDIR)ChannelDataHandler.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -std=c++11 -w -c $(SOURDIR)ChannelDataHandler.cpp -o $(OBJDIR_RELEASE)/ChannelDataHandler.o

$(OBJDIR_RELEASE)/DiagChannel.o: $(SOURDIR)DiagChannel.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)DiagChannel.cpp -o $(OBJDIR_RELEASE)/DiagChannel.o

$(OBJDIR_RELEASE)/DiagPackage.o: $(SOURDIR)DiagPackage.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)DiagPackage.cpp -o $(OBJDIR_RELEASE)/DiagPackage.o

$(OBJDIR_RELEASE)/BaseChan.o: $(SOURDIR)BaseChan.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)BaseChan.cpp -o $(OBJDIR_RELEASE)/BaseChan.o

$(OBJDIR_RELEASE)/MemoryMgr.o: $(SOURDIR)MemoryMgr.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)MemoryMgr.cpp -o $(OBJDIR_RELEASE)/MemoryMgr.o

$(OBJDIR_RELEASE)/ProtocalChannelFactory.o: $(SOURDIR)ProtocalChannelFactory.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -w -c $(SOURDIR)ProtocalChannelFactory.cpp -o $(OBJDIR_RELEASE)/ProtocalChannelFactory.o

$(OBJDIR_RELEASE)/timercontrl.o: $(SOURDIR)timercontrl.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -std=c++11 -w -c $(SOURDIR)timercontrl.cpp -o $(OBJDIR_RELEASE)/timercontrl.o

$(OBJDIR_RELEASE)/NETComm2.o: $(SOURDIR)NETComm2.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -std=c++11 -w -c $(SOURDIR)NETComm2.cpp -o $(OBJDIR_RELEASE)/NETComm2.o

$(OBJDIR_RELEASE)/xlog_wrapper.o: $(SOURDIR)xlog_wrapper.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -std=c++11 -w -c $(SOURDIR)xlog_wrapper.cpp -o $(OBJDIR_RELEASE)/xlog_wrapper.o
clean_release: 
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)	
	rm -rf $(OBJDIR_RELEASE)

.PHONY: before_debug after_debug clean_debug before_release after_release clean_release

