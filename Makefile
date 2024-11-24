
ifneq (, $(shell which clang) )
CC	= clang
else
CC  = gcc
endif

EXERCISES	?=
CLEAN_FILES	?=

.DEFAULT_GOAL	= all
.PHONY: all clean

-include *.mk

all:	${EXERCISES}

clean:
	-rm -f ${CLEAN_FILES}
