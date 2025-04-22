# Authors: 
# Odin Bjerke <odin.bjerke@uit.no>
# Morten Grønnesby <morten.gronnesby@uit.no>

# ===============
# === Options ===
# ===============

# The compiler to utilize
CC ?= gcc

# The `DEBUG` variable controls whether we compile for debugging or optimization.
# If 1, builds to build/debug. Otherwise, build/release.
DEBUG ?= 1

# Name of the target executable
EXEC_NAME = indexer

# Select one implementation per ADT (see README.md for info)
ADT_MAP = hashmap.c
ADT_LIST = doublylinkedlist.c
ADT_SET = rbtreeset.c
ADT_INDEX = index.c

# If you define other headers within adt (e.g. stack, heap), 
# declare the source file for it above and include in the following:
ADT_SRC = $(ADT_MAP) $(ADT_LIST) $(ADT_SET) $(ADT_INDEX)


# ======================
# === Compiler Flags ===
# ======================

# Linked libraries (-lm is for <math.h>)
LDFLAGS += -lm

# Specify c2x (C23) as c/libc standard, enable GNU C-lib extensions
CFLAGS += -std=c2x -D _GNU_SOURCE

# Automatically create dependancy files. This ensures we re-make on header changes, etc.
CFLAGS += -MMD -MP

ifeq ($(DEBUG), 0)
# === Compiler flags for the release build ===
# - '-O3' 			sets the highest optimization level.
# - '-D NDEBUG' 	disables most assertions/prints. Widely used in GNU C-lib.
CFLAGS += -D NDEBUG -O3
else
# === Compiler flags for the debug build ===
# '-g' 							 	enables debugging information in the compiled executable (nescessary for gdb/lldb/etc)
# '-Wall, -Wextra' 					enables most relevant warnings
# '-Werror' 						treats warnings as errors
# '-D DEBUG' 					 	opposite of NDEBUG, where even more output may be available.
CFLAGS += -g -Wall -Wextra -Werror -D DEBUG
endif

# --- careful with editing anything below ---

# ===================
# === Directories ===
# ===================

# Source directories
INCLUDE_DIR = include
SRC_DIR = src

# Other
DOC_DIR = doc
LOG_DIR = log

# Nested source directories
SRC_ADT_DIR = $(SRC_DIR)/adt

# Output directories
BUILD_DIR = build
OBJ_DIR = obj

# Set target dir depending on DEBUG
ifeq ($(DEBUG), 0)
TARGET_DIR = $(BUILD_DIR)/release
else
TARGET_DIR = $(BUILD_DIR)/debug
endif


# =================================
# === Source & Dependancy Files ===
# =================================

# Source files
SRC_COMMON := $(wildcard $(SRC_DIR)/*.c)
SRC_ADT := $(patsubst %,$(SRC_ADT_DIR)/%,$(ADT_SRC))
SRC := $(SRC_COMMON) $(SRC_ADT) 

# Include all header subdirectories
HEADERS := $(shell find $(INCLUDE_DIR) -type d)
INCLUDE_FLAGS := $(addprefix -I,$(HEADERS))

# Target objects
OBJ := $(patsubst $(SRC_DIR)/%.c,$(TARGET_DIR)/$(OBJ_DIR)/%.o,$(SRC))
EXEC = $(TARGET_DIR)/$(EXEC_NAME)

# Object dependancy files
DEP := $(OBJ:.o=.d)


# ==================
# === Make Rules ===
# ==================
#
# Note:
# `@mkdir -p $(dir $@)` ensures our output directories exist for all targets.
# - the leading `@` simply silences the command from being output by make (feel free to remove it)
# - the `-p` flag instructs mkdir to create missing intermediate path name directories
# - `$(dir $@)` gets the directory name for the target, where `$@` is make syntax for 'this target'
#

# default command that is run when simply `make` is typed with no arguments
.PHONY: all
all: exec

# alias for our current executable target (e.g. `build/release/myexec`)
.PHONY: exec
exec: $(EXEC)

.PHONY: logdir
logdir:
	@mkdir -p $(LOG_DIR)

# Rule to compile the main executable
$(EXEC): $(OBJ) $(HEADERS) Makefile
	$(info === Compiling executable: $@ ===)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# Rule to compile dependancy objects
$(TARGET_DIR)/$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c $< -o $@

# Clean up source files and dependancies, but leave directories
.PHONY: clean
clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(EXEC)

# Clean for for delivery
.PHONY: distclean
distclean: clean
	rm -rf $(BUILD_DIR)


# include dependancy (.d) files
-include $(DEP)
