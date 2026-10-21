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
	@echo "[OBSIDIAN] Uruchamianie Wolodrive 2.0 (Linux Mint x64 + Pegasus-DL)..."
	./build/wolodrive

preview: linux

src/native_catalog_snapshot.h: scripts/generate_native_snapshot.py src/catalogData.json config/additional_sources.json
	python3 scripts/generate_native_snapshot.py

build-linux: src/native_catalog_snapshot.h
	@mkdir -p build assets/covers
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 \
	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/wolodrive \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` \
	    -lz -lm -lpthread
	@echo "[OBSIDIAN] Zbudowano natywną binarkę Linux: build/wolodrive"

test-native-snapshot: src/native_catalog_snapshot.h
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_native_snapshot.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/test_native_snapshot \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	./build/test_native_snapshot

benchmark-ui: src/native_catalog_snapshot.h
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/benchmark_ui.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/benchmark_ui \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	SDL_VIDEODRIVER=dummy ./build/benchmark_ui

test: src/native_catalog_snapshot.h test-files test-ps5-renderer test-settings
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_i18n.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    -o build/test_i18n \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_regressions.c src/screenshots.c src/artwork.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c \
	    -o build/test_regressions \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_translation.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c \
	    -o build/test_translation \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc tests/test_artwork.c -o build/test_artwork \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc tests/test_screenshots.c src/text.c -o build/test_screenshots \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
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
.PHONY: test-ps5-renderer
test-ps5-renderer:
	@mkdir -p build
	gcc -Wall -Wextra -Werror -O2 -Isrc tests/test_ps5_renderer.c -o build/test_ps5_renderer `pkg-config --cflags --libs sdl2`
	SDL_VIDEODRIVER=dummy ./build/test_ps5_renderer

.PHONY: test-files
test-files:
	python3 tests/test_file_manager.py
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_file_ui.c src/file_manager.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c \
	    -o build/test_file_ui \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	SDL_VIDEODRIVER=dummy ./build/test_file_ui

.PHONY: test-settings
test-settings:
	@mkdir -p build
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 -Isrc \
	    tests/test_settings_ui.c src/file_manager.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c \
	    -o build/test_settings_ui \
	    `pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image libcurl` -lz -lm -lpthread
	SDL_VIDEODRIVER=dummy ./build/test_settings_ui

build-windows:
	@mkdir -p build assets/covers
	gcc -D_GNU_SOURCE -Wall -Wextra -O2 	    src/main.c src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c 	    -o build/wolodrive.exe 	     	    -lz -lm -lpthread
	@echo "[OBSIDIAN] Zbudowano natywna binarke Windows: build/wolodrive.exe"

windows: build-windows
	@echo "[OBSIDIAN] Uruchamianie Wolodrive 2.0 (Windows)..."
	./build/wolodrive.exe

# One distributable: quiet web service + direct Media registration + autoload.
# Embedded ELF intermediates stay in build/.internal; copy only build/obsidian.elf.
# Native PS5 rendering remains available through make ps5-native.
.PHONY: ps5-native
ps5: ps5-web-assets
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FILE_BRIDGE \
	    -DWOLO_SINGLE_SERVICE -DWOLO_WEB_NO_BROWSER -DWOLO_WEB_AUTOSTART \
	    -DWOLO_WEB_FIXED_PORT -O2 -Wall -Wextra \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags zlib` -Ibuild \
	    src/web_payload.c src/file_manager.c -lz -lpthread -o build/.internal/obsidian-bundled-service.elf
	python3 scripts/embed_ps5_service.py build/.internal/obsidian-bundled-service.elf build/ps5_web_assets.h build/ps5_service_payload.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_SINGLE_SHORTCUT -O2 -Wall -Wextra -Ibuild \
	    src/web_shortcut_installer.c -lSceIpmi -lSceAppInstUtil -o build/.internal/obsidian-shortcut-helper.elf
	python3 scripts/embed_shortcut_helper.py build/.internal/obsidian-shortcut-helper.elf build/ps5_shortcut_helper.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -Dmain=obsidian_install_shortcut -O2 -Wall -Wextra -Ibuild \
	    -c src/ps5_unified_launcher.c -o build/obsidian-shortcut-launcher.o
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -O2 -Wall -Wextra -Ibuild \
	    src/ps5_single_installer.c build/obsidian-shortcut-launcher.o -o build/obsidian.elf
	@echo "[OBSIDIAN] Copy only build/obsidian.elf; run once to install/start quietly, then open Obsidian under Media."

# Experimental full web-app metadata. Separate artifacts preserve the working
# obsidian.elf. The v2 system scaffold requires the dedicated restore target.
.PHONY: ps5-fullscreen-test ps5-fullscreen-restore
ps5-fullscreen-test: ps5-web-assets ps5-fullscreen-restore
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FILE_BRIDGE \
	    -DWOLO_SINGLE_SERVICE -DWOLO_WEB_NO_BROWSER -DWOLO_WEB_AUTOSTART \
	    -DWOLO_WEB_FIXED_PORT -O2 -Wall -Wextra \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags zlib` -Ibuild \
	    src/web_payload.c src/file_manager.c -lz -lpthread -o build/.internal/obsidian-fullscreen-service.elf
	python3 scripts/embed_ps5_service.py build/.internal/obsidian-fullscreen-service.elf build/ps5_web_assets.h build/ps5_fullscreen_service_payload.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_SINGLE_SHORTCUT -DWOLO_FULLSCREEN_WEBAPP -O2 -Wall -Wextra -Ibuild \
	    src/web_shortcut_installer.c -lSceIpmi -lSceAppInstUtil -o build/.internal/obsidian-fullscreen-helper.elf
	python3 scripts/embed_shortcut_helper.py build/.internal/obsidian-fullscreen-helper.elf build/ps5_fullscreen_shortcut_helper.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_FULLSCREEN_WEBAPP -Dmain=obsidian_install_shortcut -O2 -Wall -Wextra -Ibuild \
	    -c src/ps5_unified_launcher.c -o build/obsidian-fullscreen-launcher.o
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FULLSCREEN_WEBAPP -O2 -Wall -Wextra -Ibuild \
	    src/ps5_single_installer.c build/obsidian-fullscreen-launcher.o -o build/obsidian-fullscreen-v2.elf
	@echo "[OBSIDIAN] Experimental v2: copy only build/obsidian-fullscreen-v2.elf. Keep the restore ELF for rollback."

# Restore helper removes only its owned system scaffold, then re-registers the
# confirmed browser-card shortcut. It uses the already-installed quiet service.
ps5-fullscreen-restore:
	@mkdir -p build/.internal
	python3 scripts/generate_ps5_shortcut_icon.py build/wolodrive-icon0.png build/ps5_shortcut_icon.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_SINGLE_SHORTCUT -DWOLO_FULLSCREEN_RESTORE -O2 -Wall -Wextra -Ibuild \
	    src/web_shortcut_installer.c -lSceIpmi -lSceAppInstUtil -o build/.internal/obsidian-fullscreen-restore-helper.elf
	python3 scripts/embed_shortcut_helper.py build/.internal/obsidian-fullscreen-restore-helper.elf build/ps5_fullscreen_restore_helper.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_FULLSCREEN_RESTORE -O2 -Wall -Wextra -Ibuild \
	    src/ps5_unified_launcher.c -o build/obsidian-fullscreen-restore.elf

ps5-native: src/native_catalog_snapshot.h ps5-web-assets
	@mkdir -p build
	@if [ ! -d "${PS5_PAYLOAD_SDK}" ]; then \
	    echo "[BŁĄD] Nie znaleziono PS5_PAYLOAD_SDK w ${PS5_PAYLOAD_SDK}."; \
	    exit 1; \
	fi
	@"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --exists sdl2 SDL2_ttf SDL2_image libcurl
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -O2 -Wall -D__PS5__ -Dmain=wolodrive_native_main \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags sdl2 SDL2_ttf SDL2_image libcurl` \
	    -c src/main.c -o build/obsidian-native-main.o
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FILE_BRIDGE -DWOLO_NATIVE_FALLBACK -Dmain=wolodrive_web_main -O2 -Wall -Wextra \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags zlib` -Ibuild -c src/web_payload.c -o build/obsidian-web-fallback.o
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -O2 -Wall -Wextra -D__PS5__ -DOBSIDIAN_APP_ENTRY -Ibuild \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags sdl2 SDL2_ttf SDL2_image libcurl` \
	    src/ps5_unified_launcher.c build/obsidian-native-main.o build/obsidian-web-fallback.o \
	    src/i18n.c src/library.c src/text.c src/settings.c src/translation.c src/sys_bridge.c src/file_manager.c src/emulator_download.c src/screenshots.c src/artwork.c src/catalog_pegasus.c src/ui_netflix.c \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --libs --static sdl2 SDL2_ttf SDL2_image libcurl` \
	    -lz -lm -lpthread \
	    -o build/.internal/obsidian-app.elf
	python3 scripts/embed_shortcut_helper.py build/.internal/obsidian-app.elf build/ps5_app_payload.h app_payload_elf
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_EMBED_APP -O2 -Wall -Wextra -Ibuild \
	    src/web_shortcut_installer.c -lSceIpmi -lSceAppInstUtil -o build/.internal/obsidian-shortcut-helper.elf
	python3 scripts/embed_shortcut_helper.py build/.internal/obsidian-shortcut-helper.elf build/ps5_shortcut_helper.h
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -O2 -Wall -Wextra -Ibuild \
	    src/ps5_unified_launcher.c -o build/obsidian-native-installer.elf
	@echo "[OBSIDIAN] Native-first recovery installer: build/obsidian-native-installer.elf (installer only; embedded dashboard app with web startup fallback)"

# This additive target preserves the native PS5 build and the AI Studio output.
.PHONY: ps5-web ps5-web-assets ps5-shortcut test-web-payload
ps5-web-assets:
	@mkdir -p build/.internal
	npm run build:static
	python3 scripts/embed_web_preview.py static-preview build/ps5_web_assets.h
	python3 scripts/generate_ps5_shortcut_icon.py build/wolodrive-icon0.png build/ps5_shortcut_icon.h
	# Browser opener must never silently re-register the old Payload Manager
	# shortcut and undo a user's deliberately installed direct Web-first tile.
	# Shortcut registration is handled by standalone/unified installers.
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -O2 -Wall -Wextra \
	    src/web_browser_helper.c -lSceSystemService -lSceUserService \
	    -o build/.internal/wolodrive-browser-helper.elf
	python3 scripts/embed_browser_helper.py build/.internal/wolodrive-browser-helper.elf build/ps5_browser_helper.h

# Browser-only diagnostic build cannot overwrite the unified distributable.
ps5-web: ps5-web-assets
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FILE_BRIDGE -O2 -Wall -Wextra \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags zlib` -Ibuild src/web_payload.c src/file_manager.c -lz -lpthread -o build/wolodrive-web.elf

# Web-first opt-in: launch the service from Payload Manager's autoload list,
# then install the direct localhost Media tile separately. The existing native
# ELF, original shortcut and diagnostic targets remain as recovery options.
.PHONY: ps5-webfirst ps5-web-service ps5-webfirst-shortcut ps5-legacy-shortcut
ps5-webfirst: ps5-web-service ps5-webfirst-shortcut ps5-legacy-shortcut
	@echo "[OBSIDIAN] Web-first: autoload build/obsidian-web-service.elf, then run build/obsidian-webfirst-shortcut.elf once."

ps5-web-service: ps5-web-assets
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D_GNU_SOURCE -D__PS5__ -DWOLO_FILE_BRIDGE \
	    -DWOLO_WEB_NO_BROWSER -DWOLO_WEB_AUTOSTART -DWOLO_WEB_FIXED_PORT \
	    -O2 -Wall -Wextra \
	    `"${PS5_PAYLOAD_SDK}/bin/prospero-pkg-config" --cflags zlib` -Ibuild \
	    src/web_payload.c src/file_manager.c -lz -lpthread -o build/obsidian-web-service.elf

ps5-webfirst-shortcut: ps5-web-assets
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ -DWOLO_SHORTCUT_WEBFIRST \
	    -O2 -Wall -Wextra -Ibuild src/web_shortcut_installer.c \
	    -lSceIpmi -lSceAppInstUtil -o build/obsidian-webfirst-shortcut.elf

# Explicit rollback installer: restores Payload Manager /loadpayload: route.
ps5-legacy-shortcut: ps5-web-assets
	"${PS5_PAYLOAD_SDK}/bin/prospero-clang" -D__PS5__ \
	    -O2 -Wall -Wextra -Ibuild src/web_shortcut_installer.c \
	    -lSceIpmi -lSceAppInstUtil -o build/obsidian-legacy-shortcut.elf

ps5-shortcut: ps5

.PHONY: test-startup-splash
test-startup-splash:
	gcc -Wall -Wextra -Werror -O2 -Isrc tests/test_startup_splash.c -o build/test_startup_splash `pkg-config --cflags --libs sdl2`
	SDL_VIDEODRIVER=dummy ./build/test_startup_splash

test-web-payload:
	python3 tests/test_ps5_webapp_mount.py
	python3 tests/test_single_installer.py
	python3 tests/test_file_bridge.py
	python3 tests/test_web_payload.py
	python3 tests/test_browser_helper.py
	python3 tests/test_ps5_shortcut.py
	python3 tests/test_unified_launcher.py

deploy:
	@if [ ! -f "build/obsidian.elf" ]; then \
	    make ps5; \
	fi
	@echo "[OBSIDIAN] Wysyłanie build/obsidian.elf do ${PS5_IP}:${PS5_PORT}..."
	nc -q 0 "${PS5_IP}" "${PS5_PORT}" < build/obsidian.elf || \
	socat -u FILE:build/obsidian.elf TCP:"${PS5_IP}":"${PS5_PORT}"
	@echo "[OBSIDIAN] Payload wysłany pomyślnie!"

deploy-ftp:
	curl -T build/obsidian.elf "ftp://${PS5_IP}:${PS5_FTP_PORT}/data/pldmgr/payloads/obsidian/obsidian.elf" --ftp-create-dirs

clean:
	rm -rf build/*

# Shared glass renderer proof; does not require a display, network or PS5 SDK.
.PHONY: test-glass
test: test-glass
test-glass:
	@mkdir -p build
	gcc -Wall -Wextra -Werror -O2 -Isrc tests/test_glass_ui.c -o build/test_glass_ui `pkg-config --cflags --libs sdl2` -lm
	SDL_VIDEODRIVER=dummy ./build/test_glass_ui
