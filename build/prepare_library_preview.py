from pathlib import Path
text=Path('src/main.c').read_text()
text=text.replace('"Wolodrive",','"Wolodrive New Systems Preview",')
text=text.replace('    app->translations = translation_create();','''    app->active_row = VIEW_GAMES;
    app->library_depth = LIBRARY_CONSOLES;
    app->library_company = argc > 1 ? atoi(argv[1]) : 8;
    app->library_console_selection = argc > 2 ? atoi(argv[2]) : 0;
    app->library_sidebar_focus = false;
    app->translations = translation_create();''')
text=text.replace('        ui_render(app);','''        app->active_row = VIEW_GAMES;
        app->library_depth = LIBRARY_CONSOLES;
        app->library_company = argc > 1 ? atoi(argv[1]) : 8;
        app->library_console_selection = argc > 2 ? atoi(argv[2]) : 0;
        app->library_sidebar_focus = false;
        ui_render(app);''')
Path('build/library_preview_main.c').write_text(text)
