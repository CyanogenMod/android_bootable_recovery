SRC_LOKI := loki_flash.c loki_patch.c loki_find.c main.c
OBJ_LOKI = $(SRC_LOKI:.c=.o)
MODULE_LOKI := loki_tool

CC := arm-linux-androideabi-gcc
CC_STRIP := arm-linux-androideabi-strip

CFLAGS += -g -static -Wall
#$(LDFLAGS) +=


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(MODULE_LOKI): $(OBJ_LOKI)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

strip:
	$(CC_STRIP) --strip-unneeded $(MODULE_LOKI)
	$(CC_STRIP) --strip-debug $(MODULE_LOKI)

clean:
	rm -f *.o
	rm -f loki_tool
