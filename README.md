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

[Thanks to Chuan Ji](https://jichu4n.com/posts/how-x-window-managers-work-and-how-to-write-one-part-i/)
