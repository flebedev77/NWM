# NWM
## Nachal Window Manager
Simple hobby window manager.

Download dependencies:
`sudo pacman -S base-devel libx11 \
    xorg-server-xephyr xorg-xinit`

Build and run:
`./build_and_run.sh`

or
Use make to build and put the path to nwm into your .xinitrc:
`make`

xinitrc:
`/home/user/path/to/nwm/build/nwm`
