SRCS := src/main.cpp src/sort.cpp src/lexer.cpp src/parser.cpp src/ast.cpp src/typing.cpp src/compiler.cpp src/ranking.cpp src/interpreter.cpp
OBJS := $(SRCS:%.cpp=%.o)

.PHONY: all

all: mold

%.o: %.cpp
	clang++ -std=c++23 -Wall -Wextra -g3 -fsanitize=address,undefined -c -o $@ $<

mold: $(OBJS)
	clang++ -std=c++23 -Wall -Wextra -g3 -fsanitize=address,undefined -o $@ $^
