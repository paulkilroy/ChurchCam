#  Pre-build: stamp the firmware with its git version.
#
#  Injects -DFW_VERSION="..." from `git describe`, so the config page can show the
#  running build and the GitHub release check has something to compare against.
#  With no tags yet it falls back to the short commit (optionally -dirty); once a
#  release is tagged (e.g. v1.2.0) that tag becomes the version string.
Import("env")
import subprocess

def git_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        return "unknown"

v = git_version()
print("FW_VERSION = %s" % v)
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(v))])
