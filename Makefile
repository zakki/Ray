include Makefile.config

DEBUG ?= 0
RELEASE ?= 0

ifeq ($(DEBUG), 1)
	CXXFLAGS += -O0 -g #-ggdb
	LDFLAGS  += -O0 -g -ggdb
else
	CXXFLAGS += -O3
	LDFLAGS  += -O3
endif

TARGET = ray
CXX = g++
CXXFLAGS += -std=c++11
WARNING = -Wall -W

SOURCE_DIR = ./src
OBJECT_DIR = ./obj
INCLUDE_DIR = ./include

SOURCES = $(shell find $(SOURCE_DIR) -name '*.cpp')


OBJECTS = $(subst $(SOURCE_DIR), $(OBJECT_DIR), $(SOURCES))
OBJECTS := $(subst .cpp,.o,$(OBJECTS))
OBJECTS := $(subst .cu,.o,$(OBJECTS))

DEPENDS = $(OBJECTS:.o=.d)

INCLUDE += -I$(INCLUDE_DIR)

ifeq ($(RELEASE), 1)
	LDFLAGS += -static -lm -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
else
	LDFLAGS += -lm -lpthread
endif

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIB_CUDA) $(LIB_REDIS)

$(SOURCE_DIR)/%.cpp: $(SOURCE_DIR)/%.cu
	$(NVCC) $(NVCCFLAGS) $(INCLUDE) --cuda $< -o $@

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@if [ ! -e `dirname $@` ]; then mkdir -p `dirname $@`; fi
	$(CXX) $(WARNING) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

.PHONY: web
web:
	$(MAKE) -f web/Makefile

clean:
	-rm -f *~ $(TARGET) $(OBJECTS) $(SOURCE_DIR)/*~ $(SOURCE_DIR)/*/*~ $(SOURCE_DIR)/*/*/*~ $(SOURCE_DIR)/*/*/*/*~ $(INCLUDE_DIR)/*~ $(INCLUDE_DIR)/*/*~ $(INCLUDE_DIR)/*/*/*~ $(INCLUDE_DIR)/*/*/*/*~

-include $(DEPENDS)
