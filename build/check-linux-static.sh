set -eu
check_dir=$(mktemp -d /tmp/wolodrive-static.XXXXXX)
printf '%s\n' "$check_dir"
cd "$check_dir"
curl -fsSLO https://nodejs.org/dist/v22.16.0/node-v22.16.0-linux-x64.tar.xz
curl -fsSLO https://nodejs.org/dist/v22.16.0/SHASUMS256.txt
grep ' node-v22.16.0-linux-x64.tar.xz$' SHASUMS256.txt | sha256sum -c -
tar -xf node-v22.16.0-linux-x64.tar.xz
export PATH="$check_dir/node-v22.16.0-linux-x64/bin:$PATH"
mkdir project
cd project
source_dir=/mnt/c/Users/Callum/Documents/GitHub/wolodrive
cp "$source_dir/package.json" "$source_dir/package-lock.json" "$source_dir/tsconfig.json" "$source_dir/vite.config.ts" "$source_dir/index.html" .
cp -r "$source_dir/src" .
mkdir tests
cp "$source_dir/tests/check_static_preview.py" tests/
git -C "$source_dir" archive HEAD assets | tar -xf -
git init -q
git add assets
npm ci > install.log 2>&1
npm run lint
npm run build:static > static-build.log 2>&1
npm run build > production-build.log 2>&1
python3 tests/check_static_preview.py
printf 'LINUX_CHECK_DIR=%s\n' "$check_dir"
