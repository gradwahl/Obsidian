"""Metadata extraction and in-place snapshot enrichment; no network required."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from import_sources import axekin_labels, axekin_label_version, HostedFilePage, axekin_file_page_formats
from refresh_axekin_labels import apply_labels


class AxekinLabelTests(unittest.TestCase):
    def test_hosted_filename_beats_unrelated_zip_advertisement(self):
        parser = HostedFilePage()
        parser.feed('<meta property="og:title" content="NHL 26 [PPSA26785](axekin.ffpfs">'
                    '<h1>NHL 26 [PPSA26785](axekin.ffpfs</h1>'
                    '<h2>your-big-file.zip</h2><p>Download PKG</p>')
        self.assertEqual(parser.formats(), {'FFPFS'})
        self.assertNotIn('FFPKG', parser.formats())
        parser = HostedFilePage()
        parser.feed('<title>Download Game ffpfs</title><h2>Downloading Game.ffpfs</h2>')
        self.assertEqual(parser.formats(), {'FFPFS'})
        parser = HostedFilePage()
        parser.feed('<meta property="og:title" content="File unavailable"><h2>advert.zip</h2>')
        self.assertEqual(parser.formats(), set())

    def test_public_host_lookup_uses_html_only(self):
        from email.message import Message
        from unittest.mock import patch, MagicMock
        response = MagicMock()
        response.__enter__.return_value = response
        response.headers = Message()
        response.headers['Content-Type'] = 'text/html; charset=utf-8'
        response.read.return_value = b'<meta property="og:title" content="NHL_26.ffpfs">'
        opener = MagicMock()
        opener.open.return_value = response
        with patch('import_sources.urllib.request.build_opener', return_value=opener):
            self.assertEqual(axekin_file_page_formats('https://akirabox.com/example/file'), {'FFPFS'})
            response.headers.replace_header('Content-Type', 'application/octet-stream')
            response.read.reset_mock()
            self.assertEqual(axekin_file_page_formats('https://akirabox.com/example/file'), set())
            response.read.assert_not_called()
            opener.open.reset_mock()
            self.assertEqual(axekin_file_page_formats('https://other.example/game/file'), set())
            opener.open.assert_not_called()

    def test_host_resolution_uses_base_release_and_discards_links(self):
        from unittest.mock import patch
        game = {'slug': 'nhl-26', 'version': '01.060', 'downloadLinks': [
            {'type': 'base', 'url': 'https://akirabox.com/example/file'},
            {'type': 'update', 'url': 'https://akirabox.com/update/file'},
        ]}
        with patch('import_sources.axekin_file_page_formats', return_value={'FFPFS'}) as lookup:
            labels = axekin_labels(game, resolve_files=True)
        lookup.assert_called_once_with('https://akirabox.com/example/file')
        self.assertEqual(axekin_label_version(labels), '01.060 / FFPFS')
        self.assertNotIn('akirabox.com', json.dumps(labels))

    def test_version_only_refresh_retains_same_version_hosted_format(self):
        doc = {'sources': [{'id': 'axekin', 'packages': [
            {'sourceUrl': 'https://www.axekin.com/games/nhl-26', 'version': '01.060 / FFPFS'},
        ]}]}
        self.assertEqual(apply_labels(doc, [{'slug': 'nhl-26', 'version': '01.060'}]), 0)
        self.assertEqual(doc['sources'][0]['packages'][0]['version'], '01.060 / FFPFS')
        self.assertEqual(axekin_label_version({'version': '01.060', 'formats': ['RAR']}, '01.060 / FFPFS'), '01.060 / FFPFS')
        self.assertEqual(axekin_label_version({'version': '01.061'}, '01.060 / FFPFS'), '01.061')

    def test_explicit_filename_metadata_without_retaining_links(self):
        labels = axekin_labels({
            'slug': 'example', 'version': ' 01.04.03 ',
            'downloadLinks': [
                {'url': 'https://files.example/game.FPKG.part01.rar', 'type': 'base'},
                {'url': 'https://files.example/game.LZ4.7z', 'type': 'base'},
                {'url': 'https://files.example/opaque-id?format=iso'},
                {'url': 'javascript:pkg'}, None,
            ],
        })
        self.assertEqual(labels, {'slug': 'example', 'version': '01.04.03',
                                  'formats': ['FPKG', 'LZ4']})
        self.assertEqual(axekin_label_version(labels), '01.04.03 / FPKG+LZ4')
        self.assertNotIn('files.example', json.dumps(labels))

    def test_unknown_formats_are_not_guessed_from_platform_or_host(self):
        for links in (None, {}, [{'url': 'https://pkg.example/opaque-id'}]):
            labels = axekin_labels({'slug': 'game', 'version': '1.0.1a',
                                    'platform': 'sony-ps5', 'downloadLinks': links})
            self.assertEqual(axekin_label_version(labels), '1.0.1a')
        self.assertEqual(axekin_labels({'version': '<b>fake</b>'})['version'], '')

    def test_enrichment_preserves_identity_order_and_other_metadata(self):
        doc = {'sources': [{'id': 'axekin', 'packages': [
            {'id': 'first', 'title': 'Game', 'sizeBytes': 100, 'format': 'catalog',
             'sourceUrl': 'https://www.axekin.com/games/game'},
            {'id': 'second', 'title': 'Unavailable', 'version': 'old',
             'sourceUrl': 'https://www.axekin.com/games/gone'},
        ]}]}
        before = copy.deepcopy(doc['sources'][0]['packages'])
        self.assertEqual(apply_labels(doc, [{'slug': 'game', 'version': '1.2', 'formats': ['NSP']}]), 1)
        self.assertEqual(doc['sources'][0]['packages'][0].pop('version'), '1.2 / NSP')
        self.assertEqual(doc['sources'][0]['packages'], before)

    def test_conflicting_live_rows_cannot_partially_modify_snapshot(self):
        doc = {'sources': [{'id': 'axekin', 'packages': []}]}
        before = copy.deepcopy(doc)
        with self.assertRaises(ValueError):
            apply_labels(doc, [{'slug': 'game', 'version': '1'}, {'slug': 'game', 'version': '2'}])
        self.assertEqual(doc, before)

    def test_bundled_spider_man_version_and_complete_matching(self):
        source = json.loads((ROOT / 'assets/catalogs/axekin.json').read_text(encoding='utf8'))['sources'][0]
        game = next(row for row in source['packages'] if row['sourceUrl'].endswith('/marvels-spider-man-2'))
        self.assertEqual(game['version'], '01.04.03 / FFPFS')
        nhl = next(row for row in source['packages'] if row['sourceUrl'].endswith('/nhl-26'))
        self.assertEqual(nhl['version'], '01.060 / FFPFS')
        self.assertGreater(source['labelCoverage']['entriesWithLabels'], 20000)
        self.assertFalse(any('downloadLinks' in row or 'downloadUrl' in row for row in source['packages']))


if __name__ == '__main__':
    unittest.main()
