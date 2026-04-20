rm -rf ./build

export Nuke_ROOT=/opt/Nuke16.0v2 # Replace with your Nuke installation path

cmake -S . -B ./build -Wno-dev -DNuke_ROOT=$Nuke_ROOT -DOPTIX=OFF -DOIDN_CPU=ON -DOIDN_CUDA=OFF -DOIDN_SYCL=OFF -DOIDN_METAL=OFF -DOIDN_HIP=OFF

cmake --build ./build --target install

cp -r $HOME/dev/CGDenoiser/plugins/* $HOME/.nuke