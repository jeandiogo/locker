# makefile
#
BIN = test.out
SRC = test.cpp #$(wildcard *.cpp)
LIB = -lcurl -lssl -lcrypto -lfcgi
OPT = -std=c++2a -O3 -march=native -pthread -fopenmp -fopenacc -pipe #-fwhole-program
ERR = -Wall -Wextra -pedantic -Werror -pedantic-errors -Wfatal-errors -Wno-unused
WRN = -Wnull-dereference -Wsign-conversion -Wconversion -Wshadow -Wcast-align -Wuseless-cast
FLG = $(OPT) $(LIB) $(ERR) $(WRN)
#
.PHONY: all
#
all:
	@clear
	@clear
	@time -f "[ %es ]" g++ $(SRC) -o $(BIN) $(FLG)
	@sudo rm -f *~ *.o
	@sudo chown `whoami`:`whoami` $(BIN)
	@sudo chmod u=rwX,go=rX $(BIN)
	@time -f "[ %es ]" ./$(BIN)
#
