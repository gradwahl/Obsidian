# Pigułki i sidebar — 21 października 2026

Zmiana dotyczy dwóch rendererów: React/CSS i C/SDL2. Zastany checkout zawierał wiele innych, niezacommitowanych zmian. Nie były resetowane. Nie jest to potwierdzenie pełnego parytetu PS5/web.

## A — obcinanie

Webowy Home i główny kontener dziedziczą wysokość sceny dopasowanej do okna, zamiast kończyć się na 1020 px. Kategorie są w flexboxie z `margin-top:auto`, `overflow:visible` i dolnym odstępem 48 jednostek sceny: 48 px przy 1920×1080, 32 px przy 1280×720. Cień ma zapas poza kapsułą. Zachowano skalowanie logicznej sceny 1920×1080, wspólne z SDL.

SDL wylicza pozycję dolnych kategorii z `SDL_RenderGetLogicalSize`, z fallbackiem do rozmiaru wyjścia renderera. Rysowanie i obsługa wskaźnika myszy korzystają z tej samej wysokości. Kategorie rysowane są po wyłączeniu klipu karuzeli. Footer nie przykrywa już kategorii.

## B — materiał

`src/glass.css` zawiera wspólne `.glass-pill`, `--sm`, `--md`, `--active`. Bazowe wartości odczytano z istniejącego CSS: `rgba(28,28,38,.72)`, `rgba(22,22,30,.78)`, aktywne `rgba(229,9,20,.35)`, ramka `rgba(255,255,255,.15)`, tekst `#f5f5f5`. Zachowano osobne istniejące kolory tekstu metadanych i kategorii oraz czerwone końce gradientu głównej akcji `#ff1e27`/`#c80812`. Białe/czarne warstwy optyczne tworzą obrys, gloss i głębię. Nie zmieniano palety na podstawie obrazka.

Pigułki mają promień 9999 px, metadane wysokość 30, kategorie 40. Szkło używa blur 16 px / saturate 150%, maskowanego obrysu, cieni wewnętrznych i zewnętrznych. Fallback bez masek zachowuje zwykłą ramkę. Nie dodano bibliotek ani wymagań serwera w przeglądarce.

`src/ui_glass.h` zawiera wspólne `drawGlassPill`: gradientowe wachlarze SDL_RenderGeometry (16 segmentów na narożnik), tekstury target 2×, cache ograniczony do 12 pozycji, cienie i gloss generowane przy tworzeniu tekstury, dziewięcioczęściowe kopie SDL_RenderCopyF zachowujące końce przy rozciąganiu, blend i filtrowanie linear. Cache jest zwalniany z rendererem. Blur tła jest symulowany przez przezroczystość, zgodnie ze specyfikacją SDL.

## C — sidebar

Jedna szklana kapsuła, stały promień 34, padding 8, przezroczyste pozycje 44 z przerwami 4. Webowe ikony mają 20 px, tekst stałą kolumnę; Ustawienia są przy dole z zapasem na halo. W obu rendererach rail ma 92/272 jednostki szerokości, a rozszerzanie nie przesuwa treści. Poszerzenie trwa 240 ms, cubic-bezier(.4,0,.2,1). Nieaktywne pozycje nie mają osobnych powierzchni szkła. Webowe tooltipy nawigacji mają wspólny materiał.

## F — kontrakt rzeczywistego logo

Przed zmianami React zawierał osobne `P` i `olDrive`; czerwone P nie poruszało się, a reszta pojawiała się przez opacity 200 ms. Wyzwalacze: hover, fokus sidebara lub otwarte języki. SDL rysował P oraz `oldrive`, z alfą zależną od postępu rozsuwania. W obu implementacjach motyw Ubiquity ukrywał logo w sidebarze. Nie znaleziono animacji rozsuwania P i D.

Zachowano strukturę, klasy, kierunek, wyzwalacze oraz ukrycie w Ubiquity. Web: 480 ms, cubic-bezier(.22,1,.36,1) przy odkrywaniu i cubic-bezier(.4,0,.2,1) przy ukrywaniu, blur 3→0, overflow visible, reduced-motion opacity 150 ms. Nie dodano nowej animacji P/D ani nowej widoczności logo. SDL nadal wiąże alfę napisu z rozszerzaniem sidebara; nie ma osobnej animacji liter i nie jest to implementacja P/D z załączonego opisu. Widocznego logo w sidebarze Ubiquity nie można zatwierdzić z tych ekranów, bo pozostaje ukryte.

Pliki F: `src/glass.css`; czas natywnego rozszerzania: `src/ui_netflix.c`, `src/ui_glass.h`.

## G — wspólne zaznaczenie

React używa jednego komponentu SlidingHighlight w sidebarze, kategoriach i językach. Pozycje mierzone są względem listy w jednostkach sceny, z obsługą ResizeObserver, fontów, pierwszego pokazania i resize. Przed anulowaniem lotu odczytywany jest aktualny prostokąt. Pierwsze pokazanie i zmiana rozmiaru okna ustawiają pozycję bez lotu; zwinięty sidebar nie animuje zmiany wyboru.

Web: 280–520 ms (`280 + dystans×0.25`), cubic-bezier(.22,1,.36,1), unia prostokątów w 45% lotu; osobna warstwa halo schodzi do 70% i wraca. Reduced-motion: opacity 150 ms z niewidoczną zmianą położenia. Otwieranie języków 200 ms, zamykanie 140 ms; odstęp wejścia pozycji 20 ms. Lista zamyka się 120 ms po planowanym końcu lotu. Etykiety sidebara i kategorii znikają przez 90 ms, zmieniają język na minimum opacity i wracają przez 140 ms. To obecnie wspólny crossfade 230 ms; osobna pełna sekwencja etykiety języka z ruchem ±4 px/220 ms nie została wdrożona.

SDL: jedna struktura HighlightAnim i funkcje updateHighlight/drawHighlight, rzeczywisty zegar SDL, zakres 280–520 ms, czoło easeOutCubic, tył easeOutQuart z opóźnieniem 40 ms. Nowy cel przejmuje aktualne krawędzie. Po wyborze języka zamknięcie jest odroczone o lot +120 ms. Native nie odtwarza jeszcze webowego staggera, scaleY dropdownu ani crossfade tłumaczeń; jest to pozostała różnica animacji G3. Brak potwierdzenia ustawień reduced-motion na PS5.

Pliki G: `src/components/SlidingHighlight.tsx`, `src/components/Sidebar.tsx`, `src/components/UbiquityHomeView.tsx`, `src/useLanguagePresentation.ts`, `src/glass.css`, `src/ui_glass.h`, `src/ui_netflix.c`.

## Weryfikacja

- PASS: npm run lint.
- PASS: npm run test:web (pełna sekwencja, uruchamiana osobno od buildu; oba polecenia generują public/poldrive-pages i nie mogą działać równolegle).
- PASS: npm run build:static; wygenerowany static-preview jest lokalnie aktualny. Ten katalog był już ignorowany przez .gitignore, czego nie zmieniano.
- PASS: python tests/check_static_preview.py — HTML/JS/CSS, 85 690 rekordów, 472 strony JSON i 16 plików artwork przez zwykły HTTP, bez Node w runtime.
- Browser HTTP: oglądane 1280×720 i 1920×1080, stan zwinięty i rozwinięty; zmiana języka, sąsiednie/dalekie kategorie, seria szybkich kliknięć, resize, Home, Search (zapytanie Spider), szczegóły, zamknięcie dialogu, biblioteka i Settings. Sprawdzono sterowanie klawiaturą Left/Right. Konsola sprawdzanego buildu: brak wpisów error. Po resize oceniano ustalony obraz, a nie pierwszą klatkę aktualizacji rozmiaru viewportu.
- PASS: make build-linux, make test-native-snapshot, make test-glass (WSL Ubuntu 24.04). Test szkła sprawdza renderowanie cache, zachowanie klipu, początek bez lotu, przerwanie i snap.
- make test: zatrzymuje się na wcześniejszych niespójnościach tests/test_regressions.c: DESCRIPTION_BUTTON, load_library_artwork, library_alpha_bounds, prepare_library_logo, load_additional_catalog. Testy Files, Settings i wyboru renderera przeszły przed błędem kompilacji. Nie jest to pełny zielony make test.
- Native Linux: wygenerowane cztery PNG 1280×720 / 1920×1080, collapsed/expanded, przez tests/capture_glass_ui.c. Porównanie potwierdza widoczny dolny rząd i wspólną kapsułę; pozostają różnice fontów, ikon, składu tytułu, ujęcia tła, metadanych i karuzeli. Zastany native pokazuje cztery pola metadanych, web do siedmiu; pól i tekstów nie usuwano w celu ukrycia tej różnicy. Web doładowuje katalog 85 690 rekordów, native używa snapshotu 862. Obrazy wymagające sieci nie zostały pobrane w offline fixture.
- PS5: nie testowano na urządzeniu ani SDK; Linux nie jest dowodem zachowania PS5.
- AI Studio deployment not verified. Nie ma bezpośredniego dostępu do wdrożenia AI Studio. Wynik dotyczy lokalnego statycznego HTTP.

Do zatwierdzenia pełnego parytetu trzeba porównać te same dane i grę na PS5, zaakceptować rozwiązanie różnicy istniejącego logo P/P+D, dopracować pozostałe G3 na obu platformach oraz zweryfikować statyczny katalog w AI Studio. Nie oznaczono zmiany jako parity-complete.

## Pliki zmiany

- A/B/C: src/native.css, src/glass.css, src/main.tsx, src/components/HeroBanner.tsx, src/components/Sidebar.tsx, src/components/UbiquityHomeView.tsx, src/components/UbiquityGameDetails.tsx, src/ui_netflix.c, src/ui_glass.h.
- F i G: zestawy podane osobno powyżej.
- Weryfikacja: tests/test_react_layout.cjs, tests/test_glass_ui.c, tests/capture_glass_ui.c, Makefile, niniejszy raport.
- Wygenerowane: static-preview/index.html, static-preview/assets/index-*.js/css, pozostałe zasoby i strony katalogu; build/glass-native-*.png.

Statyczne sprawdzenie wdrożenia: uruchomić `python -m http.server 3000 --directory static-preview`, otworzyć URL root, zweryfikować start, Home, Search, sidebar, kategorie, szczegóły i Settings w obu rozdzielczościach. Następnie wgrać dokładnie zawartość static-preview do AI Studio i powtórzyć te same kroki, sprawdzając konsolę i brak wymagania Node/kluczy/połączenia z PS5.
