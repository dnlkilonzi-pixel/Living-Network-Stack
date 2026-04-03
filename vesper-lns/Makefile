# Living Network Stack – Makefile
# Build:         make
# Build library: make lib
# Build examples:make examples
# Run demo:      make run
# Clean:         make clean

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -g \
           -Icore -Iintent_engine -Ipvm -Imutation_engine \
           -Iidentity_layer -Iexecution_engine -Itransport \
           -Inetwork -Itrust_layer -Ireplay -Iobserver -Ibus -Isdk

TARGET  := lns_demo
LIB     := liblns.a

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
        replay/replay.c \
        observer/observer.c \
        bus/bus.c \
        demo/main.c

# Library sources: all modules except demo/ and sdk/examples/
LIB_SRCS := core/lns.c \
            intent_engine/intent.c \
            pvm/pvm.c \
            mutation_engine/mutation.c \
            identity_layer/identity.c \
            execution_engine/exec.c \
            transport/udp.c \
            transport/tcp.c \
            network/network.c \
            trust_layer/trust.c \
            replay/replay.c \
            observer/observer.c \
            bus/bus.c \
            sdk/lns_sdk.c

LIB_OBJS := $(LIB_SRCS:.c=.o)
OBJS     := $(SRCS:.c=.o)

EXAMPLE_SRCS := sdk/examples/example_basic.c \
                sdk/examples/example_replay.c
EXAMPLE_BINS := sdk/examples/example_basic \
                sdk/examples/example_replay

.PHONY: all lib examples run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Build successful: $(TARGET)"

# Static library – link against this in your own project
lib: $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^
	@echo "Library built: $(LIB)"

# SDK example programs (require lib to be built first)
examples: $(LIB) $(EXAMPLE_BINS)

sdk/examples/example_basic: sdk/examples/example_basic.c $(LIB)
	$(CC) $(CFLAGS) -Isdk -o $@ $< $(LIB)
	@echo "Built: $@"

sdk/examples/example_replay: sdk/examples/example_replay.c $(LIB)
	$(CC) $(CFLAGS) -Isdk -o $@ $< $(LIB)
	@echo "Built: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: all
	@echo ""
	@./$(TARGET)

clean:
	rm -f $(OBJS) $(LIB_OBJS) $(TARGET) $(LIB) $(EXAMPLE_BINS)
