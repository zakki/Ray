EMCXX ?= em++
WEB_DIR ?= build/web
WEB_OBJECT_DIR ?= obj/web

CXXFLAGS ?= -O3
WEB_CXXFLAGS := -std=c++11 -DRAY_WEB -Iinclude -MMD -MP
LDFLAGS ?= -O3
WEB_LDFLAGS := -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512MB \
	-sSTACK_SIZE=8MB -sEXIT_RUNTIME=0 \
	-sEXPORTED_FUNCTIONS=_main,_GTP_exec -sEXPORTED_RUNTIME_METHODS=ccall \
	--pre-js web/loader.js \
	--preload-file sim_params@/sim_params --preload-file uct_params@/uct_params

SOURCES := $(shell find src -name '*.cpp')
OBJECTS := $(patsubst src/%.cpp,$(WEB_OBJECT_DIR)/%.o,$(SOURCES))
PARAMETERS := $(wildcard sim_params/*.txt uct_params/*.txt)

.PHONY: all
all: $(WEB_DIR)/ray.js $(WEB_DIR)/ray.wasm $(WEB_DIR)/ray.data $(WEB_DIR)/index.html

$(WEB_DIR)/ray.js $(WEB_DIR)/ray.wasm $(WEB_DIR)/ray.data &: $(OBJECTS) $(PARAMETERS) sim_params uct_params web/loader.js web/Makefile
	@mkdir -p $(WEB_DIR)
	$(EMCXX) $(OBJECTS) $(LDFLAGS) $(WEB_LDFLAGS) -o $(WEB_DIR)/ray.js

$(WEB_OBJECT_DIR)/%.o: src/%.cpp web/Makefile
	@mkdir -p $(dir $@)
	$(EMCXX) $(CPPFLAGS) $(CXXFLAGS) $(WEB_CXXFLAGS) -c $< -o $@

$(WEB_DIR)/index.html: web/index.html
	@mkdir -p $(WEB_DIR)
	cp $< $@

-include $(OBJECTS:.o=.d)
