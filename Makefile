BINS = info decap fixup

OBJS = $(addsuffix .o,$(BINS))

CFLAGS += -Werror -g
LDFLAGS += -g

.PHONY: all clean

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

$(OBJS): decap.h
