"""Source size parsing and the complete offline seed must remain honest."""
import importlib.util
import json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("catalog_sizes",ROOT/"scripts/repair_catalog_sizes.py")
repair=importlib.util.module_from_spec(spec)
spec.loader.exec_module(repair)

class CatalogSizeTests(unittest.TestCase):
    def test_explicit_units(self):
        for text, value in [("Size: 58GB",58),("Size: 699MB",0.699),
                            ("Size: 1,5 GB",1.5),("Size: 1 GiB",1.073741824),
                            ("Size: 512 KB",0.000512),("Size: 1B",1e-9)]:
            self.assertAlmostEqual(repair.description_size_gb(text),value)
    def test_do_not_guess_or_use_backport_component(self):
        for text in ["", "BackPort Size: 3GB", "Size: unknown", "Size: -1 GB", "Size: 1GB\nSize: 2GB"]:
            self.assertEqual(repair.description_size_gb(text),0)
        self.assertEqual(repair.description_size_gb("Size: 58GB\nBackPort Size: 3GB"),58)
    def test_all_seed_records(self):
        rows=json.loads((ROOT/"src/catalogData.json").read_text(encoding="utf-8"))
        self.assertEqual(sum(item['source_name']=='Pippo .exFAT' for item in rows),852)
        for item in rows:
            self.assertEqual(item['size_gb'],repair.corrected_size(item),item['title'])
        expected={'Unknown 9 Awakening Deluxe Edition':58,'NAIAD':1,'Lies of P: Overture':72}
        for title,size in expected.items():
            self.assertEqual(next(i['size_gb'] for i in rows if i['title']==title),size)

if __name__ == '__main__': unittest.main()
