CFLAGS += -O2 -W{all,extra,error}
DEBUGFLAGS = -DDEBUG -pg -Og -ggdb3

SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)

OBJDIR = build
OBJDIR.release = $(OBJDIR)/release/
OBJDIR.debug = $(OBJDIR)/debug/

OBJ = ilish
OBJ.release = $(OBJDIR.release)$(OBJ)
OBJ.debug = $(OBJDIR.debug)$(OBJ)

MKDIR = mkdir -p
MKDIR.release = $(MKDIR) $(OBJDIR.release)
MKDIR.debug = $(MKDIR) $(OBJDIR.debug)

DOC = doxygen
DOCCONF = Doxyfile

all: release

build: debug

release:
	${MKDIR.release}
	${CC} $(CFLAGS) $(SRC) -o $(OBJ.release)

debug:
	${MKDIR.debug}
	${CC} $(CFLAGS) $(DEBUGFLAGS) $(SRC) -o $(OBJ.debug)

doc:
	$(DOC) $(DOCCONF)

clean:
	rm -rf $(OBJDIR)
