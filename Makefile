CFLAGS += -Wall -Wextra -Wno-unused-parameter -O3

countty: countty.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f countty
