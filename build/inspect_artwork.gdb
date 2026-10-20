set pagination off
set debuginfod enabled off
symbol-file build/wolodrive-debug
python
frame = gdb.newest_frame()
while frame and frame.name() != 'main':
    frame = frame.older()
if frame:
    frame.select()
    app = gdb.parse_and_eval('app').dereference()
    row = app['rows'][app['active_row']]
    selected = int(row['selected_col'])
    print('Selected', selected, 'scroll', row['scroll_x'])
    cache = app['artwork'].dereference()
    print('Texture bytes', cache['bytes'])
    for i in range(64):
        e = cache['entries'][i]
        if e['key'].string():
            print(i, e['key'].string(), 'texture', bool(e['texture']), 'surface', bool(e['surface']), 'busy', bool(e['busy']), 'priority', e['priority'], 'used', e['used'], 'failed', bool(e['failed']), 'disk_checked', e['disk_checked'])
    for i in range(max(0, selected-2), min(int(row['item_count']), selected+8)):
        item = row['items'][i].dereference()
        print('NEARBY', i, item['title'].string(), item['cover_url'].string())
    import json
    with open('build/live_artwork.h', 'w', encoding='utf-8') as out:
        out.write('static const GameItem fixture_items[] = {\n')
        for i in range(int(row['item_count'])):
            item = row['items'][i].dereference()
            fields = ['id_code','title','cover_url','boxart_url','icon_url','landscape_url','backdrop_url']
            out.write('{' + ','.join('.'+key+'='+json.dumps(item[key].string(), ensure_ascii=False) for key in fields) + '},\n')
        out.write('};\n')
end
detach
