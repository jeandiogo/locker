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
OPT =  -std=c++23 -O3 -march=native -flto=auto -pipe -pthread #-fimplicit-constexpr -fmodule-implicit-inline
WRN =  -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors
WRN += -Wnull-dereference -Wshadow -Wconversion -Wsign-conversion -Warith-conversion -Wold-style-cast
WRN += -Wcast-align=strict -Wcast-qual -Wredundant-decls -Wmismatched-tags -Wsuggest-override #-Wsuggest-final-types
WNO =  -Wno-unused -Wno-vla
#
OUT = $(BIN)~
NMS = $(basename $(SRC))
OBJ = $(addsuffix .o,$(NMS))
DEP = $(addsuffix .d,$(NMS))
TMP = $(addsuffix ~,$(NMS)) $(addsuffix .gch,$(NMS)) $(addsuffix .gcda,$(NMS)) $(addsuffix .gcno,$(NMS)) $(addsuffix .i,$(NMS)) $(addsuffix .s,$(NMS))
FLG = $(OPT) $(LIB) $(WRN) $(WNO)
#
.PHONY: all clean static test valgrind
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
static: clean
	@g++ -o $(BIN) $(SRC) $(FLG) -fwhole-program -static -static-libgcc -static-libstdc++
	@readelf -d $(BIN)
	@ldd $(BIN) || true
	@nm -D $(BIN)
#
test: all
	@time -f "[ %es ]" ./$(BIN)
#
valgrind: all
	@valgrind -v --leak-check=full --show-leak-kinds=all --expensive-definedness-checks=yes --track-origins=yes --track-fds=yes --trace-children=yes ./$(BIN)
#
-include $(DEP)
#
