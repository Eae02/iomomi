SC=glslangValidator
FLAGS=-G --auto-map-locations

OUT_PATH=../Data/Shaders

SHADERS=ImGui.fs ImGui.vs
HEADER_FILES=$(wildcard Inc/*.glh)

all: $(addsuffix .spv,$(addprefix $(OUT_PATH)/,$(SHADERS)))

$(OUT_PATH)/%.vs.spv : %.vs.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S vert $< -o $@

$(OUT_PATH)/%.fs.spv : %.fs.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S frag $< -o $@
	
$(OUT_PATH)/%.gs.spv : %.gs.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S geom $< -o $@

$(OUT_PATH)/%.tcs.spv : %.tcs.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S tesc $< -o $@

$(OUT_PATH)/%.tes.spv : %.tes.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S tese $< -o $@

$(OUT_PATH)/%.cs.spv : %.cs.glsl $(HEADER_FILES)
	$(SC) $(FLAGS) -S comp $< -o $@
