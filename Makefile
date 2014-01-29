PROJ=gitminer

CC=gcc

CFLAGS=-std=gnu99 -Wall -DUNIX -g -DDEBUG

PROC_TYPE = $(strip $(shell uname -m | grep 64))
OS = $(shell uname -s 2>/dev/null | tr [:lower:] [:upper:])
DARWIN = $(strip $(findstring DARWIN, $(OS)))

ifneq ($(DARWIN),)
	CFLAGS += -DMAC
	LIBS=-framework OpenCL

	ifeq ($(PROC_TYPE),)
		CFLAGS+=-arch i386
	else
		CFLAGS+=-arch x86_64
	endif
else

LIBS=-lOpenCL -lpthread -lrt
ifeq ($(PROC_TYPE),)
	CFLAGS+=-m32
else
	CFLAGS+=-m64
endif

ifdef AMDAPPSDKROOT
   INC_DIRS=. $(AMDAPPSDKROOT)/include
	ifeq ($(PROC_TYPE),)
		LIB_DIRS=$(AMDAPPSDKROOT)/lib/x86
	else
		LIB_DIRS=$(AMDAPPSDKROOT)/lib/x86_64
	endif
else

ifdef CUDA
   INC_DIRS=. $(CUDA)/OpenCL/common/inc
endif
INC_DIRS=. /usr/local/cuda-5.5/include

endif
endif

$(PROJ): $(PROJ).c git.c sha1.c cl.c
	$(CC) $(CFLAGS) -o gitminer2 $^ $(INC_DIRS:%=-I%) $(LIB_DIRS:%=-L%) $(LIBS)

.PHONY: clean

clean:
	rm $(PROJ)

astyle:
	astyle --options=astyle.config *.{c,cl}
