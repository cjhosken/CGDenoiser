rm -rf ./build

export Nuke_ROOT=/home/hoske/software/Foundry/Nuke17.0v1 # Replace with your Nuke installation path

cmake -S . -B ./build -Wno-dev -DUSE_OPTIX=OFF

cmake --build ./build --target install

cp -r $HOME/dev/CGDenoiser/plugins/* $HOME/.nuke