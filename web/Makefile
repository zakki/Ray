EMCXX ?= em++
WEB_DIR ?= build/web
WEB_OBJECT_DIR ?= obj/web
PYTHON ?= python3
WEB_UCT_FORMAT ?= log8
UCT_PARAMETERS := $(WEB_OBJECT_DIR)/uct_params.bin
UCT_FORMAT_STAMP := $(WEB_OBJECT_DIR)/uct-format

CXXFLAGS ?= -O3
WEB_CXXFLAGS := -std=c++11 -DRAY_WEB -Iinclude -MMD -MP
LDFLAGS ?= -O3
WEB_LDFLAGS := -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512MB \
	-sSTACK_SIZE=8MB -sEXIT_RUNTIME=0 \
	-sEXPORTED_FUNCTIONS=_main,_GTP_exec -sEXPORTED_RUNTIME_METHODS=ccall \
	--pre-js web/loader.js \
	--preload-file sim_params@/sim_params --preload-file $(UCT_PARAMETERS)@/uct_params.bin

SOURCES := $(shell find src -name '*.cpp')
OBJECTS := $(patsubst src/%.cpp,$(WEB_OBJECT_DIR)/%.o,$(SOURCES))
SIM_PARAMETERS := $(wildcard sim_params/*.txt)
UCT_TEXT_PARAMETERS := $(wildcard uct_params/*.txt)

.PHONY: all
all: $(WEB_DIR)/ray.js $(WEB_DIR)/ray.wasm $(WEB_DIR)/ray.data $(WEB_DIR)/index.html $(WEB_DIR)/ray.data.gz

$(WEB_DIR)/ray.js $(WEB_DIR)/ray.wasm $(WEB_DIR)/ray.data &: $(OBJECTS) $(SIM_PARAMETERS) $(UCT_PARAMETERS) sim_params web/loader.js web/Makefile
	@mkdir -p $(WEB_DIR)
	$(EMCXX) $(OBJECTS) $(LDFLAGS) $(WEB_LDFLAGS) -o $(WEB_DIR)/ray.js

$(WEB_OBJECT_DIR)/%.o: src/%.cpp web/Makefile
	@mkdir -p $(dir $@)
	$(EMCXX) $(CPPFLAGS) $(CXXFLAGS) $(WEB_CXXFLAGS) -c $< -o $@

$(WEB_DIR)/index.html: web/index.html
	@mkdir -p $(WEB_DIR)
	cp $< $@

-include $(OBJECTS:.o=.d)

# Changing the encoding must rebuild the package even if its previous variant exists.
.PHONY: FORCE
FORCE:

$(UCT_FORMAT_STAMP): FORCE
	@mkdir -p $(dir $@)
	@$(PYTHON) -c 'from pathlib import Path; p=Path("$@"); value="$(WEB_UCT_FORMAT)\n"; p.write_text(value) if not p.exists() or p.read_text() != value else None'

$(UCT_PARAMETERS): $(UCT_TEXT_PARAMETERS) uct_params tools/pack_uct_params.py $(UCT_FORMAT_STAMP)
	$(PYTHON) tools/pack_uct_params.py --input uct_params --output $@ --format $(WEB_UCT_FORMAT)

# Optional precompressed HTTP response; serve with Content-Encoding: gzip.
$(WEB_DIR)/ray.data.gz: $(WEB_DIR)/ray.data
	$(PYTHON) -c 'import gzip; from pathlib import Path; p=Path("$<"); Path("$@").write_bytes(gzip.compress(p.read_bytes(), compresslevel=9, mtime=0))'
