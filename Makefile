BINS = info decap fixup newdecap

OBJS = $(addsuffix .o,$(BINS))
OBJS += lead.o header.o rpm.o

CFLAGS += -Werror -g
LDFLAGS += -g

.PHONY: all clean

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

$(OBJS): decap.h
newdecap.o: list.h lead.h header.h rpm.h
rpm.o: rpm.h lead.h header.h
lead.o: lead.h
header.o: header.h list.h lead.h

newdecap: lead.o header.o rpm.o
