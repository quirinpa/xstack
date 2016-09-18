# EXE:=xstack
CFLAGS:=$(CFLAGS) -ggdb
EXE:=xstkd xstkc
# EXE:=xstkd
.PHONY: all clean

# all: $(EXE)
all: $(EXE)

$(EXE): %: %.c
	$(CC) -o $@ $^ $(CFLAGS) -lX11

clean:
	$(RM) $(EXE)
# $(EXE): % : %.o
# 	$(CC) -o $@ $^ $(CFLAGS) `pkg-config --libs x11`

# %.o: %.c
# 	$(CC) -c $< $(CFLAGS) `pkg-config --cflags x11`



