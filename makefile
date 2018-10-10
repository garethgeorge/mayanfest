CPPCC=g++
CC=gcc
CPPFLAGS= -std=c++11 -g -O0 
CFLAGS= 

OBJS=src/diskinterface.o src/segment.o src/filesystem.o
INCLUDES=-I ./3rdparty/ -I ./src/
TEST_OBJS=tests/diskinterface-test.o \
	# tests/segment-test.o \
	tests/filesystem-test.o 

all: test

test: ${TEST_OBJS} ${OBJS}
	${CPPCC} ${CPPFLAGS} -o test tests/test-main.cpp ${TEST_OBJS} ${OBJS} ${INCLUDES}

%.o: %.c
	# @echo CC $@
	${CC} -c ${CFLAGS} $< -o $@ ${INCLUDES}

%.o: %.cpp
	# @echo CPPCC $@
	${CPPCC} -c ${CPPFLAGS} $< -o $@ ${INCLUDES}

clean:
	rm -f filesystem
	find . -type f -name '*.o' -delete