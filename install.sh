rm -rf ./build

export Nuke_ROOT=/home/hoske/software/Foundry/Nuke17.0v1 # Replace with your Nuke installation path

cmake -S . -B ./build -Wno-dev -DOPTIX=ON -DOIDN_CPU=ON -DOIDN_CUDA=ON -DOIDN_SYCL=OFF -DOIDN_METAL=OFF -DOIDN_HIP=OFF

cmake --build ./build --target install

cp -r $HOME/dev/CGDenoiser/plugins/* $HOME/.nuke