"""Embed the isolated shortcut installer in the unified PS5 payload."""
import sys
from pathlib import Path


def embed(source, target, symbol="shortcut_helper_elf"):
    data = Path(source).read_bytes()
    if not data.startswith(b'\x7fELF'):
        raise ValueError('Shortcut helper must be an ELF')
    with Path(target).open('w', encoding='ascii', newline='\n') as output:
        output.write('/* Generated isolated shortcut installer. */\n')
        output.write(f'static const unsigned char {symbol}[] = {{\n')
        for start in range(0, len(data), 24):
            output.write(','.join(str(byte) for byte in data[start:start+24]) + ',\n')
        output.write('};\n')


if __name__ == '__main__':
    embed(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "shortcut_helper_elf")
