PROTOBUFS=Dir EntranceEntity WallLightEntity GravitySwitchEntity

all: $(addprefix Build/, $(addsuffix .pb.h, $(PROTOBUFS)))

Build/%.pb.h : %.proto
	protoc --cpp_out=Build $<
