export CC = gcc
export CFLAGS = -O2 -std=gnu99 -Wall -Wwrite-strings -pedantic
CFLAGS += -DTUMBLEWEED
CFLAGS += -DMAGIC
POST_LINKING = -lpthread
POST_LINKING += -lmagic
SUBDIRS =
OBJS=conf.o file.o hash.o http.o list.o log.o \
	malloc.o mime.o node_web.o opts.o \
	send_web.o str.o tcp.o thrd.o tumbleweed.o unix.o

.PHONY: all clean install $(SUBDIRS)

all: $(SUBDIRS) tumbleweed

%.o : ../src/%.c ../src/%.h
	$(CC) $(CFLAGS) -c $<

tumbleweed: $(OBJS)
	$(CC) $(OBJS) -o tumbleweed $(POST_LINKING)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir; \
	done
	rm -f *.o tumbleweed

install:
	strip tumbleweed
