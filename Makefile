# ==============================================================================
# WOLODRIVE 2.0 - Dual-Target Makefile (Odporny na formatowanie schowka)
# ==============================================================================

PS5_IP          ?= 192.168.1.100
PS5_PORT        ?= 9021
PS5_FTP_PORT    ?= 1337
PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk

.PHONY: all linux preview build-linux test benchmark-ui ps5 deploy deploy-ftp clean

all: build-linux

linux: build-linux
	@echo "[WOLODRIVE] Uruchamianie Wolodrive 2.0 (Linux Mint x64 + Pegasus-DL)..."
	./build/wolodrive

preview: linux

build-linux:
	@mkdir -p build assets/covers
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 \
	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/wolodrive \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` \
	    -lm -lpthread
	@echo "[WOLODRIVE] Zbudowano natywną binarkę Linux: build/wolodrive"

benchmark-ui:
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/benchmark_ui.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/benchmark_ui \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	SDL_VIDEODRIVER=dummy ./build/benchmark_ui

test:
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_i18n.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/test_i18n \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_regressions.c src/screenshots.c src/artwork.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c \
	    -o build/test_regressions \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_translation.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c \
	    -o build/test_translation \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc tests/test_artwork.c -o build/test_artwork \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc tests/test_screenshots.c src/text.c -o build/test_screenshots \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	python3 -m unittest discover -s tests -p test_download_artwork.py
	python3 -m unittest discover -s tests -p test_import_sources.py
	python3 -m unittest discover -s tests -p test_full_sources.py
	python3 -m unittest discover -s tests -p test_enrich_artwork.py
	python3 tests/run_tests.py

ps5:
	@mkdir -p build
	@if [ ! -d "${PS5_PAYLOAD_SDK}" ]; then \
	    echo "[BŁĄD] Nie znaleziono PS5_PAYLOAD_SDK w ${PS5_PAYLOAD_SDK}."; \
	    exit 1; \
	fi
	clang --target=x86_64-scei-ps5 -D_GNU_SOURCE -O2 -Wall -D__PS5__ \
	    -I"${PS5_PAYLOAD_SDK}/include" -I"${PS5_PAYLOAD_SDK}/include/SDL2" \
	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -L"${PS5_PAYLOAD_SDK}/lib" \
	    -lSDL2 -lSDL2_ttf -lSDL2_image -lfreetype -lpng -ljpeg -lwebp -lcurl -lssl -lcrypto -lz -lpthread -lkernel -lm \
	    -o build/wolodrive.elf
	@echo "[WOLODRIVE] Zbudowano payload PS5: build/wolodrive.elf"

deploy:
	@if [ ! -f "build/wolodrive.elf" ]; then \
	    make ps5; \
	fi
	@echo "[WOLODRIVE] Wysyłanie build/wolodrive.elf do ${PS5_IP}:${PS5_PORT}..."
	nc -q 0 "${PS5_IP}" "${PS5_PORT}" < build/wolodrive.elf || \
	socat -u FILE:build/wolodrive.elf TCP:"${PS5_IP}":"${PS5_PORT}"
	@echo "[WOLODRIVE] Payload wysłany pomyślnie!"

deploy-ftp:
	curl -T build/wolodrive.elf "ftp://${PS5_IP}:${PS5_FTP_PORT}/data/homebrew/wolodrive/wolodrive.elf" --ftp-create-dirs

clean:
	rm -rf build/*
