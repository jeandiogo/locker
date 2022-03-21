########################################################################################################################
# 
# Locker (C++ Library)
# Copyright (C) 2020 Jean "Jango" Diogo <jeandiogo@gmail.com>
# 
# Licensed under the Apache License Version 2.0 (the "License").
# You may not use this file except in compliance with the License.
# You may obtain a copy of the License at <http://www.apache.org/licenses/LICENSE-2.0>.
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and limitations under the License.
# 
########################################################################################################################
# 
# makefile
# 
########################################################################################################################
#
LIB = #link libs here
BIN = test.out
DIR = .
SRC = $(wildcard $(DIR)/*.cpp)
#
OPT = -std=c++20 -O3 -march=native -pipe -flto -pthread #-fPIC -fopenmp -fopenacc
WRN = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wnull-dereference -Wshadow -Wconversion -Wsign-conversion -Warith-conversion
XTR = -Wcast-align=strict -Wpacked -Wcast-qual -Wredundant-decls #-Wundef -Wuseless-cast -Wsuggest-override -Wsuggest-final-methods -Wsuggest-final-types
WNO = -Wno-unused -Wno-vla
#
OUT = $(BIN)~
NMS = $(basename $(SRC))
OBJ = $(addsuffix .o,$(NMS))
DEP = $(addsuffix .d,$(NMS))
TMP = $(addsuffix ~,$(NMS)) $(addsuffix .gch,$(NMS)) $(addsuffix .gcda,$(NMS)) $(addsuffix .gcno,$(NMS))
FLG = $(OPT) $(LIB) $(WRN) $(XTR) $(WNO)
#
.PHONY: all clear test
#
all: $(OUT)
#
$(OUT): $(OBJ)
	@g++ -o $@ $^ $(FLG) -fuse-linker-plugin
	@mv -f $@ $(BIN)
#
%.o: %.cpp
	@clear
	@clear
	@g++ -o $@ $< -MMD -MP -c $(FLG)
#
clean:
	@rm -rf $(OBJ) $(DEP) $(TMP)
#
test: all
	@time -f "[ %es ]" ./$(BIN)
#
-include $(DEP)
#
