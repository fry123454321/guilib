dir_debug=./build_debug
dir_release=./build_release

generate_release:
	conan install ./conan --output-folder=${dir_release}/.conan --build=missing --settings=build_type=Release
	cmake --fresh --preset Release -G Ninja
	cmake --build --preset Release

generate_debug:
	conan install ./conan --output-folder=${dir_debug}/.conan --build=missing --settings=build_type=Debug
	cmake --fresh --preset Debug -G Ninja
	cmake --build --preset Debug

run_debug:
	cd ${dir_debug}/bin && LIBGUI_ROOT_DIR=$(CURDIR) ./app

submodule:
	git submodule update --init --recursive
