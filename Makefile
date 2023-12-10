CFLAGS += -O2 -W{all,extra,error}

SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)

OBJDIR = build
OBJDIR.release = $(OBJDIR)/release/
OBJDIR.debug = $(OBJDIR)/debug/
OBJDIR.test = $(OBJDIR)/test/

OBJ = ilish.o
OBJ.release = $(OBJDIR.release)$(OBJ)
OBJ.debug = $(OBJDIR.debug)$(OBJ)

MKDIR = mkdir -p
MKDIR.release = $(MKDIR) $(OBJDIR.release)
MKDIR.debug = $(MKDIR) $(OBJDIR.debug)
MKDIR.test = $(MKDIR) $(OBJDIR.test)

# TEST = $(OBJDIR.test)test
# TRY = $(OBJDIR.test)try
# TRUTH = $(OBJDIR.test)truth

all: release

build: debug

release:
	${MKDIR.release}
	${CC} $(CFLAGS) $(SRC) -o $(OBJ.release)

debug:
	${MKDIR.debug}
	${CC} $(CFLAGS) -DDEBUG -pg -Og -ggdb3 $(SRC) -o $(OBJ.debug)

# test: debug
# 	${MKDIR.test}
# 	echo "'a' pop" | tee $(TEST)
# 	echo "2 pop" | tee -a $(TEST)
# 	echo "[] pop" | tee -a $(TEST)
# 	echo "\"Hey\" pop" | tee -a $(TEST)

# 	echo "" > $(TRY)
# 	./$(OBJ.debug) -f $(TEST) | tee -a $(TRY)

# 	echo "" > $(TRUTH)
# 	diff $(TRUTH) $(TRY)

clean:
	rm -rf $(OBJDIR)
