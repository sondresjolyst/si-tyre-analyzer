"""PlatformIO pre-build: set the firmware name and generate src/web/Logo.h."""

# env and Import are globals injected by SCons; not resolvable statically.
# mypy: ignore-errors
import os

Import("env")

producer = env.GetProjectOption("custom_producer_name")
sensor = env.GetProjectOption("custom_sensor_type")
prog_version = env.GetProjectOption("custom_version")

firmware_name = f"firmware_{producer}_v{prog_version}"
env.Replace(PROGNAME=firmware_name)

if sensor == "mock":
    env.Append(CPPDEFINES=["SENSOR_IS_MOCK"])


def generate_logo_header():
    project_dir = env["PROJECT_DIR"]
    svg_path = os.path.join(
        project_dir, "tools", "si_tyre_analyzer", "gui", "assets", "si_tyre_logo.svg"
    )
    out_path = os.path.join(project_dir, "src", "web", "Logo.h")
    with open(svg_path, encoding="utf-8") as f:
        svg = f.read()
    content = (
        "#ifndef SRC_WEB_LOGO_H_\n"
        "#define SRC_WEB_LOGO_H_\n\n"
        'static const char kLogo[] = R"SVG(' + svg + ')SVG";\n\n'
        "#endif  // SRC_WEB_LOGO_H_\n"
    )
    if os.path.exists(out_path):
        with open(out_path, encoding="utf-8") as f:
            if f.read() == content:
                return
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)


generate_logo_header()
