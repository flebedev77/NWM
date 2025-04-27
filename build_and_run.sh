set -e

make
XEPHYR=$(whereis -b Xephyr | sed -E 's/^.*: ?//')
if [ -z "$XEPHYR" ]; then
    echo "Xephyr not found, exiting"
    exit 1
fi
xinit ./xinitrc -- \
    "$XEPHYR" \
        :$RANDOM \
        -ac \
        -screen 800x600 \
        -host-cursor
