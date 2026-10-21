"""Bundle the quiet service and the exact health identity its installer expects."""
import re
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from embed_shortcut_helper import embed


def embed_service(source, assets, target):
    with Path(assets).open(encoding="ascii") as stream:
        prefix = "".join(stream.readline() for _ in range(3))
    match = re.search(r'web_build_id\[\] = "([a-f0-9]{64})";', prefix)
    if not match:
        raise ValueError("Missing embedded preview build identity")
    embed(source, target, "service_payload_elf")
    with Path(target).open("a", encoding="ascii", newline="\n") as stream:
        stream.write('#define OBSIDIAN_EXPECTED_HEALTH "wolodrive-webfirst-v2-files-lan '
                     + match[1] + '\\n"\n')


if __name__ == "__main__":
    embed_service(*sys.argv[1:])
