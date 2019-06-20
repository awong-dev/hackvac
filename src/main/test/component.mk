CXXFLAGS += -std=c++17
ifeq ($(shell uname -s),Darwin)
COMPONENT_ADD_LDFLAGS = -Wl,-force_load $(COMPONENT_BUILD_DIR)/$(COMPONENT_LIBRARY)
else
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
endif
