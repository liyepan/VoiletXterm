#!/usr/bin/env bash
# Copy every non-system dylib the bundle references into Contents/Frameworks and
# rewrite the install names. macdeployqt covers Qt itself; this catches the
# Homebrew dependencies Qt was built against.
set -euo pipefail

app=${1:?usage: bundle-libs.sh <path-to-.app>}
frameworks="$app/Contents/Frameworks"
mkdir -p "$frameworks"

is_system_path() {
    case "$1" in
        /usr/lib/*|/System/*|/Library/Apple/*) return 0 ;;
        @rpath/*|@executable_path/*|@loader_path/*) return 0 ;;
    esac
    return 1
}

fix_binary() {
    local bin="$1"
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$bin" 2>/dev/null || true

    local deps
    deps=$(otool -L "$bin" | tail -n +2 | awk '{print $1}')
    local dep name dest
    for dep in $deps; do
        is_system_path "$dep" && continue
        [ -e "$dep" ] || continue
        name=$(basename "$dep")
        dest="$frameworks/$name"
        if [ ! -f "$dest" ]; then
            cp -L "$dep" "$dest"
            chmod u+w "$dest"
            install_name_tool -id "@rpath/$name" "$dest" 2>/dev/null || true
            fix_binary "$dest"
        fi
        install_name_tool -change "$dep" "@rpath/$name" "$bin" 2>/dev/null || true
    done
}

exe="$app/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$app/Contents/Info.plist")"
fix_binary "$exe"

# Every framework and plugin may drag in its own dependencies.
find "$app/Contents/Frameworks" "$app/Contents/PlugIns" -name '*.dylib' -o -name '*.so' 2>/dev/null \
    | while read -r lib; do fix_binary "$lib"; done

echo "bundled libraries:"
ls -1 "$frameworks" | sed 's/^/  /'
