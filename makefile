CC     = gcc
CFLAGS = -Wall -Wextra -g

SRC = src/main.c \
      src/common/auth.c \
      src/common/user_manager.c \
      src/common/message_handler.c

all:
	$(CC) $(CFLAGS) $(SRC) -o chat

clean:
	rm -f chat