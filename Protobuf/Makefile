PROTOBUFS=Dir EntranceEntity WallLightEntity GravitySwitchEntity FloorButtonEntity \
CubeEntity GooPlaneEntity WaterPlaneEntity DecalEntity PlatformEntity GravityBarrierEntity \
Activator Vec
BUILD_DIR=Build

all: $(BUILD_DIR) $(addprefix $(BUILD_DIR)/, $(addsuffix .pb.h, $(PROTOBUFS)))

$(BUILD_DIR):
	mkdir -p Build

$(BUILD_DIR)/%.pb.h: %.proto
	protoc --cpp_out=$(BUILD_DIR) $<
