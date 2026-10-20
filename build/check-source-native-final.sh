set -eu
cd /mnt/c/Users/Callum/Documents/GitHub/wolodrive
make build-linux
gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
  tests/test_regressions.c src/screenshots.c src/artwork.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c \
  -o build/test_sources_regressions \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl) -lm -lpthread
task_data=$(mktemp -d /tmp/wolodrive-sources.XXXXXX)
SDL_VIDEODRIVER=dummy XDG_DATA_HOME="$task_data" ./build/test_sources_regressions
