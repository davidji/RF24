
PROJECT=rf24

RF24=$(realpath ../..)
INCDIR=$(RF24)/include $(realpath include)
CFLAGS=-O2 $(patsubst %,-I%,$(INCDIR))
CPPFLAGS=-O2 -std=c++11 $(patsubst %,-I%,$(INCDIR))
VPATH=. $(RF24)
OBJECTS=rf24_linux_io.o RF24.o
BUILDDIR=build


IINCDIR=
OBJDIR=$(BUILDDIR)/objs
LIB=$(BUILDDIR)/$(PROJECT).a

.PHONY: directories

all: directories $(LIB)

directories:
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o : %.c Makefile $(OBJDIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp Makefile $(OBJDIR)
	$(CPP) -c $(CPPFLAGS) $< -o $@

$(LIB): $(addprefix $(OBJDIR)/,$(OBJECTS))
	$(AR) rcs -o $@ $^

all: directories $(LIB)

clean:
	rm -Rf $(BUILDDIR)
