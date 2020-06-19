# makefile
#
BIN = test.out
SRC = $(wildcard *.cpp)
LIB = #`pkg-config --libs --cflags SDL2_gfx SDL2_image SDL2_ttf SDL2_mixer SDL2_net sdl2 sfml-all opencv` -lcurl -lssl -lcrypto -lfcgi
OPT = -std=c++2a -O3 -march=native -pthread -fopenmp -fopenacc -pipe -fwhole-program
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
	@time -f "[ %es ]" ./$(BIN)
#
zip:
	@sudo zip -q -r $(BIN).zip .
	@sudo chown `whoami`:`whoami` $(BIN).zip
	@sudo chmod -R u=rwX,go=rX $(BIN).zip
	@google-chrome --new-window https://drive.google.com/drive/my-drive >/dev/null 2>&1 &
	@nemo `pwd`  2>&1 &
#
