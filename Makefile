BINS = info

OBJS = $(addsuffix .o,$(BINS))

CFLAGS += -Werror

.PHONY: all clean

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

info.o: decap.h
