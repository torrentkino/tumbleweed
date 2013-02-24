export CC = gcc
export CFLAGS = -Wall -Wwrite-strings -pedantic
CFLAGS += -DTUMBLEWEED
CFLAGS += -DMAGIC
export OPTIMISATION = -O2
#SEC_LINKING  = -static
PRE_LINKING  = -L/opt/lib/ -Wl,--as-needed
POST_LINKING = -lpthread
POST_LINKING += -lmagic
SUBDIRS =

.PHONY: all clean install $(SUBDIRS)

all: $(SUBDIRS) tumbleweed

conf.o: ../src/conf.c ../src/conf.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/conf.c

unix.o: ../src/unix.c ../src/unix.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/unix.c

log.o: ../src/log.c ../src/log.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/log.c

file.o: ../src/file.c ../src/file.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/file.c

hash.o: ../src/hash.c ../src/hash.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/hash.c

node_web.o: ../src/node_web.c ../src/node_web.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/node_web.c

http.o: ../src/http.c ../src/http.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/http.c

send_web.o: ../src/send_web.c ../src/send_web.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/send_web.c

list.o: ../src/list.c ../src/list.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/list.c

malloc.o: ../src/malloc.c ../src/malloc.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/malloc.c

opts.o: ../src/opts.c ../src/opts.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/opts.c

str.o: ../src/str.c ../src/str.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/str.c

tcp.o: ../src/tcp.c ../src/tcp.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/tcp.c

thrd.o: ../src/thrd.c ../src/thrd.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/thrd.c

main.o: ../src/main.c ../src/main.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/main.c

mime.o: ../src/mime.c ../src/mime.h
	$(CC) $(CFLAGS) $(OPTIMISATION) -c ../src/mime.c

tumbleweed: \
	main.o mime.o conf.o unix.o log.o file.o node_web.o hash.o http.o send_web.o list.o malloc.o opts.o str.o thrd.o tcp.o
	$(CC) $(SEC_LINKING) $(PRE_LINKING) \
	main.o mime.o conf.o unix.o log.o file.o node_web.o hash.o http.o send_web.o list.o malloc.o opts.o str.o thrd.o tcp.o \
	-o tumbleweed $(POST_LINKING)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir; \
	done
	rm -f *.o tumbleweed

install:
	strip tumbleweed
