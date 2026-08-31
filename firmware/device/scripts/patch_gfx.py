Import("env")
import os

def patch_gfx(source, target, env):
    gfx_dir = env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV/GFX Library for Arduino/src/databus")
    cpp_file = os.path.join(gfx_dir, "Arduino_ESP32RGBPanel.cpp")
    
    if not os.path.exists(cpp_file):
        print(f"File not found: {cpp_file}")
        return

    with open(cpp_file, "r") as f:
        content = f.read()

    # Add bounce buffer if not present
    target_str = "_panel_config->timings.flags.pclk_idle_high = _pclk_idle_high;"
    patch_str = "\n    _panel_config->bounce_buffer_size_px = w * 10;\n"
    
    if target_str in content and "bounce_buffer_size_px" not in content:
        content = content.replace(target_str, target_str + patch_str)
        with open(cpp_file, "w") as f:
            f.write(content)
        print("Patched Arduino_ESP32RGBPanel.cpp with bounce buffer!")

env.AddPreAction("buildprog", patch_gfx)
