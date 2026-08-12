Import("env")
from os.path import join
from pathlib import Path

fw = Path(env["PROJECT_DIR"]).resolve().parent
shared = fw / "src"
if not shared.exists():
    raise SystemExit("shared src missing: %s" % shared)

# Shared headers + lvgl package parent for "lvgl/lvgl.h".
# Device include/ stays first (platformio -I include) for stubs like sim/github_ota.h.
libdeps = Path(env["PROJECT_LIBDEPS_DIR"]) / env["PIOENV"]
device_inc = Path(env["PROJECT_DIR"]) / "include"
env.Prepend(CPPPATH=[str(device_inc)])
env.Append(CPPPATH=[str(shared), str(libdeps)])

build = join("$BUILD_DIR", "shared")

env.BuildSources(
    join(build, "app"),
    str(shared / "app"),
    "+<app.cpp> +<active_games.cpp> +<checklist.cpp> +<desk_timer.cpp> "
    "+<page_log.cpp> +<score_log.cpp> "
    "-<storage.cpp> -<background.cpp>",
)

env.BuildSources(
    join(build, "ui"),
    str(shared / "ui"),
    "+<*> -<fonts.cpp> -<emoji_badge.cpp> -<emoji_palette.cpp>",
)
