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
# This is a generic makefile.
# List your library link flags in the variable LIB, and your source code filenames in the variable SRC.
# To include all the source files in the current directory (and its subdirectories), uncomment the wildcard flags.
# 
########################################################################################################################
#
LIB = #link your libs here
BIN = test.out
SRC = test.cpp #$(wildcard *.cpp) $(wildcard **/*.cpp)
OBJ = $(SRC:.cpp=.o)
DPS = $(OBJ:.o=.d)
OPT = -pipe -std=c++20 -O3 -march=native -pthread -fopenmp -fopenacc
ERR = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors
WRN = -Wnull-dereference -Wsign-conversion -Wconversion -Wshadow -Wcast-align -Wuseless-cast
WNO = -Wno-unused -Wno-vla
DBG = -g -D_GLIBCXX_DEBUG -ftrapv -fsanitize=undefined -fsanitize=address
FLG = $(OPT) $(LIB) $(ERR) $(WRN) $(WNO)
#
-include $(DPS)
.PHONY: all clear auth debug whole test zip
#
all: $(OBJ)
	@clear
	@clear
	@g++ $^ -o $(BIN) $(FLG) -flto -fuse-linker-plugin
#
%.o: %.cpp
	@clear
	@clear
	@g++ -MMD $^ -c -o $@ $(FLG) -flto -fuse-linker-plugin
#
debug:
	@clear
	@clear
	@time -f "[ %es ]" g++ $(SRC) -o $(BIN) $(FLG) $(DBG)
#
whole:
	@clear
	@clear
	@time -f "[ %es ]" g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program
#
clear:
	@sudo rm -rf *~ *.o *.d $(BIN)
#
auth:
	@sudo chown `whoami`:`whoami` $(BIN)
	@sudo chmod u=rwX,go=rX $(BIN)
#
test: clear whole auth
	@time -f "[ %es ]" ./$(BIN)
#
zip:
	@sudo zip -q -r $(BIN).zip .
#
