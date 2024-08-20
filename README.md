# Pi Pico Camera

This expands on the waveshare demo code for the camera to utilize teh buttons.

- Button 1 (K1) Changes the pallet of the image to full grayscale (default), black and white (2 color) or grayscale (4 color)
- Button 2 (K2) applies a color mask to the image. This cycles between grayscale, red, pink, green, blue, and yellow

Written as quick-and-dirty solution so the code is of expedience not quality. Buttons/debouncing is done on iteration cycles instead of interrupts/timers.

Any code I added is in `main.h` and `main.c`. The rest came with the demo code

- Product https://www.waveshare.com/pico-cam-a.htm
- Wiki: https://www.waveshare.com/wiki/PICO-Cam-A

### Build

Prereqs (Apple Silicon Mac)

```bash
brew install --cask gcc-arm-embedded
brew install cmake
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
export PICO_SDK_PATH=~/pico-sdk
```

```bash
chmod +x build.sh
./build.sh
```

Todo/ideas:

- Add workflow to build the uf2 file
- Save images
- more modes and color masks
- better button handling
