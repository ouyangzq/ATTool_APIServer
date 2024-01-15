ifndef CC
CC = gcc
endif
ifndef LD
LD = ld
endif

SOURCES =  main.c openDev.c json.c
OBJS := $(SOURCES:.c=.o)

ifndef CFLAGS
CFLAGS := -O2
endif
CPPFLAGS := -I ATTool_APIServer

ATTool_APIServer: $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf ATTool_APIServer  $(OBJS)

compile: ATTool_APIServer

install: compile
	mkdir -p $(DESTDIR)/usr/sbin
	cp ATTool_APIServer $(DESTDIR)/usr/sbin/ATTool_APIServer
