BINS = info decap fixup newdecap

OBJS = $(addsuffix .o,$(BINS))

CFLAGS += -Werror -g
LDFLAGS += -g

.PHONY: all clean

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

$(OBJS): decap.h
newdecap.o: list.h lead.h header.h rpm.h

newdecap: lead.o header.o rpm.o
