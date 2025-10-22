from typing import Any, TYPE_CHECKING
if TYPE_CHECKING:
    def Import(x: str) -> None: ...
    env: Any

# Provided by PlatformIO at build time
Import("env")

import os, sys, configparser, re

def dbg(*a):
    print("[rename_firmware]", *a)
    sys.stdout.flush()

# project dir & read version from platformio.ini (common.version)
proj_dir = env.subst("$PROJECT_DIR") or os.getcwd()
cfg = configparser.ConfigParser()
cfg.read(os.path.join(proj_dir, "platformio.ini"))
version = None
if cfg.has_section("common") and cfg.has_option("common", "version"):
    version = cfg.get("common", "version")
version = version or "0.0.0"

# collect env name and board (use PIOENV and BOARD)
pioenv = env.get("PIOENV") or env.subst("${PIOENV}") or ""
board = env.subst("${BOARD}") or ""
prog = env.subst("${PROGNAME}") or "firmware"

# sanitize for filenames
def sanitize(s):
    s = str(s)
    s = s.strip()
    s = s.replace(" ", "_")
    return re.sub(r"[^A-Za-z0-9._-]", "_", s)

pioenv_token = sanitize(pioenv)
board_token = sanitize(board)
prog_token = sanitize(prog)
version_token = sanitize(version)

# build new name: <prog>_<env>_<board>_<version>
# if you don't want version, remove the last part below
parts = [prog_token]
if pioenv_token:
    parts.append(pioenv_token)
if board_token:
    parts.append(board_token)
parts.append(version_token)
new_base = "_".join(parts)

try:
    env.Replace(PROGNAME=new_base)
    dbg("set PROGNAME ->", env.subst("${PROGNAME}"))
except Exception as e:
    dbg("failed to set PROGNAME:", e)