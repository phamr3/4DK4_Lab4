# ARQ simulator (Stop-and-Wait / Go-Back-N / Selective-Repeat)
# Build: make  (or mingw32-make on Windows)

CC = gcc
CFLAGS = -O2 -Wall

OBJS = main.o sender.o receiver.o channel.o packet_arrival.o output.o cleanup.o simlib.o

arq_sim: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

%.o: %.c *.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o arq_sim arq_sim.exe
