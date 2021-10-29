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
OPT = -std=c++20 -O3 -march=native -pipe -flto -pthread -fopenmp -fopenacc -fPIC
WRN = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wnull-dereference -Wshadow -Wconversion -Wsign-conversion -Warith-conversion
XTR = -Wcast-align=strict -Wpacked -Wcast-qual -Wredundant-decls -Wundef -Wuseless-cast -Wsuggest-override -Wsuggest-final-methods -Wsuggest-final-types
WNO = -Wno-unused -Wno-vla
#
OUT = $(BIN)~
NMS = $(basename $(SRC))
OBJ = $(addsuffix .o,$(NMS))
DEP = $(addsuffix .d,$(NMS))
TMP = $(addsuffix ~,$(NMS)) $(addsuffix .gch,$(NMS)) $(addsuffix .gcda,$(NMS)) $(addsuffix .gcno,$(NMS))
FLG = $(OPT) $(LIB) $(WRN) $(XTR) $(WNO)
WHL = g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program
#
.PHONY: all clear permissions profile safe static test unsafe upload valgrind zip
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
clear:
	@sudo rm -rf $(OBJ) $(DEP) $(TMP)
#
permissions:
	@sudo chown -R `whoami`:`whoami` .
	@sudo chmod -R u=rwX,go=rX .
#
profile: clear
	@$(WHL) -fprofile-generate
	@./$(BIN)
	@$(WHL) -fprofile-use -fprofile-correction
#
safe:
	@$(WHL) -fstack-protector-all -fstack-clash-protection -fsplit-stack -fsanitize=undefined
#
static:
	@$(WHL) -static -static-libgcc -static-libstdc++
	@readelf -d $(BIN)
	@ldd $(BIN) || true
	@nm -D $(BIN)
#
test: all
	@time -f "[ %es ]" ./$(BIN)
#
unsafe:
	@g++ $(SRC) -o $(BIN) $(OPT) -fwhole-program
#
upload: zip
	@nohup google-chrome --new-window https://drive.google.com/drive/my-drive </dev/null >/dev/null 2>&1 &
	@nohup nemo `pwd`  </dev/null >/dev/null 2>&1 &
#
valgrind: all
	@valgrind -v --leak-check=full --show-leak-kinds=all --expensive-definedness-checks=yes --track-origins=yes --track-fds=yes --trace-children=yes ./$(BIN)
#
zip: clear
	@sudo zip -qr $(BIN).zip .
#
-include $(DEP)
#
