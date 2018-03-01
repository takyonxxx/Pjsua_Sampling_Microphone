# AUTO-GENERATED; DO NOT MODIFY 
include param.make

SRC = $(wildcard *.cpp)
OBJ = $(SRC:%.cpp=%.o)
CFLAGS = -g -Wall -Wextra -Werror -Wfatal-errors $(USERFLAGS) $(USERDEFINES)
RAWINCPATH = $(foreach dep,$(DEPS),-I.libs/$(dep)/inc)
IPATH = $(subst _,,$(subst +,,$(RAWINCPATH)))
RAWLIBPATH = $(foreach dep,$(DEPS),-L.libs/$(dep)/lib -l$(dep))
LPATH = $(subst _,,$(subst +,,$(RAWLIBPATH)))

ifeq ($(CXX),g++)
ARCH=$(shell uname -i)
else
ARCH="omap4430"
endif

CSET=$(shell hg id -i)
HEX=$(subst +,,$(CSET))
MOD=$(findstring +,$(CSET))

OK="\033[1;32m"
FAIL="\033[1;31m"
DONE="\033[0m"

all: makebin tarball

makebin: Version $(DEPS) $(OBJ) 
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJ) $(LPATH) $(USERLIBS) $(SYSROOT)

.cpp.o:
	$(CXX) $(CFLAGS) $(IPATH) -c -o $*.o $<

+%:
	mkdir -p .libs
	mkdir -p ~/.local_libraries
	tar zxvf ~/.local_libraries/$*.tgz -C .libs
	$(RM) $*.tgz

_%:
	mkdir -p .libs
	$(eval INP := $(subst @, ,$*))
	$(eval PACKAGE := $(word 1, $(INP)))
	$(eval REV := $(word 2, $(INP)))
	$(if $(REV), \
		$(eval PKGNAME := $(PACKAGE).$(REV)), \
		$(eval PKGNAME := current) )

	curl ftp://poyraz/dist/$(PACKAGE)/$(PKGNAME).$(ARCH).tgz -o $(PACKAGE).tgz -u builder:builder
	tar zxvf $(PACKAGE).tgz -C .libs
	$(RM) $(PACKAGE).tgz

Version: $(DEPS)
	revlib.py bin $(NAME)

clean:
	$(RM) -rf *.o *core* $(TARGET) Version.h .libs *~ .*swp *.log

tarball: makebin
	mkdir -p ~/.local_binaries
	mkdir -p $(NAME)
	cp $(TARGET) $(NAME)/
	tar -c $(NAME) | gzip -c > ~/.local_binaries/$(NAME).tgz
	rm -rf $(NAME)

dist: tarball
	$(if $(MOD), \
		echo $(FAIL)"Modified! Commit and Push first!"$(DONE), \
		curl -T ~/.local_binaries/$(NAME).tgz ftp://poyraz/packages/$(NAME)/$(NAME).$(HEX).$(ARCH).tgz --ftp-create-dirs --user installer:installer; \
		curl -T ~/.local_binaries/$(NAME).tgz ftp://poyraz/packages/$(NAME)/current.$(ARCH).tgz --ftp-create-dirs --user installer:installer; \
		echo $(OK)"Deployment package is built and uploaded!"$(DONE) )

