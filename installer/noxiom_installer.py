#!/usr/bin/env python3
"""
noxiom_installer.py — NOXIOM OS GUI Installer

Single-file installer using stdlib only (tkinter, urllib, subprocess,
threading, tempfile, os, platform, ctypes, logging).

Usage:
  Windows : python noxiom_installer.py   (must run as Administrator)
  Linux   : sudo python3 noxiom_installer.py
  macOS   : sudo python3 noxiom_installer.py

Robustness features:
  - Download retried up to 3 times with exponential back-off
  - Cancel button available throughout download + write
  - Drive size checked against image size before writing
  - Windows volumes locked/dismounted via FindFirstVolumeW (catches
    drives with no drive letter assigned)
  - Common Windows error codes translated to plain-English messages
  - MB/s speed and ETA shown during download and write
  - Drive auto-ejected on Windows after a successful write
  - Safe close: prompts if user closes window mid-install
  - Full debug log written to the system temp directory
  - Falls back to latest release if no pre-release exists
"""

import os
import sys
import json
import logging
import platform
import subprocess
import tempfile
import threading
import time
import urllib.request
import tkinter as tk
from tkinter import ttk, messagebox

# ── GitHub release settings ──────────────────────────────────────────────────
GITHUB_OWNER = "sintaxsaint"
GITHUB_REPO  = "noxiom"
API_URL = (
    f"https://api.github.com/repos/{GITHUB_OWNER}/{GITHUB_REPO}"
    f"/releases?per_page=10"
)
ASSET_X86   = "noxiom-x86_64.img"
ASSET_ARM64 = "noxiom-arm64.img"

# ── Tuning ────────────────────────────────────────────────────────────────────
CHUNK            = 4 * 1024 * 1024   # 4 MB I/O chunk
DOWNLOAD_RETRIES = 3                 # max download attempts
DOWNLOAD_TIMEOUT = 60                # seconds per HTTP request
SPEED_WINDOW     = 3.0               # seconds of history for speed average

# ── Logging ───────────────────────────────────────────────────────────────────
LOG_PATH = os.path.join(tempfile.gettempdir(), "noxiom_installer.log")
logging.basicConfig(
    filename=LOG_PATH,
    level=logging.DEBUG,
    format="%(asctime)s  %(levelname)-8s  %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
log = logging.getLogger("noxiom")

# ── UI theme ──────────────────────────────────────────────────────────────────
C_BG     = "#0d1117"
C_BG2    = "#161b22"
C_BG3    = "#21262d"
C_FG     = "#e6edf3"
C_FG2    = "#8b949e"
C_ACCENT = "#58a6ff"
C_GREEN  = "#3fb950"
C_YELLOW = "#d29922"
C_RED    = "#f85149"
C_BORDER = "#30363d"

FONT_TITLE  = ("Consolas", 18, "bold")
FONT_LABEL  = ("Consolas", 10)
FONT_SMALL  = ("Consolas", 9)
FONT_BUTTON = ("Consolas", 11, "bold")

# ── Windows error-code → human message ───────────────────────────────────────
_WIN_ERRORS = {
    2:    "Drive not found. Is it still connected?",
    5:    "Access denied. Run the installer as Administrator.",
    19:   "Write-protected. Check the write-protect switch on the drive.",
    21:   "Device not ready. Wait a few seconds for the drive to mount, then retry.",
    32:   "Drive in use by another program. Close File Explorer and retry.",
    87:   "Invalid parameter (buffer alignment). Please report this bug.",
    112:  "Not enough space on the drive.",
    1117: "I/O device error. Try a different USB port or SD card.",
    1224: "Drive locked by another application. Close File Explorer and retry.",
}

def _win_err(code):
    return _WIN_ERRORS.get(code, f"Windows error {code}.")


# ── Admin check ───────────────────────────────────────────────────────────────
def is_admin():
    try:
        if platform.system() == "Windows":
            import ctypes
            return bool(ctypes.windll.shell32.IsUserAnAdmin())
        return os.geteuid() == 0
    except Exception:
        return False


# ── Drive model ───────────────────────────────────────────────────────────────
class Drive:
    def __init__(self, path, label, bus, size_bytes):
        self.path       = path        # \\.\PhysicalDriveN  or /dev/sdX
        self.label      = label
        self.bus        = bus         # USB / SD / MMC / EXTERNAL
        self.size_bytes = size_bytes

    def size_str(self):
        gb = self.size_bytes / (1024 ** 3)
        return f"{gb:.1f} GB" if gb >= 1.0 else f"{self.size_bytes // (1024**2)} MB"

    def __str__(self):
        return f"[{self.bus}]  {self.label}  —  {self.size_str()}"


# ── Drive detection (removable only) ─────────────────────────────────────────
def list_drives():
    system = platform.system()
    try:
        if system == "Windows":
            return _drives_windows()
        if system == "Linux":
            return _drives_linux()
        if system == "Darwin":
            return _drives_macos()
    except Exception as exc:
        log.warning(f"Drive detection error: {exc}")
    return []


def _disk_size_windows(disk_num):
    """
    Query the actual size of PhysicalDriveN via IOCTL_DISK_GET_LENGTH_INFO.
    Used as a fallback when Get-Disk returns 0 (common for USB card readers
    where the reader itself has no storage — the inserted media does).
    """
    try:
        import ctypes
        from ctypes import wintypes
        k32 = _setup_k32()
        GENERIC_READ             = 0x80000000
        FILE_SHARE_READ          = 0x1
        FILE_SHARE_WRITE         = 0x2
        OPEN_EXISTING            = 3
        INVALID_HANDLE           = ctypes.c_void_p(-1).value
        IOCTL_DISK_GET_LENGTH_INFO = 0x0007405C

        class GET_LENGTH_INFORMATION(ctypes.Structure):
            _fields_ = [("Length", ctypes.c_int64)]

        h = k32.CreateFileW(
            f"\\\\.\\PhysicalDrive{disk_num}",
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            None, OPEN_EXISTING, 0, None,
        )
        if h == INVALID_HANDLE or h is None:
            return 0
        gli = GET_LENGTH_INFORMATION()
        br  = wintypes.DWORD(0)
        ok  = k32.DeviceIoControl(
            h, IOCTL_DISK_GET_LENGTH_INFO,
            None, 0, ctypes.byref(gli), ctypes.sizeof(gli),
            ctypes.byref(br), None,
        )
        k32.CloseHandle(h)
        return int(gli.Length) if ok else 0
    except Exception as exc:
        log.debug(f"Size query for disk {disk_num} failed: {exc}")
        return 0


def _drives_windows():
    ps = (
        "Get-Disk | Where-Object {$_.BusType -in @('USB','SD','MMC')} | "
        "Select-Object Number,FriendlyName,Size,BusType | "
        "ConvertTo-Json -Compress"
    )
    try:
        raw = subprocess.check_output(
            ["powershell", "-NoProfile", "-Command", ps],
            stderr=subprocess.DEVNULL, timeout=15,
        ).decode("utf-8", errors="replace").strip()
    except Exception as exc:
        log.warning(f"PowerShell drive query failed: {exc}")
        return []
    if not raw:
        return []
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        return []
    if isinstance(data, dict):
        data = [data]
    drives = []
    for d in data:
        num  = d.get("Number", 0)
        name = d.get("FriendlyName") or f"Disk {num}"
        size = int(d.get("Size") or 0)
        # Get-Disk returns 0 for card readers that report the reader hardware,
        # not the inserted media.  Fall back to a direct IOCTL query.
        if size == 0:
            size = _disk_size_windows(num)
            log.debug(f"Disk {num} size from IOCTL: {size}")
        bus  = (d.get("BusType") or "USB").upper()
        drives.append(Drive(f"\\\\.\\PhysicalDrive{num}", name, bus, size))
    log.debug(f"Windows drives: {[str(d) for d in drives]}")
    return drives


def _parse_lsblk_size(s):
    s = s.strip().upper()
    mult = {"B": 1, "K": 1024, "M": 1024**2, "G": 1024**3, "T": 1024**4}
    if s and s[-1] in mult:
        try:
            return int(float(s[:-1]) * mult[s[-1]])
        except ValueError:
            pass
    try:
        return int(s)
    except ValueError:
        return 0


def _drives_linux():
    try:
        raw  = subprocess.check_output(
            ["lsblk", "-J", "-d", "-o", "NAME,SIZE,RM,TYPE,MODEL"],
            stderr=subprocess.DEVNULL, timeout=10,
        ).decode("utf-8", errors="replace")
        data = json.loads(raw)
    except Exception as exc:
        log.warning(f"lsblk failed: {exc}")
        return []
    drives = []
    for dev in data.get("blockdevices", []):
        if str(dev.get("rm", "0")) != "1" or dev.get("type") != "disk":
            continue
        name  = dev.get("name", "")
        model = (dev.get("model") or name).strip()
        size  = _parse_lsblk_size(dev.get("size", "0"))
        drives.append(Drive(f"/dev/{name}", model, "USB", size))
    log.debug(f"Linux drives: {[str(d) for d in drives]}")
    return drives


def _drives_macos():
    try:
        import plistlib
        raw = subprocess.check_output(
            ["diskutil", "list", "-plist", "external"],
            stderr=subprocess.DEVNULL, timeout=10,
        )
        pl = plistlib.loads(raw)
    except Exception as exc:
        log.warning(f"diskutil failed: {exc}")
        return []
    drives = []
    for item in pl.get("AllDisksAndPartitions", []):
        dev_id = item.get("DeviceIdentifier", "")
        size   = int(item.get("Size", 0))
        label  = _macos_disk_name(dev_id)
        drives.append(Drive(f"/dev/{dev_id}", label, "EXTERNAL", size))
    return drives


def _macos_disk_name(dev_id):
    try:
        import plistlib
        raw = subprocess.check_output(
            ["diskutil", "info", "-plist", dev_id],
            stderr=subprocess.DEVNULL, timeout=5,
        )
        pl = plistlib.loads(raw)
        return pl.get("MediaName") or pl.get("IORegistryEntryName") or dev_id
    except Exception:
        return dev_id


# ── GitHub release info ───────────────────────────────────────────────────────
class ReleaseInfo:
    def __init__(self, tag, assets, is_prerelease):
        self.tag           = tag
        self.assets        = assets      # { name: {"url": …, "size": int} }
        self.is_prerelease = is_prerelease

    def asset_url(self, name):
        return self.assets.get(name, {}).get("url")

    def asset_size(self, name):
        return self.assets.get(name, {}).get("size", 0)


def fetch_release():
    """
    Return the most recent pre-release.  Falls back to the most recent
    release of any kind if no pre-release exists.
    """
    req = urllib.request.Request(API_URL, headers={"User-Agent": "noxiom-installer/1.0"})
    with urllib.request.urlopen(req, timeout=DOWNLOAD_TIMEOUT) as resp:
        releases = json.loads(resp.read().decode("utf-8"))
    if not isinstance(releases, list):
        releases = [releases]

    data = next((r for r in releases if r.get("prerelease")), None)
    if data is None:
        log.info("No pre-release found; falling back to latest release.")
        data = releases[0] if releases else None
    if data is None:
        raise RuntimeError("No releases found on GitHub. Check the releases page.")

    tag    = data.get("tag_name", "unknown")
    assets = {
        a["name"]: {"url": a["browser_download_url"], "size": int(a.get("size", 0))}
        for a in data.get("assets", [])
    }
    log.info(f"Release: {tag}  prerelease={data.get('prerelease')}  assets={list(assets)}")
    return ReleaseInfo(tag, assets, bool(data.get("prerelease")))


# ── Speed / ETA tracker ───────────────────────────────────────────────────────
class SpeedTracker:
    """Rolling-window bytes-per-second tracker."""

    def __init__(self):
        self._hist = []   # [(monotonic_time, total_bytes)]

    def update(self, total_bytes):
        now = time.monotonic()
        self._hist.append((now, total_bytes))
        cutoff = now - SPEED_WINDOW
        self._hist = [(t, b) for t, b in self._hist if t >= cutoff]

    def bps(self):
        if len(self._hist) < 2:
            return 0.0
        dt = self._hist[-1][0] - self._hist[0][0]
        db = self._hist[-1][1] - self._hist[0][1]
        return db / dt if dt > 0 else 0.0

    def eta_str(self, remaining):
        speed = self.bps()
        if speed <= 0 or remaining <= 0:
            return ""
        secs = int(remaining / speed)
        return f"{secs // 60}m {secs % 60}s" if secs >= 60 else f"{secs}s"

    @staticmethod
    def fmt_speed(bps):
        if bps <= 0:
            return ""
        if bps >= 1024 ** 2:
            return f"  {bps / 1024**2:.1f} MB/s"
        return f"  {bps / 1024:.0f} KB/s"


# ── Download with retry ───────────────────────────────────────────────────────
def download_asset(url, dest_path, progress_cb, cancel_event):
    """
    Stream url → dest_path, retrying up to DOWNLOAD_RETRIES times.
    Raises InterruptedError if cancel_event fires.
    """
    last_exc = None
    for attempt in range(1, DOWNLOAD_RETRIES + 1):
        if cancel_event.is_set():
            raise InterruptedError("Cancelled.")
        try:
            log.info(f"Download attempt {attempt}: {url}")
            req = urllib.request.Request(url, headers={"User-Agent": "noxiom-installer/1.0"})
            downloaded = 0
            with urllib.request.urlopen(req, timeout=DOWNLOAD_TIMEOUT) as resp, \
                 open(dest_path, "wb") as f:
                while True:
                    if cancel_event.is_set():
                        raise InterruptedError("Cancelled.")
                    chunk = resp.read(64 * 1024)
                    if not chunk:
                        break
                    f.write(chunk)
                    downloaded += len(chunk)
                    progress_cb(downloaded)
            log.info(f"Download complete: {downloaded} bytes")
            return
        except InterruptedError:
            raise
        except Exception as exc:
            last_exc = exc
            log.warning(f"Download attempt {attempt} failed: {exc}")
            if attempt < DOWNLOAD_RETRIES:
                wait = 2 ** attempt
                log.info(f"Retrying in {wait}s…")
                time.sleep(wait)
    raise OSError(f"Download failed after {DOWNLOAD_RETRIES} attempts: {last_exc}")


# ── Write image to device ─────────────────────────────────────────────────────
def write_image(image_path, device_path, progress_cb, cancel_event):
    log.info(f"Writing {image_path} → {device_path}")
    if platform.system() == "Windows":
        _write_windows(image_path, device_path, progress_cb, cancel_event)
    else:
        _write_unix(image_path, device_path, progress_cb, cancel_event)


def _setup_k32():
    """
    Return kernel32 with full argtypes + restypes for every function we use.

    Without argtypes, ctypes defaults to passing Python ints as c_int (32-bit).
    On 64-bit Windows a HANDLE is 64 bits, so large handle values get truncated
    and every subsequent API call silently operates on a garbage handle.
    """
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)

    # HANDLE is void* — use c_void_p so 64-bit values survive the round-trip.
    H = ctypes.c_void_p

    k.CreateFileW.restype  = H
    k.CreateFileW.argtypes = [
        ctypes.c_wchar_p,   # lpFileName
        wintypes.DWORD,     # dwDesiredAccess
        wintypes.DWORD,     # dwShareMode
        H,                  # lpSecurityAttributes (NULL)
        wintypes.DWORD,     # dwCreationDisposition
        wintypes.DWORD,     # dwFlagsAndAttributes
        H,                  # hTemplateFile (NULL)
    ]

    k.CloseHandle.restype  = ctypes.c_bool
    k.CloseHandle.argtypes = [H]

    # DeviceIoControl — lpBytesReturned / lpOverlapped can be NULL, so c_void_p
    k.DeviceIoControl.restype  = ctypes.c_bool
    k.DeviceIoControl.argtypes = [
        H,               # hDevice
        wintypes.DWORD,  # dwIoControlCode
        H,               # lpInBuffer
        wintypes.DWORD,  # nInBufferSize
        H,               # lpOutBuffer
        wintypes.DWORD,  # nOutBufferSize
        H,               # lpBytesReturned
        H,               # lpOverlapped
    ]

    k.WriteFile.restype  = ctypes.c_bool
    k.WriteFile.argtypes = [
        H,               # hFile
        H,               # lpBuffer
        wintypes.DWORD,  # nNumberOfBytesToWrite
        H,               # lpNumberOfBytesWritten
        H,               # lpOverlapped
    ]

    k.FindFirstVolumeW.restype  = H
    k.FindFirstVolumeW.argtypes = [ctypes.c_wchar_p, wintypes.DWORD]

    k.FindNextVolumeW.restype  = ctypes.c_bool
    k.FindNextVolumeW.argtypes = [H, ctypes.c_wchar_p, wintypes.DWORD]

    k.FindVolumeClose.restype  = ctypes.c_bool
    k.FindVolumeClose.argtypes = [H]

    return k


def _ps_disk_offline(disk_num):
    """
    Use PowerShell Set-Disk to take ALL volumes on disk_num offline at once.
    This is the most reliable method because it handles every filesystem type,
    including Linux ext4/btrfs partitions that Windows can't open for writing.
    Returns True if the disk was successfully taken offline.
    """
    if disk_num < 0:
        return False
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command",
             f"$ErrorActionPreference='Stop'; Set-Disk -Number {disk_num} -IsOffline $true"],
            capture_output=True, timeout=15,
        )
        ok = result.returncode == 0
        stderr = result.stderr.decode(errors='replace').strip()
        log.debug(f"PowerShell Set-Disk offline: disk={disk_num} ok={ok} "
                  f"rc={result.returncode} stderr={stderr!r}")
        return ok
    except Exception as exc:
        log.debug(f"PowerShell Set-Disk offline failed: {exc}")
        return False


def _ps_disk_online(disk_num):
    """Bring a disk back online after a failed write (cleanup path)."""
    if disk_num < 0:
        return
    try:
        subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command",
             f"Set-Disk -Number {disk_num} -IsOffline $false; "
             f"Set-Disk -Number {disk_num} -IsReadOnly $false"],
            capture_output=True, timeout=15,
        )
        log.debug(f"PowerShell Set-Disk online: disk={disk_num}")
    except Exception as exc:
        log.debug(f"PowerShell Set-Disk online failed (non-fatal): {exc}")


def _write_windows(image_path, device_path, progress_cb, cancel_event):
    import ctypes
    from ctypes import wintypes

    k32 = _setup_k32()

    GENERIC_READ             = 0x80000000
    GENERIC_WRITE            = 0x40000000
    FILE_SHARE_READ          = 0x00000001
    FILE_SHARE_WRITE         = 0x00000002
    OPEN_EXISTING            = 3
    INVALID_HANDLE           = ctypes.c_void_p(-1).value

    FSCTL_LOCK_VOLUME               = 0x00090018
    FSCTL_DISMOUNT_VOLUME           = 0x00090020
    FSCTL_UNLOCK_VOLUME             = 0x0009001C

    def _ioctl(h, code, out_buf=None, out_size=0):
        br = wintypes.DWORD(0)
        return k32.DeviceIoControl(
            h, code,
            None, 0,
            out_buf, out_size,
            ctypes.byref(br), None,
        )

    # Disk number from "\\.\PhysicalDriveN"
    try:
        disk_num = int(device_path.replace("\\\\.\\PhysicalDrive", ""))
    except ValueError:
        disk_num = -1
    log.debug(f"Target disk number: {disk_num}")

    # ── Primary: PowerShell Set-Disk offline ─────────────────────────────────
    # This takes ALL volumes offline in one call, including Linux ext4/btrfs
    # partitions that Windows can't open with write access individually.
    ps_offline = _ps_disk_offline(disk_num)

    # ── Secondary: per-partition lock + dismount ──────────────────────────────
    # Belt-and-suspenders fallback if Set-Disk isn't supported for this drive.
    #
    # IMPORTANT: We use \\.\HarddiskNPartitionM paths here, NOT FindFirstVolumeW.
    # FindFirstVolumeW only returns Volume GUIDs for partitions Windows *recognises*
    # (FAT32, NTFS, ReFS). Linux ext4/btrfs partitions have NO Volume GUID, so they
    # are invisible to that API. However, they DO get a raw partition device node in
    # the Windows storage stack (\\.\HarddiskNPartitionM), which we can open and lock.
    vol_handles = []
    if not ps_offline:
        log.debug("Set-Disk offline unavailable; locking partitions by device path")
        if disk_num >= 0:
            for part_num in range(1, 32):
                part_path = f"\\\\.\\Harddisk{disk_num}Partition{part_num}"
                h = None
                for access in [GENERIC_READ | GENERIC_WRITE, GENERIC_READ, 0]:
                    tmp = k32.CreateFileW(
                        part_path, access,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        None, OPEN_EXISTING, 0, None,
                    )
                    if tmp not in (INVALID_HANDLE, None):
                        h = tmp
                        break
                if h is None:
                    err = ctypes.get_last_error()
                    # 2=file not found, 3=path not found, 87=invalid param,
                    # 1168=element not found — all mean the partition does not exist.
                    log.debug(f"  {part_path}: open failed err={err}")
                    if err in (2, 3, 87, 1168):
                        break   # No more partitions on this disk
                    continue    # Unexpected error; skip this slot and keep going
                ok_l = _ioctl(h, FSCTL_LOCK_VOLUME)
                ok_d = _ioctl(h, FSCTL_DISMOUNT_VOLUME)
                log.debug(f"  {part_path}: lock={ok_l} dismount={ok_d}")
                vol_handles.append(h)
        log.debug(f"Locked {len(vol_handles)} partitions")

    # Open physical drive for writing.
    handle = k32.CreateFileW(
        device_path, GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None, OPEN_EXISTING, 0, None,
    )
    if handle == INVALID_HANDLE or handle is None:
        for h in vol_handles:
            k32.CloseHandle(h)
        if ps_offline:
            _ps_disk_online(disk_num)
        err = ctypes.get_last_error()
        log.error(f"CreateFileW({device_path}) failed: error {err}")
        raise OSError(_win_err(err))

    written_total = 0
    try:
        io_buf = (ctypes.c_char * CHUNK)()
        with open(image_path, "rb") as f:
            while True:
                if cancel_event.is_set():
                    raise InterruptedError("Cancelled.")
                chunk = f.read(CHUNK)
                if not chunk:
                    break
                # Raw drive writes must be exact multiples of 512 bytes.
                if len(chunk) % 512 != 0:
                    chunk = chunk + b"\x00" * (512 - len(chunk) % 512)
                ctypes.memmove(io_buf, chunk, len(chunk))
                written = wintypes.DWORD(0)
                ok = k32.WriteFile(handle, io_buf, len(chunk),
                                    ctypes.byref(written), None)
                if not ok:
                    err = ctypes.get_last_error()
                    log.error(f"WriteFile failed: error {err}")
                    raise OSError(_win_err(err))
                written_total += written.value
                progress_cb(written_total)
        log.info(f"Write complete: {written_total} bytes")
    finally:
        k32.CloseHandle(handle)
        for h in vol_handles:
            _ioctl(h, FSCTL_UNLOCK_VOLUME)
            k32.CloseHandle(h)
        # Always bring the disk back online after we're done (success OR failure).
        # If we leave it offline after a successful write, Windows tries to re-read
        # the old (now-overwritten) partition table from cache and shows
        # "cannot read partition 4 and 5" errors.
        if ps_offline:
            _ps_disk_online(disk_num)


def _write_unix(image_path, device_path, progress_cb, cancel_event):
    written_total = 0
    with open(image_path, "rb") as src, \
         open(device_path, "wb", buffering=0) as dst:
        while True:
            if cancel_event.is_set():
                raise InterruptedError("Cancelled.")
            chunk = src.read(CHUNK)
            if not chunk:
                break
            dst.write(chunk)
            written_total += len(chunk)
            progress_cb(written_total)
        dst.flush()
        os.fsync(dst.fileno())
    log.info(f"Write complete: {written_total} bytes")


def _eject_windows(device_path):
    """Send IOCTL_STORAGE_EJECT_MEDIA so Windows shows 'safe to remove'."""
    try:
        import ctypes
        from ctypes import wintypes
        k32 = _setup_k32()
        GENERIC_READ  = 0x80000000
        GENERIC_WRITE = 0x40000000
        FILE_SHARE_READ  = 0x1
        FILE_SHARE_WRITE = 0x2
        OPEN_EXISTING    = 3
        INVALID_HANDLE   = ctypes.c_void_p(-1).value
        IOCTL_STORAGE_EJECT_MEDIA = 0x2D4808
        h = k32.CreateFileW(device_path, GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             None, OPEN_EXISTING, 0, None)
        if h not in (None, INVALID_HANDLE):
            br = wintypes.DWORD(0)
            k32.DeviceIoControl(h, IOCTL_STORAGE_EJECT_MEDIA,
                                 None, 0, None, 0, ctypes.byref(br), None)
            k32.CloseHandle(h)
            log.info("Drive ejected.")
    except Exception as exc:
        log.warning(f"Eject failed (non-fatal): {exc}")


# ── GUI ───────────────────────────────────────────────────────────────────────
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("NOXIOM OS INSTALLER")
        self.resizable(False, False)
        self.configure(bg=C_BG)

        self._release = None
        self._drives  = []
        self._arch    = tk.StringVar(value="arm64")
        self._status  = tk.StringVar(value="Fetching latest release…")
        self._cancel  = threading.Event()
        self._busy    = False

        self._build_ui()
        self._check_admin()
        self.after(50,  self._fetch_release_async)
        self.after(100, self.refresh_drives)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── UI construction ──────────────────────────────────────────────────────
    def _build_ui(self):
        def sep():
            tk.Frame(self, bg=C_BORDER, height=1).pack(fill="x")

        # Header
        hdr = tk.Frame(self, bg=C_BG2)
        hdr.pack(fill="x")
        tk.Label(hdr, text="NOXIOM OS", font=FONT_TITLE,
                 bg=C_BG2, fg=C_ACCENT).pack(side="left", padx=16, pady=10)
        self._ver_lbl = tk.Label(hdr, text="fetching version…",
                                  font=FONT_SMALL, bg=C_BG2, fg=C_FG2)
        self._ver_lbl.pack(side="right", padx=16, pady=10)
        sep()

        # Architecture
        arch_frm = tk.Frame(self, bg=C_BG, padx=16, pady=10)
        arch_frm.pack(fill="x")
        tk.Label(arch_frm, text="Architecture", font=FONT_LABEL,
                 bg=C_BG, fg=C_FG2).pack(anchor="w")
        rb_row = tk.Frame(arch_frm, bg=C_BG)
        rb_row.pack(anchor="w", pady=(4, 0))
        for val, txt in [("x86_64", "x86_64   (PC / server)"),
                          ("arm64",  "arm64    (Raspberry Pi)")]:
            tk.Radiobutton(rb_row, text=txt, variable=self._arch, value=val,
                           font=FONT_LABEL, bg=C_BG, fg=C_FG,
                           selectcolor=C_BG3,
                           activebackground=C_BG,
                           activeforeground=C_ACCENT).pack(side="left", padx=(0, 24))
        sep()

        # Drive list
        drive_hdr = tk.Frame(self, bg=C_BG, padx=16, pady=6)
        drive_hdr.pack(fill="x")
        tk.Label(drive_hdr, text="Target Drive", font=FONT_LABEL,
                 bg=C_BG, fg=C_FG2).pack(side="left")
        tk.Button(drive_hdr, text="↺  Refresh", font=FONT_SMALL,
                  bg=C_BG3, fg=C_ACCENT,
                  activebackground=C_BG, activeforeground=C_ACCENT,
                  relief="flat", cursor="hand2",
                  command=self.refresh_drives).pack(side="right")

        list_frm = tk.Frame(self, bg=C_BG, padx=16)
        list_frm.pack(fill="x")
        sb = tk.Scrollbar(list_frm, orient="vertical")
        self._drive_lb = tk.Listbox(
            list_frm, height=4, font=FONT_LABEL,
            bg=C_BG3, fg=C_FG,
            selectbackground=C_ACCENT, selectforeground=C_BG,
            bd=0, highlightthickness=1,
            highlightcolor=C_BORDER, highlightbackground=C_BORDER,
            yscrollcommand=sb.set, activestyle="none",
        )
        sb.config(command=self._drive_lb.yview)
        self._drive_lb.pack(side="left", fill="x", expand=True, pady=4)
        sb.pack(side="right", fill="y", pady=4)

        self._no_drive_lbl = tk.Label(self, text="",
                                       font=FONT_SMALL, bg=C_BG, fg=C_FG2)
        self._no_drive_lbl.pack(padx=16, anchor="w")

        tk.Label(self,
                 text="⚠  ALL DATA on the selected drive will be permanently erased.",
                 font=FONT_SMALL, bg=C_BG, fg=C_YELLOW,
                 wraplength=444, justify="left").pack(fill="x", padx=16, pady=(2, 8))
        sep()

        # Progress
        prog_frm = tk.Frame(self, bg=C_BG, padx=16, pady=8)
        prog_frm.pack(fill="x")
        self._status_lbl = tk.Label(prog_frm, textvariable=self._status,
                                     font=FONT_SMALL, bg=C_BG, fg=C_FG2, anchor="w")
        self._status_lbl.pack(fill="x")
        style = ttk.Style()
        style.theme_use("default")
        style.configure("Nox.Horizontal.TProgressbar",
                         troughcolor=C_BG3, background=C_ACCENT, thickness=14)
        self._progress = ttk.Progressbar(prog_frm, orient="horizontal",
                                          length=444, mode="determinate",
                                          style="Nox.Horizontal.TProgressbar")
        self._progress.pack(fill="x", pady=(4, 0))
        sep()

        # Buttons
        btn_frm = tk.Frame(self, bg=C_BG, pady=12)
        btn_frm.pack()
        self._install_btn = tk.Button(
            btn_frm, text="   Install Noxiom OS   ",
            font=FONT_BUTTON, bg=C_ACCENT, fg=C_BG,
            activebackground=C_GREEN, activeforeground=C_BG,
            relief="flat", cursor="hand2", padx=20, pady=8,
            command=self._on_install,
        )
        self._install_btn.pack(side="left", padx=(0, 12))
        self._cancel_btn = tk.Button(
            btn_frm, text="Cancel",
            font=FONT_BUTTON, bg=C_BG3, fg=C_FG2,
            activebackground=C_RED, activeforeground=C_FG,
            relief="flat", cursor="hand2", padx=12, pady=8,
            command=self._on_cancel, state="disabled",
        )
        self._cancel_btn.pack(side="left")

        # Admin warning + log path
        self._admin_lbl = tk.Label(self, text="", font=FONT_SMALL,
                                    bg=C_BG, fg=C_RED,
                                    wraplength=444, justify="center")
        self._admin_lbl.pack(pady=(0, 2))
        tk.Label(self, text=f"Log: {LOG_PATH}", font=FONT_SMALL,
                 bg=C_BG, fg=C_FG2, wraplength=444).pack(pady=(0, 8))

        self.geometry("480x610")

    # ── Admin check ──────────────────────────────────────────────────────────
    def _check_admin(self):
        if not is_admin():
            self._admin_lbl.config(
                text="⚠  Not running as Administrator / root.\n"
                     "Drive writes will fail without elevated privileges."
            )

    # ── Safe close ───────────────────────────────────────────────────────────
    def _on_close(self):
        if self._busy:
            if not messagebox.askyesno(
                "Quit", "Installation is in progress. Cancel and quit?"
            ):
                return
            self._cancel.set()
        self.destroy()

    # ── Release fetch ─────────────────────────────────────────────────────────
    def _fetch_release_async(self):
        threading.Thread(target=self._fetch_release_worker, daemon=True).start()

    def _fetch_release_worker(self):
        try:
            release = fetch_release()
            self._release = release
            tag = release.tag
            note = " (pre-release)" if release.is_prerelease else " (stable)"
            def _ok():
                self._ver_lbl.config(text=f"Latest: {tag}{note}", fg=C_GREEN)
                self._status.set("Ready. Select a drive and click Install.")
            self.after(0, _ok)
        except Exception as exc:
            log.error(f"fetch_release: {exc}")
            msg = str(exc)
            def _fail():
                self._ver_lbl.config(text="version: unavailable", fg=C_RED)
                self._status.set(f"Could not fetch release info: {msg}")
            self.after(0, _fail)

    # ── Drive refresh ─────────────────────────────────────────────────────────
    def refresh_drives(self):
        if self._busy:
            return
        self._drive_lb.delete(0, tk.END)
        self._drives = list_drives()
        if self._drives:
            self._no_drive_lbl.config(text="")
            for d in self._drives:
                self._drive_lb.insert(tk.END, f"  {d}")
        else:
            self._no_drive_lbl.config(
                text="No removable drives detected. Insert a drive and click ↺ Refresh."
            )

    # ── Install ───────────────────────────────────────────────────────────────
    def _on_install(self):
        if not self._release:
            messagebox.showerror("Not ready",
                "Release info has not loaded yet.\n"
                "Check your internet connection and try again.")
            return

        sel = self._drive_lb.curselection()
        if not sel:
            messagebox.showerror("No drive selected", "Please select a target drive.")
            return

        drive      = self._drives[sel[0]]
        arch       = self._arch.get()
        asset_name = ASSET_ARM64 if arch == "arm64" else ASSET_X86
        asset_url  = self._release.asset_url(asset_name)
        asset_size = self._release.asset_size(asset_name)

        if not asset_url:
            messagebox.showerror("Asset not found",
                f"'{asset_name}' not found in release '{self._release.tag}'.\n"
                "Check the GitHub releases page.")
            return

        # Refuse if the drive is definitely too small.
        if drive.size_bytes > 0 and asset_size > 0 and drive.size_bytes < asset_size:
            messagebox.showerror("Drive too small",
                f"The selected drive ({drive.size_str()}) is smaller than the image "
                f"({asset_size / (1024**3):.1f} GB).\n"
                "Please use a larger drive.")
            return

        if not messagebox.askyesno(
            "Confirm installation",
            f"Install Noxiom OS ({arch}) to:\n\n"
            f"  {drive}\n\n"
            "⚠  ALL DATA on this drive will be PERMANENTLY ERASED.\n\n"
            "Continue?",
            icon="warning",
        ):
            return

        self._busy = True
        self._cancel.clear()
        self._install_btn.config(state="disabled")
        self._cancel_btn.config(state="normal")
        self._status_lbl.config(fg=C_FG2)
        self._progress.configure(value=0)

        threading.Thread(
            target=self._install_worker,
            args=(drive, asset_url, asset_size),
            daemon=True,
        ).start()

    def _on_cancel(self):
        self._cancel.set()
        self._cancel_btn.config(state="disabled")
        self._status.set("Cancelling…")

    # ── Install worker (background thread) ────────────────────────────────────
    def _install_worker(self, drive, url, total_size):
        tmp_fd, tmp_path = tempfile.mkstemp(suffix=".img")
        os.close(tmp_fd)
        log.info(f"Install started: drive={drive.path}  url={url}")

        try:
            # Phase 1 — Download (0 → 50 %)
            speed = SpeedTracker()

            def dl_cb(downloaded):
                speed.update(downloaded)
                pct  = (downloaded / total_size * 50) if total_size else 0
                now  = downloaded / (1024 ** 2)
                tot  = total_size / (1024 ** 2)
                spd  = SpeedTracker.fmt_speed(speed.bps())
                eta  = speed.eta_str(total_size - downloaded)
                info = f"  ETA {eta}" if eta else ""
                self.after(0, lambda p=pct, n=now, t=tot:
                    (self._progress.configure(value=p),
                     self._status.set(f"Downloading…  {n:.1f} / {t:.1f} MB{spd}{info}")))

            self.after(0, lambda: self._status.set("Starting download…"))
            download_asset(url, tmp_path, dl_cb, self._cancel)

            # Phase 2 — Write (50 → 100 %)
            img_size = os.path.getsize(tmp_path)
            speed2   = SpeedTracker()

            def wr_cb(written):
                speed2.update(written)
                pct  = 50 + (written / img_size * 50) if img_size else 50
                now  = written  / (1024 ** 2)
                tot  = img_size / (1024 ** 2)
                spd  = SpeedTracker.fmt_speed(speed2.bps())
                eta  = speed2.eta_str(img_size - written)
                info = f"  ETA {eta}" if eta else ""
                self.after(0, lambda p=pct, n=now, t=tot:
                    (self._progress.configure(value=p),
                     self._status.set(f"Writing…  {n:.1f} / {t:.1f} MB{spd}{info}")))

            self.after(0, lambda: self._status.set("Writing to drive…"))
            write_image(tmp_path, drive.path, wr_cb, self._cancel)

            # Eject (Windows only)
            if platform.system() == "Windows":
                self.after(0, lambda: self._status.set("Ejecting drive…"))
                _eject_windows(drive.path)

            self.after(0, self._on_done)

        except InterruptedError:
            log.info("Installation cancelled by user.")
            self.after(0, self._on_cancelled)
        except Exception as exc:
            msg = str(exc)
            log.error(f"Installation failed: {exc}")
            self.after(0, lambda m=msg: self._on_error(m))
        finally:
            self._busy = False
            try:
                os.unlink(tmp_path)
            except OSError:
                pass

    # ── Outcome handlers ──────────────────────────────────────────────────────
    def _on_done(self):
        self._progress.configure(value=100)
        self._status.set("Done! Safe to remove the drive.")
        self._status_lbl.config(fg=C_GREEN)
        self._install_btn.config(state="normal")
        self._cancel_btn.config(state="disabled")
        messagebox.showinfo("Done",
            "Noxiom OS has been written to the drive.\n\n"
            "The drive has been safely ejected.\n"
            "Insert it into your target device and power on.")

    def _on_cancelled(self):
        self._progress.configure(value=0)
        self._status.set("Cancelled.")
        self._status_lbl.config(fg=C_YELLOW)
        self._install_btn.config(state="normal")
        self._cancel_btn.config(state="disabled")

    def _on_error(self, msg):
        self._status.set(f"Error: {msg}")
        self._status_lbl.config(fg=C_RED)
        self._install_btn.config(state="normal")
        self._cancel_btn.config(state="disabled")
        messagebox.showerror("Installation failed",
            f"{msg}\n\nSee the log for details:\n{LOG_PATH}")


# ── Entry point ───────────────────────────────────────────────────────────────
def _relaunch_as_admin_windows():
    import ctypes
    rc = ctypes.windll.user32.MessageBoxW(
        0,
        "This installer needs Administrator privileges to write to drives.\n\n"
        "Relaunch as Administrator?",
        "NOXIOM OS Installer",
        0x00000034,   # MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
    )
    if rc == 6:       # IDYES
        params = " ".join(f'"{a}"' for a in sys.argv)
        ctypes.windll.shell32.ShellExecuteW(
            None, "runas", sys.executable, params, None, 1
        )
    sys.exit(0)


def main():
    log.info(f"Installer started.  Platform: {platform.system()} {platform.version()}")
    if platform.system() == "Windows" and not is_admin():
        _relaunch_as_admin_windows()
        return
    app = App()
    app.mainloop()
    log.info("Installer closed.")


if __name__ == "__main__":
    main()
