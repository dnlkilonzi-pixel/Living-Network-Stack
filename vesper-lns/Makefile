# Living Network Stack – Makefile
# Build: make
# Run:   make run
# Clean: make clean

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -g \
           -Icore -Iintent_engine -Ipvm -Imutation_engine \
           -Iidentity_layer -Iexecution_engine -Itransport \
           -Inetwork -Itrust_layer

TARGET  := lns_demo

SRCS := core/lns.c \
        intent_engine/intent.c \
        pvm/pvm.c \
        mutation_engine/mutation.c \
        identity_layer/identity.c \
        execution_engine/exec.c \
        transport/udp.c \
        transport/tcp.c \
        network/network.c \
        trust_layer/trust.c \
        demo/main.c

OBJS := $(SRCS:.c=.o)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	@echo ""
	@./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
