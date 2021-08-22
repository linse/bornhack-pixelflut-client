CFLAGS+=$(shell libpng-config --cflags) -g
LDFLAGS+=$(shell libpng-config --ldflags)
.PHONY: clean
owndisplay: owndisplay.o
	$(CC) -o $@ $^ $(LDFLAGS)
clean:
	rm -f owndisplay owndisplay.o
