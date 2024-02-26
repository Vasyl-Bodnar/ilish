CFLAGS += -O2 -W{all,extra,error,pedantic}
DEBUGFLAGS = -DDEBUG -pg -Og -ggdb3

SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
SRC.runtime = runtime/runtime.c

OBJDIR = build
OBJDIR.release = $(OBJDIR)/release/
OBJDIR.debug = $(OBJDIR)/debug/
OBJDIR.runtime = $(OBJDIR)/runtime/

OBJ = ilish
OBJ.release = $(OBJDIR.release)$(OBJ)
OBJ.debug = $(OBJDIR.debug)$(OBJ)
OBJ.runtime = $(OBJDIR.runtime)runtime.o

MKDIR = mkdir -p
MKDIR.release = $(MKDIR) $(OBJDIR.release)
MKDIR.debug = $(MKDIR) $(OBJDIR.debug)
MKDIR.runtime = $(MKDIR) $(OBJDIR.runtime)

DOC = doxygen
DOCCONF = Doxyfile

all: release runt

runt:
	${MKDIR.runtime}
	${CC} $(CFLAGS) -c $(SRC.runtime) -o $(OBJ.runtime)

release:
	${MKDIR.release}
	${CC} $(CFLAGS) $(SRC) -o $(OBJ.release)

debug:
	${MKDIR.debug}
	${CC} $(CFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OBJ.debug)

doc:
	$(DOC) $(DOCCONF)

clean:
	rm -f $(OBJ.release)
	rm -f $(OBJ.debug)
	rm -f $(OBJ.runtime)
	rm -r $(OBJDIR.release)
	rm -r $(OBJDIR.debug)
	rm -r $(OBJDIR.runtime)

cleandoc:
	rm -rf $(OBJDIR)/docs
