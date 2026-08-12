#!/usr/bin/env python3
"""Drive a WerkBuddy desk over USB serial (WERKPAGER_DEVICE_DRIVE=1).

  python firmware/scripts/drive-device.py --port COM5 ping info "shot hub-a"
  python firmware/scripts/drive-device.py --port COM5 tap 100 100 wait 300 shot
"""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys
import time

import serial

BAUD = 921600


def sync(ser: serial.Serial, seconds: float = 8.0) -> bool:
  deadline = time.time() + seconds
  while time.time() < deadline:
    ser.reset_input_buffer()
    ser.write(b"ping\n")
    ser.flush()
    time.sleep(0.12)
    while ser.in_waiting:
      line = ser.readline().decode("utf-8", "replace").strip()
      if line:
        print(f"RX {line}")
      if line.startswith("OK"):
        return True
  return False


def read_exact(ser: serial.Serial, n: int, timeout: float = 90.0) -> bytes:
  buf = bytearray()
  deadline = time.time() + timeout
  while len(buf) < n:
    if time.time() > deadline:
      raise TimeoutError(f"need {n} bytes, got {len(buf)}")
    chunk = ser.read(n - len(buf))
    if chunk:
      buf.extend(chunk)
  return bytes(buf)


def cmd_shot(ser: serial.Serial, name: str, out_dir: pathlib.Path) -> None:
  ser.write(b"shot\n" if not name else f"shot {name}\n".encode())
  ser.flush()
  header = None
  deadline = time.time() + 20
  while time.time() < deadline:
    line = ser.readline().decode("utf-8", "replace").strip()
    if line:
      print(f"RX {line}")
    if line.startswith("SHOT "):
      parts = line.split()
      # SHOT W H RGB565 NBYTES
      header = (int(parts[1]), int(parts[2]), int(parts[4]))
      break
    if line.startswith("FAIL"):
      raise RuntimeError(line)
  if not header:
    raise RuntimeError("No SHOT header")
  w, h, nbytes = header
  print(f"reading {nbytes} bytes...")
  raw = read_exact(ser, nbytes)
  # Trailer lines
  for _ in range(6):
    line = ser.readline().decode("utf-8", "replace").strip()
    if line:
      print(f"RX {line}")
    if line.startswith("OK"):
      break
  out_dir.mkdir(parents=True, exist_ok=True)
  stem = name or "device-preview"
  png = out_dir / f"{stem}.png"
  raw_path = out_dir / f"{stem}.rgb565"
  raw_path.write_bytes(raw)
  conv = pathlib.Path(__file__).with_name("rgb565_to_png.py")
  subprocess.check_call([sys.executable, str(conv), str(w), str(h), str(raw_path), str(png)])
  raw_path.unlink(missing_ok=True)
  print(f"saved {png}")


def run_one(ser: serial.Serial, line: str, out_dir: pathlib.Path) -> None:
  print(f"TX {line}")
  if line.split()[0] == "shot":
    parts = line.split()
    name = parts[1] if len(parts) > 1 else ""
    cmd_shot(ser, name, out_dir)
    return
  ser.write((line + "\n").encode())
  ser.flush()
  deadline = time.time() + 15
  resp = ""
  while time.time() < deadline:
    resp = ser.readline().decode("utf-8", "replace").strip()
    if resp:
      print(f"RX {resp}".encode("ascii", "replace").decode("ascii"))
    if resp.startswith("OK") or resp.startswith("FAIL"):
      break
  if not resp.startswith("OK"):
    raise RuntimeError(f"command failed: {line} -> {resp}")


def main() -> int:
  ap = argparse.ArgumentParser()
  ap.add_argument("--port", required=True)
  ap.add_argument("--baud", type=int, default=BAUD)
  ap.add_argument("--out", default="", help="PNG output dir (default firmware/sim-out)")
  ap.add_argument("cmds", nargs="+", help='commands e.g. ping info "shot hub"')
  args = ap.parse_args()

  firmware = pathlib.Path(__file__).resolve().parents[1]
  out_dir = pathlib.Path(args.out) if args.out else firmware / "sim-out"

  ser = serial.Serial()
  ser.port = args.port
  ser.baudrate = args.baud
  ser.timeout = 1.0
  ser.write_timeout = 5.0
  # Avoid auto-reset on open (CH340 toggles DTR/RTS).
  ser.dtr = False
  ser.rts = False
  ser.open()
  time.sleep(0.3)
  if not sync(ser):
    print("WARN: no ping ACK yet - continuing", file=sys.stderr)

  for c in args.cmds:
    run_one(ser, c, out_dir)

  ser.close()
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
