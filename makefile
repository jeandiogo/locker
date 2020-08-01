########################################################################################################################
# 
# Locker (C++ Library)
# Copyright (c) 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>
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
LIB = #link your libs here
BIN = test.out
SRC = test.cpp
#SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)
DPS = $(OBJ:.o=.d)
OPT = -std=c++20 -O3 -march=native -pipe -pthread -fopenmp -fopenacc
ERR = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors
WRN = -Wnull-dereference -Wsign-conversion -Wconversion -Wshadow -Wcast-align -Wuseless-cast
WNO = -Wno-unused -Wno-vla
FLG = $(OPT) $(LIB) $(ERR) $(WRN) $(WNO)
#
-include $(DPS)
.PHONY: all clear test prof $(BIN)
#
all: $(BIN)
#
$(BIN): $(OBJ)
	@g++ $^ -o $@ $(FLG) -fuse-linker-plugin
#
%.o: %.cpp
	@clear
	@clear
	@g++ $^ -MMD -c $(FLG) -flto
#
clear:
	@sudo rm -rf *~ *.o *.d *.gch *.gcda $(BIN)
#
test: clear all
	@time -f "[ %es ]" ./$(BIN)
#
prof: clear
	@g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program -fprofile-generate
	@./$(BIN)
	@g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program -fprofile-use
#
