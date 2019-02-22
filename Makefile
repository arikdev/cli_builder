TOP_DIR = $(CURDIR)
SRC_DIR = $(TOP_DIR)/src
INCLUDE_DIR = $(TOP_DIR)/include
BUILD_DIR = $(TOP_DIR)/build
SRCS = $(wildcard $(SRC_DIR)/*.c)
INCLUDES = $(wildcard $(INCLUDE_DIR)/*.h)
BUILD_SRCS = $(SRCS:$(SRC_DIR)%=$(BUILD_DIR)%)
BUILD_INCLUDES = $(INCLUDES:$(INCLUDE_DIR)%=$(BUILD_DIR)%)
OBJS = $(BUILD_SRCS:%.c=%.o)
CPPFLAGS += -I$(BUILD_DIR)
TARGET = $(BUILD_DIR)/test
CFLAGS += $(if $(DEBUG),-g -O0 -DDEBUG)
LIB = $(BUILD_DIR)/libcli.a
LDFLAGS = -L$(BUILD_DIR) -lcli

$(TARGET): $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) test.c $(LDFLAGS) -o $@

$(LIB): $(OBJS)
	ar rcs $@ $^

$(OBJS): $(BUILD_SRCS) $(BUILD_INCLUDES)

$(BUILD_SRCS): $(SRCS)
	ln -fs $(@:$(BUILD_DIR)%=$(SRC_DIR)%) $@
$(BUILD_INCLUDES): $(INCLUDES)
	ln -fs $(@:$(BUILD_DIR)%=$(INCLUDE_DIR)%) $@

$(SRCS): $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

print-%:
	@echo $* $($*)

clean:
	@rm -rf $(BUILD_DIR)

