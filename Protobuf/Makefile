PROTOBUFS=Dir EntranceEntity WallLightEntity GravitySwitchEntity FloorButtonEntity World \
CubeEntity GooPlaneEntity WaterPlaneEntity DecalEntity PlatformEntity GravityBarrierEntity PointLightEntity \
Activator Vec CubeSpawnerEntity ForceFieldEntity Ramp WindowEntity WaterWallEntity SlidingWallEntity LadderEntity
BUILD_DIR=Build

all: $(BUILD_DIR) $(addprefix $(BUILD_DIR)/, $(addsuffix .pb.h, $(PROTOBUFS)))

$(BUILD_DIR):
	mkdir -p Build

$(BUILD_DIR)/%.pb.h: %.proto
	protoc --cpp_out=$(BUILD_DIR) $<
