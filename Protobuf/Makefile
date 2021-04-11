BUILD_DIR=Build

all: $(BUILD_DIR) $(addprefix $(BUILD_DIR)/, $(patsubst %.proto,%.pb.h,$(wildcard *.proto)))

$(BUILD_DIR):
	mkdir -p Build

$(BUILD_DIR)/%.pb.h: %.proto
	protoc --cpp_out=$(BUILD_DIR) $<
