# makefile
#
SRC = test.cpp
BIN = test.out
LIB = #link your libs here
OPT = -pipe -std=c++2a -O3 -march=native -pthread -fopenmp -fopenacc
ERR = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wno-unused
WRN = -Wnull-dereference -Wsign-conversion -Wconversion -Wshadow -Wcast-align -Wuseless-cast
FLG = $(OPT) $(LIB) $(ERR) $(WRN)
#
.PHONY: all clear conf test
#
all:
	@clear
	@clear
	@time -f "[ %es ]" g++ $(SRC) -o $(BIN) $(FLG)
#
clear:
	@sudo rm -f *~ *.o
#
conf:
	@sudo chown `whoami`:`whoami` $(BIN)
	@sudo chmod u=rwX,go=rX $(BIN)
#
test: all clear conf
	@time -f "[ %es ]" ./$(BIN)
#
