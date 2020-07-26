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
SRC = test.cpp
BIN = test.out
LIB = #link your libs here
OPT = -pipe -std=c++20 -O3 -march=native -pthread -fopenmp -fopenacc
ERR = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wno-unused -Wno-vla
WRN = -Wnull-dereference -Wsign-conversion -Wconversion -Wshadow -Wcast-align -Wuseless-cast
FLG = $(OPT) $(LIB) $(ERR) $(WRN)
#
.PHONY: all clear test
#
all:
	@clear
	@clear
	@time -f "[ %es ]" g++ $(SRC) -o $(BIN) $(FLG)
#
clear:
	@rm -rf *~ *.o $(BIN)
#
test: clear all
	@time -f "[ %es ]" ./$(BIN)
#
