#common makefile header

DIR_INC = ../../include
DIR_BIN = ../aiui_sample/build/
DIR_LIB = ../../libs

TARGET	= awaken_offline_sample
BIN_TARGET = $(DIR_BIN)/$(TARGET)

CROSS_COMPILE = 
CFLAGS = -g -Wall -I$(DIR_INC)

ifdef LINUX64
LDFLAGS := -L$(DIR_LIB)/x64
else
LDFLAGS := -L$(DIR_LIB)/x86 
endif
LDFLAGS += -lmsc -lrt -ldl -lpthread -lasound -lstdc++

OBJECTS := $(patsubst %.c,%.o,$(wildcard *.c))


$(BIN_TARGET) : $(OBJECTS)
	$(CROSS_COMPILE)g++ $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o : %.c
	$(CROSS_COMPILE)g++ -c $(CFLAGS) $< -o $@
clean:
	@rm -f *.o $(BIN_TARGET)

.PHONY:clean

#common makefile foot
