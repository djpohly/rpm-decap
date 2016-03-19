BINS = info decap

OBJS = $(addsuffix .o,$(BINS))

CFLAGS += -Werror

.PHONY: all clean

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

info.o decap.o: decap.h
