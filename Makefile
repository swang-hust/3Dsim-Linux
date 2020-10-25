NAME = 3dsim
INC_DIR += ./include
BUILD_DIR ?= ./build

# So ... what is this mean?
ifeq ($(SHARE), 1)
SO = -so
SO_CFLAGS = -fPIC -D_SHARE=1
SO_LDLAGS = -shared -fPIC
endif

OBJ_DIR ?= $(BUILD_DIR)/obj$(SO)
BINARY ?= $(BUILD_DIR)/$(NAME)$(SO)

.DEFAULT_GOAL = app

# Compilation flags
CC = gcc
LD = gcc
INCLUDES  = $(addprefix -I, $(INC_DIR))
CFLAGS += -O2 -MMD -Wall -Werror -ggdb3 $(INCLUDES) -fomit-frame-pointer
# CFLAGS += -O2 -MMD -ggdb3 $(INCLUDES) -fomit-frame-pointer

# Files to be compiled
SRCS = $(shell find src/ -name "*.c")
OBJS = $(SRCS:src/%.c=$(OBJ_DIR)/%.o)

# Compilation patterns
$(OBJ_DIR)/%.o: src/%.c
	@echo + CC $<
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(SO_CFLAGS) -c -o $@ $<

# Depencies
-include $(OBJS:.o=.d)

.PHONY: app run clean
app: $(BINARY)

# Command to execute NEMU
3DSIM_EXEC := $(BINARY)

$(BINARY): $(OBJS)
	@echo + LD $@
	@$(LD) -O2 -rdynamic $(SO_LDLAGS) -o $@ $^

run: $(BINARY)
	$(3DSIM_EXEC)

clean: 
	@rm -rf build/
