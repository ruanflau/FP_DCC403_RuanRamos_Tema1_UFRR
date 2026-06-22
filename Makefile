CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lssl -lcrypto -lz
LDFLAGS = -lssl -lcrypto -lz -lpthread

SRC_DIR = src
OBJ_DIR = obj

COMMON_SRCS = $(SRC_DIR)/common.c $(SRC_DIR)/compress.c $(SRC_DIR)/tls.c $(SRC_DIR)/logger.c
COMMON_OBJS = $(COMMON_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
COMMON_SRCS = $(SRC_DIR)/common.c $(SRC_DIR)/compress.c $(SRC_DIR)/tls.c $(SRC_DIR)/logger.c $(SRC_DIR)/net_io.c

all: industrial_agent control_proxy

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

industrial_agent: $(OBJ_DIR)/agent.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

control_proxy: $(OBJ_DIR)/proxy.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJ_DIR) industrial_agent control_proxy

valgrind: industrial_agent
	valgrind --leak-check=full --show-leak-kinds=all ./industrial_agent --port=9095

.PHONY: all clean valgrind