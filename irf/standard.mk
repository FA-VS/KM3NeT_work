# Paths and Prefix
PREFIX = $(IRF_DIR)

# Object directory
OBJ_DIR = $(PREFIX)/build

# Include and library directories
INCLUDES = -I$(ROOTSYS)/include -I/usr/include/root/ -I$(PREFIX) -I$(shell root-config --incdir) 

# Library paths
#LIBS = -L$(PREFIX)/km3net-dataformat/lib -lKM3NeTROOT -luuid `root-config --libs --glibs`
LIBS = -L$(PREFIX)/km3net-dataformat/lib -luuid $(shell root-config --libs)

# Compiler and Flags
CXX = $(shell root-config --cxx)
CXXFLAGS = $(shell root-config --cflags) -Wall -fPIC

# Default object compilation rule
$(OBJ_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

