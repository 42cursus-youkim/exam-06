TARGET := mini

SRC := mini_serv.c
OBJ := $(SRC:.c=.o)

CFLAGS := -Wall -Wextra -Werror -Wpedantic -std=c99 -g

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET) 9999
