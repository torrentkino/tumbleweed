#SUBDIRS = torrentkino6 torrentkino4 tknss tkcli
SUBDIRS = tumbleweed

.PHONY : all clean install docs sync debian ubuntu $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

install:
	for dir in $(SUBDIRS); do \
		$(MAKE) install -C $$dir; \
	done

docs:
	./bin/docs.sh tumbleweed

sync:
	./bin/sync.sh tumbleweed

debian:
	./bin/debian.sh tumbleweed

ubuntu:
	./bin/debian.sh tumbleweed

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir; \
	done
