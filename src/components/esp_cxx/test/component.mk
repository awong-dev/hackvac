CXXFLAGS += -std=gnu++17
#COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
COMPONENT_ADD_LDFLAGS = -Wl,-force_load $(COMPONENT_BUILD_DIR)/$(COMPONENT_LIBRARY)
