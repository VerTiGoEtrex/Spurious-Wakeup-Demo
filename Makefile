CCFLAGS := -Wall -Werror -O3
LDFLAGS := -lpthread

BIN = spurious

all: $(BIN)

spurious: spurious.o
	$(CC) $(CCFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CCFLAGS) -c $< $(LDFLAGS)

clean:
	rm -f *~ *.o $(BIN)
