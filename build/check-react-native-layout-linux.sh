set -eu
source_dir=/mnt/c/Users/Callum/Documents/GitHub/wolodrive
project=/tmp/wolodrive-static.q4N0w2/project
cp -r "$source_dir/src/." "$project/src/"
cp "$source_dir/package.json" "$project/package.json"
cp "$source_dir/tests/test_react_layout.cjs" "$project/tests/"
cd "$project"
export PATH="/tmp/wolodrive-static.q4N0w2/node-v22.16.0-linux-x64/bin:/usr/bin:/bin"
npm run test:web
npm run lint
npm run build > production-build.log 2>&1
npm run build:static > static-build.log 2>&1
python3 tests/check_static_preview.py
