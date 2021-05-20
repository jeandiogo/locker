########################################################################################################################
# 
# Locker (C++ Library)
# Copyright (C) 2021 Jean Diogo (Jango) <jeandiogo@gmail.com>
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
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)
DEP = $(OBJ:.o=.d)
OPT = -std=c++20 -O3 -march=native -pipe -flto -pthread -fopenmp -fopenacc -fPIC
WRN = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wnull-dereference -Wshadow -Wconversion -Wsign-conversion -Warith-conversion
XTR = -Wcast-align=strict -Wpacked -Wcast-qual -Wredundant-decls -Wundef -Wuseless-cast -Wsuggest-override -Wsuggest-final-methods -Wsuggest-final-types
WNO = -Wno-unused -Wno-vla
FLG = $(OPT) $(LIB) $(WRN) $(XTR) $(WNO)
#
.PHONY: all test clear profile valgrind permissions zip
#
all: $(BIN)
#
%.o: %.cpp
	@clear
	@clear
	@g++ -o $@ $< -MMD -MP -c $(FLG)
#
$(BIN): $(OBJ)
	@g++ -o $@ $^ $(FLG) -fuse-linker-plugin
#
test: all
	@time -f "[ %es ]" ./$(BIN)
#
clear:
	@sudo rm -rf *~ *.o *.d *.gch *.gcda *.gcno $(BIN)
#
profile: clear
	@g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program -fprofile-generate
	@./$(BIN)
	@g++ $(SRC) -o $(BIN) $(FLG) -fwhole-program -fprofile-use -fprofile-correction
#
valgrind: all
	@valgrind -v --leak-check=full --show-leak-kinds=all --expensive-definedness-checks=yes --track-origins=yes --track-fds=yes --trace-children=yes ./$(BIN)
#
permissions:
	@sudo chown -R `whoami`:`whoami` .
	@sudo chmod -R u=rwX,go=rX .
#
zip: clear permissions
	@sudo zip -qr $(BIN).zip .
	@google-chrome --new-window https://drive.google.com/drive/my-drive >/dev/null 2>&1 &
	@nemo `pwd` &
#
-include $(DEP)
#
