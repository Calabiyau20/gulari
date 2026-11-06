#!/usr/bin/env bash
# Downloads the libraries Synthgularity builds against, at the exact versions it was tested with.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p libs

fetch () {   # fetch <dir> <url> <commit>
    if [ ! -d "libs/$1/.git" ]; then
        git clone --quiet "$2" "libs/$1"
    fi
    git -C "libs/$1" fetch --quiet origin "$3" || true
    git -C "libs/$1" checkout --quiet "$3"
}

fetch JUCE                 https://github.com/juce-framework/JUCE.git              f72bad64d29715216226685810c5196bd0d79d77   # 8.0.9
fetch eurorack             https://github.com/pichenettes/eurorack.git             f4cfa7697095a28d292bff758beb51deb49b5a86
rm -rf libs/eurorack/stmlib
git clone --quiet https://github.com/pichenettes/stmlib.git libs/eurorack/stmlib
git -C libs/eurorack/stmlib checkout --quiet d18def816c51d1da0c108236928b2bbd25c17481
fetch clap-juce-extensions https://github.com/free-audio/clap-juce-extensions.git  7adee3a1bd4684d4caa5601100e364abccff4b4d
git -C libs/clap-juce-extensions submodule update --init --recursive --quiet

echo "Libraries ready. Next:"
echo "  cmake -B build -G Xcode            (or: cmake -B build -DCMAKE_BUILD_TYPE=Release)"
echo "  cmake --build build --config Release"
