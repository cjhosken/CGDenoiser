rm -rf ./build

export Nuke_ROOT=/home/hoske/software/Foundry/Nuke15.2v4 # Replace with your Nuke installation path

cmake -S . -B ./build -Wno-dev

cmake --build ./build --target install