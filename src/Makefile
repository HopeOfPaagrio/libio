VERSION		= 0.1
CFLAGS		+= -g -Wall -Wextra -Wmissing-declarations
CPPFLAGS	+= -I.. -MMD -MP -DVERSION=\"$(VERSION)\"
SRCS		= event.c loop.c select.c endpoint.c endpoint_socket.c \
		  queue.c queue_socket.c
OBJDIR		:= obj-$(shell uname -s)-$(shell uname -r)
OBJS		:= $(SRCS:%.c=$(OBJDIR)/%.o)

.PHONY: all
all: $(OBJDIR) libio.a
libio.a: $(OBJS)
	ar rs $@ $^

$(OBJDIR)/%.o: %.c Makefile
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

-include $(SRCS:%.c=$(OBJDIR)/%.d)

.PHONY: clean
clean:
	rm -f *~ core *.core libio.a
	rm -rf $(OBJDIR)
