.PHONY: build run san clean install release

OUT := build/edit
FILES := src/main.c /usr/local/lib/tools.o
BASE_FLAGS := -ggdb -rdynamic -fuse-ld=mold -fmax-errors=1 -std=c11 -pipe
RELEASE_FLAGS := -O2

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wuninitialized -Wcast-qual -Wdisabled-optimization -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wstrict-prototypes -Wpointer-to-int-cast -Wint-to-pointer-cast -Wconversion -Wduplicated-cond -Wduplicated-branches -Wformat=2 -Wshift-overflow=2 -Wint-in-bool-context -Wvector-operation-performance -Wvla -Wdisabled-optimization -Wredundant-decls -Wmissing-parameter-type -Wold-style-declaration -Wlogical-not-parentheses -Waddress -Wmemset-transposed-args -Wmemset-elt-size -Wsizeof-pointer-memaccess -Wwrite-strings -Wtrampolines -Werror=implicit-function-declaration -Winvalid-pch

PATH_FLAGS_FT := -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/include/sysprof-4

PATH_FLAGS := $(PATH_FLAGS_FT)
LINK_FLAGS := -fuse-ld=lld -lfreetype -lglfw -lvulkan -ldl -pthread -lX11 -lXxf86vm -lXrandr -lXi -lm

#SAN_FLAGS := -fsanitize=undefined -fsanitize=address
SAN_FLAGS := -fsanitize-trap=all

export GCC_COLORS = warning=01;33

edit: build/edit

install: release
	cp build/edit ~/.local/bin/edit-bin

build: build/edit

build/main_frag.spv: shaders/main_frag.glsl
	@glslc -fshader-stage=frag shaders/main_frag.glsl -O -o build/main_frag.spv
	@xxd -i build/main_frag.spv build/main_frag.h
build/main_vert.spv: shaders/main_vert.glsl
	@glslc -fshader-stage=vert shaders/main_vert.glsl -O -o build/main_vert.spv
	@xxd -i build/main_vert.spv build/main_vert.h

release: build/main_vert.spv build/main_frag.spv src/*
	@gcc $(WARN_FLAGS) $(PATH_FLAGS) $(OPT_FLAGS) $(BASE_FLAGS) $(FILES) $(LINK_FLAGS) -o$(OUT)

build/edit: build/main_vert.spv build/main_frag.spv src/*
	@gcc $(WARN_FLAGS) $(PATH_FLAGS) $(BASE_FLAGS) $(FILES) $(LINK_FLAGS) -o$(OUT)

san: build/main_vert.spv build/main_frag.spv src/*
	@gcc $(WARN_FLAGS) $(PATH_FLAGS) $(SAN_FLAGS) $(BASE_FLAGS) $(FILES) $(LINK_FLAGS) -o$(OUT)

clean:
	@rm -r build/*
