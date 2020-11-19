########################################################################################################################
# 
# Locker (C++ Library)
# Copyright (C) 2020 Jean Diogo (Jango) <jeandiogo@gmail.com>
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
OPT = -std=c++20 -O3 -march=native -pthread -fopenmp -fopenacc -pipe -flto -fPIC
WRN = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wnull-dereference -Wshadow -Wconversion -Wsign-conversion -Warith-conversion
XTR = -Wcast-align=strict -Wpacked -Wundef -Wcast-qual -Wredundant-decls -Wuseless-cast -Wsuggest-override -Wsuggest-final-methods -Wsuggest-final-types
WNO = -Wno-vla -Wno-unused
FLG = $(OPT) $(LIB) $(WRN) $(XTR) $(WNO)
#
.PHONY: all test clear profile valgrind permissions zip
#
all: $(BIN)
#
$(BIN): $(OBJ)
	@g++ -o $@ $^ $(FLG) -fuse-linker-plugin
#
%.o: %.cpp
	@clear
	@clear
	@g++ -o $@ $< -MMD -MP -c $(FLG)
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
	@sudo zip -q -r $(BIN).zip .
#
-include $(DEP)
#
