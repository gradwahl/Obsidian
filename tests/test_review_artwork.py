"""Review reports keep candidates separate from approval and use local images."""
from html.parser import HTMLParser
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import review_artwork as review


class ReviewTests(unittest.TestCase):
    def test_cjk_captions_fit_the_actual_column_width(self):
        class Font:
            def getlength(self, text):
                return sum(2 if ord(c) > 127 else 1 for c in text)
        font = Font()
        lines = review.caption_lines("カンガエル EXIT Japanese text", font, width=10)
        self.assertTrue(all(font.getlength(line) <= 10 for line in lines))
        self.assertEqual("".join(lines).replace(" ", ""), "カンガエルEXITJapanesetext")

    def test_network_allowlist_and_local_html_are_separate_from_decisions(self):
        entry = {"identity": "Source|id", "title": '<script>Bad & title</script>',
                 "platform": "sony-ps3", "source": "Source", "reason": "ambiguous",
                 "sourceUrl": 'https://source.test/?a=1&b=2', "candidates": [
                     {"url": "https://images.launchbox-app.com/test.jpg", "provider": "launchbox"},
                     {"url": "http://images.launchbox-app.com/test.jpg", "provider": "unsafe"},
                     {"url": "https://127.0.0.1/private", "provider": "unsafe"}]}
        self.assertEqual(review.candidate_urls([entry]), [entry["candidates"][0]["url"]])
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            output = root / "review"
            output.mkdir()
            with patch.object(review, "ROOT", root):
                image = review.local_image(entry["candidates"][0]["url"])
                image.parent.mkdir(parents=True)
                image.write_bytes(b"fixture")
                review.write_index([entry], output, ["sheet-001.jpg"])
            content = (output / "index.html").read_text(encoding="utf-8")
            self.assertIn("&lt;script&gt;Bad &amp; title&lt;/script&gt;", content)
            self.assertNotIn("<script>", content)
            self.assertIn('src="../assets/covers/', content)
            self.assertNotIn('src="https://', content)
            self.assertIn("Nothing on this page is approved", content)
            self.assertEqual(set(p.name for p in output.iterdir()), {"index.html"})
            with patch.object(review, "ROOT", root):
                review.write_index([entry], output, [], {entry["candidates"][0]["url"]})
            self.assertNotIn('<img ', (output / "index.html").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
