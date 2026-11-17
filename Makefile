CXXFLAGS ?= -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Wshadow
WASI ?= $(HOME)/Library/Caches/cambc/wasi-sdk-33

.PHONY: test wasm-check clean

# lib/kit against slow reference answers, with the sanitizers on.
test: build/test_kit
	./build/test_kit

build/test_kit: tests/test_kit.cpp lib/kit/*.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -Ilib $< -o $@

# lib/kit builds with the toolchain `cambc build` uses (wasm32-wasip1).
wasm-check: tests/test_kit.cpp lib/kit/*.h
	@mkdir -p build
	$(WASI)/bin/clang++ --target=wasm32-wasip1 -std=c++20 -O2 -Wall -Wextra -msimd128 -mbulk-memory \
		-fwasm-exceptions -mllvm -wasm-use-legacy-eh=false -Ilib -c $< -o build/test_kit.wasm.o
	@echo "lib/kit compiles for wasm32-wasip1"

clean:
	rm -rf build
