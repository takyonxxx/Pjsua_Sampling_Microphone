# MAKE PARAMETERS FOR BINARY FILES
NAME = yketiccrypto
TARGET = yketiccrypto.bin
# Use  _mgmtagent@dd8f15c28533 for specific version, otherwise current package is used.
DEPS =

# Preprocessor definitions
override USERDEFINES +=
# Sample :
# USERDEFINES = -DENABLE_UNITY -DENABLE_SOUND -DDEBUG_MESSAGES #-DSINGLETON_DEBUG

# Compiler flags
USERFLAGS = `pkg-config --cflags libpjproject`
# Sample :
# USERFLAGS = $(shell sdl-config --cflags)
# USERFLAGS = $(USERFLAGS) $(shell pkg-config ftgl --cflags)

# Libraries
USERLIBS = `pkg-config --libs libpjproject`
# Sample :
# USERLIBS = $(shell sdl-config --libs)
# USERLIBS = $(USERLIBS) $(shell pkg-config ftgl --libs)
# USERLIBS = $(USERLIBS) -lzmq -llua5.1 -lGL -lconfig -lm -lsfml-network -lSUSI-3.02
