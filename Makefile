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

############## Do not change anything from here downwards! #############
SRC = $(wildcard $(SRCDIR)/*$(EXT))
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o)
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
$(APPNAME): $(OBJ)
	@echo "building the executable..."
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Creates the dependecy rules
%.d: $(SRCDIR)/%$(EXT)
	@echo "generating .d files..."
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:%.d=$(OBJDIR)/%.o) >$@

# Includes all .h files
#-include $(DEP)



# Building rule for .o files and its .c/.cpp in combination with all .h
# $@ name of the target of the rule
# $< name of the first dependency
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	@echo "compiling files in obj directory..."	
	$(CC) $(CXXFLAGS) -o $@ -c $<

################### Cleaning rules for Unix-based OS ###################
# Cleans complete project
.PHONY: clean
clean:
	$(RM) $(DELOBJ) $(DEP) $(APPNAME)

# Cleans .o files
.PHONY: clean_o
clean_o:
	@echo "removing .o files..."	
	$(RM) $(DELOBJ)

# Cleans only all files with the extension .d
.PHONY: cleandep
cleandep:
	$(RM) $(DEP)