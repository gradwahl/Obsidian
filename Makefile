# ==============================================================================
# WOLODRIVE 2.0 - Dual-Target Makefile (Odporny na formatowanie schowka)
# ==============================================================================

PS5_IP          ?= 192.168.1.100
PS5_PORT        ?= 9021
PS5_FTP_PORT    ?= 1337
PS5_PAYLOAD_SDK ?= /opt/ps5-payload-sdk

.PHONY: all linux preview build-linux test test-native-snapshot benchmark-ui ps5 deploy deploy-ftp clean

all: build-linux

linux: build-linux
	@echo "[POLDRIVE] Uruchamianie Wolodrive 2.0 (Linux Mint x64 + Pegasus-DL)..."
	./build/wolodrive

preview: linux

src/native_catalog_snapshot.h: scripts/generate_native_snapshot.py src/catalogData.json config/additional_sources.json
	python3 scripts/generate_native_snapshot.py

build-linux: src/native_catalog_snapshot.h
	@mkdir -p build assets/covers
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 \
	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/wolodrive \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` \
	    -lm -lpthread
	@echo "[POLDRIVE] Zbudowano natywną binarkę Linux: build/wolodrive"

test-native-snapshot: src/native_catalog_snapshot.h
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_native_snapshot.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/test_native_snapshot \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	./build/test_native_snapshot

benchmark-ui: src/native_catalog_snapshot.h
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/benchmark_ui.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/benchmark_ui \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lm -lpthread
	SDL_VIDEODRIVER=dummy ./build/benchmark_ui

test: src/native_catalog_snapshot.h
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
	python3 -m unittest discover -s tests -p test_launchbox_artwork.py
	python3 -m unittest discover -s tests -p test_gametdb_artwork.py
	python3 -m unittest discover -s tests -p test_dlpsgame_artwork.py
	python3 -m unittest discover -s tests -p test_review_artwork.py
	python3 -m unittest discover -s tests -p "test_igdb*.py"
	python3 tests/run_tests.py

.PHONY: build-windows windows
build-windows:
	@mkdir -p build assets/covers
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c 	    -o build/wolodrive.exe 	     	    -lm -lpthread
	@echo "[POLDRIVE] Zbudowano natywna binarke Windows: build/wolodrive.exe"

windows: build-windows
	@echo "[POLDRIVE] Uruchamianie Wolodrive 2.0 (Windows)..."
	./build/wolodrive.exe

ps5: src/native_catalog_snapshot.h
	@mkdir -p build
	@if [ ! -d "${PS5_PAYLOAD_SDK}" ]; then \
	    echo "[BŁĄD] Nie znaleziono PS5_PAYLOAD_SDK w ${PS5_PAYLOAD_SDK}."; \
	    exit 1; \
	fi
	@"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --exists sdl2 SDL2_ttf SDL2_image libcurl
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -O2 -Wall -D__PS5__ \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags sdl2 SDL2_ttf SDL2_image libcurl` \
	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --libs --static sdl2 SDL2_ttf SDL2_image libcurl` \
	    -lm \
	    -o build/wolodrive.elf
	@echo "[POLDRIVE] Zbudowano payload PS5: build/wolodrive.elf"

# This additive target preserves the native PS5 build and the AI Studio output.
.PHONY: ps5-web ps5-shortcut test-web-payload
ps5-web:
	npm run build:static
	python3 scripts/embed_web_preview.py static-preview build/ps5_web_assets.h
	python3 scripts/generate_ps5_shortcut_icon.py build/wolodrive-icon0.png build/ps5_shortcut_icon.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -Dmain=wolodrive_install_shortcut -O2 -Wall -Wextra -Ibuild \
	    -c src/web_shortcut_installer.c -o build/wolodrive-shortcut.o
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -DWOLO_BUNDLED_SHORTCUT -O2 -Wall -Wextra \
	    src/web_browser_helper.c build/wolodrive-shortcut.o -lSceIpmi -lSceAppInstUtil \
	    -lSceSystemService -lSceUserService -o build/wolodrive-browser-helper.elf
	python3 scripts/embed_browser_helper.py build/wolodrive-browser-helper.elf build/ps5_browser_helper.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -O2 -Wall -Wextra \
	    -Ibuild src/web_payload.c -lpthread -o build/wolodrive-web.elf
	cp build/wolodrive-web.elf build/Poldrive.elf

ps5-shortcut: ps5-web

test-web-payload:
	python3 tests/test_web_payload.py
	python3 tests/test_browser_helper.py
	python3 tests/test_ps5_shortcut.py

deploy:
	@if [ ! -f "build/wolodrive.elf" ]; then \
	    make ps5; \
	fi
	@echo "[POLDRIVE] Wysyłanie build/wolodrive.elf do ${PS5_IP}:${PS5_PORT}..."
	nc -q 0 "${PS5_IP}" "${PS5_PORT}" < build/wolodrive.elf || \
	socat -u FILE:build/wolodrive.elf TCP:"${PS5_IP}":"${PS5_PORT}"
	@echo "[POLDRIVE] Payload wysłany pomyślnie!"

deploy-ftp:
	curl -T build/wolodrive.elf "ftp://${PS5_IP}:${PS5_FTP_PORT}/data/homebrew/wolodrive/wolodrive.elf" --ftp-create-dirs

clean:
	rm -rf build/*
