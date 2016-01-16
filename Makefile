
SUBDIRS = examples/Chibios/GettingStarted \
	examples/Chibios/Stream \
	utility/Linux

all: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

.PHONY: all $(SUBDIRS)
