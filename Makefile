########################################################################
####################### Makefile Template ##############################
########################################################################

# Compiler settings - Can be customized.
CC = g++
CXXFLAGS = -std=c++11 -Wall
LDFLAGS = -pthread

# Makefile settings - Can be customized.
APPNAME = trading_app
EXT = .cpp
SRCDIR = source
OBJDIR = obj

VAR1 = HOLA
VAR2 = $(VAR1) CHAO

############## Do not change anything from here downwards! #############

# variable
SRC = $(wildcard $(SRCDIR)/*$(EXT))
SRC2 = $(wildcard *.cpp)

# Substitution reference
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o)

OBJ2 = $(SRC2:%$(EXT)=%.o)


DEP = $(OBJ:$(OBJDIR)/%.o=%.d)

# UNIX-based OS variables & settings
RM = rm
DELOBJ = $(OBJ)

########################################################################
####################### Targets beginning here #########################
########################################################################

all: $(APPNAME)

# Builds the app
# $@ name of the target of the rule
# $^ names of all dependencies with spaces between them
$(APPNAME): $(OBJ) $(OBJ2)
	@echo "building the executable..."
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Creates the dependecy rules
#%.d: $(SRCDIR)/%$(EXT)
#	@echo "generating .d files..."
#	@$(CPP) $(CFLAGS) $< -MM -MT $(@:%.d=$(OBJDIR)/%.o) >$@

# Includes all .h files
#-include $(DEP)

%.o: %$(EXT)
	@echo "compiling files in the current directory..."	
	$(CC) $(CXXFLAGS) -o $@ -c $<

# Building rule for .o files and its .c/.cpp in combination with all .h
# $@ target
# $< dependency
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	$(info $$SRC is [${SRC}])
	$(info $$SRC2 is [${SRC2}])
	$(info $$VAR2 is [${VAR2}])		
	$(info $$OBJ is [${OBJ}])
	$(info $$OBJ2 is [${OBJ2}])
	@echo "compiling files in obj/ directory..."	
	$(CC) $(CXXFLAGS) -o $@ -c $<


################### Cleaning rules for Unix-based OS ###################

# Cleans complete project
#.PHONY: clean
#clean:
#	$(RM) $(DELOBJ) $(DEP) $(APPNAME)

# Cleans .o files
.PHONY: clean_o
clean_o:
	@echo "removing .o files..."	
	$(RM) $(DELOBJ)
	$(RM) $(OBJ2)
	$(RM) $(APPNAME)

# Cleans only all files with the extension .d
#.PHONY: cleandep
#cleandep:
#	$(RM) $(DEP)