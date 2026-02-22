#!/usr/bin/env python3
"""
noxiom_installer.py — NOXIOM OS GUI Installer

Single-file installer using stdlib only (tkinter, urllib, subprocess,
threading, tempfile, os, platform, ctypes).

Usage:
  Windows : python noxiom_installer.py   (must run as Administrator)
  Linux   : sudo python3 noxiom_installer.py
  macOS   : sudo python3 noxiom_installer.py

The installer:
  1. Fetches the latest Noxiom release info from GitHub.
  2. Shows only *removable* drives (USB sticks, SD / micro-SD cards).
     Internal hard disks and NVMe drives are NEVER shown.
  3. After the user picks an arch (x86_64 or arm64) and a drive,
     it downloads the correct raw image and writes it to the drive.
  4. Progress is shown as 0-50 % (download) and 50-100 % (write).
"""

import os
import sys
import json
import platform
import subprocess
import threading
import tempfile
import urllib.request
import urllib.error
import tkinter as tk
from tkinter import ttk, messagebox

# ── GitHub release settings ──────────────────────────────────────────────────
GITHUB_OWNER = "sintaxsaint"
GITHUB_REPO  = "noxiom"
# Fetch up to 10 releases and pick the most recent pre-release.
# GitHub has no "latest pre-release" shortcut endpoint.
API_URL = (
    f"https://api.github.com/repos/{GITHUB_OWNER}/{GITHUB_REPO}/releases?per_page=10"
)

# Expected asset filenames in the GitHub release:
ASSET_X86   = "noxiom-x86_64.img"
ASSET_ARM64 = "noxiom-arm64.img"

# ── UI theme (dark) ───────────────────────────────────────────────────────────
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

CHUNK = 4 * 1024 * 1024   # 4 MB I/O chunks for write


# ── Privilege check ───────────────────────────────────────────────────────────
def is_admin():
    """Return True if running with administrator / root privileges."""
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
        self.path       = path        # e.g. \\.\PhysicalDrive1  or /dev/sdb
        self.label      = label
        self.bus        = bus         # "USB", "SD", "MMC", "EXTERNAL"
        self.size_bytes = size_bytes

    def size_str(self):
        gb = self.size_bytes / (1024 ** 3)
        if gb >= 1.0:
            return f"{gb:.1f} GB"
        return f"{self.size_bytes // (1024 ** 2)} MB"

    def __str__(self):
        return f"[{self.bus}]  {self.label}  —  {self.size_str()}"


# ── Drive detection (removable only) ─────────────────────────────────────────
def list_drives():
    """Return a list of Drive objects for removable drives only."""
    system = platform.system()
    if system == "Windows":
        return _drives_windows()
    if system == "Linux":
        return _drives_linux()
    if system == "Darwin":
        return _drives_macos()
    return []


def _drives_windows():
    ps = (
        "Get-Disk | Where-Object {$_.BusType -in @('USB','SD','MMC')} | "
        "Select-Object Number,FriendlyName,Size,BusType | "
        "ConvertTo-Json -Compress"
    )
    try:
        raw = subprocess.check_output(
            ["powershell", "-NoProfile", "-Command", ps],
            stderr=subprocess.DEVNULL, timeout=15
        ).decode("utf-8", errors="replace").strip()
    except Exception:
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
        num   = d.get("Number", 0)
        name  = d.get("FriendlyName") or f"Disk {num}"
        size  = int(d.get("Size") or 0)
        bus   = (d.get("BusType") or "USB").upper()
        drives.append(Drive(
            path=f"\\\\.\\PhysicalDrive{num}",
            label=name, bus=bus, size_bytes=size
        ))
    return drives


def _parse_size(s):
    """Parse lsblk size string like '32G', '512M', '1T' → bytes."""
    s = s.strip().upper()
    if not s:
        return 0
    mult = {"B": 1, "K": 1024, "M": 1024**2, "G": 1024**3, "T": 1024**4}
    unit = s[-1]
    if unit in mult:
        try:
            return int(float(s[:-1]) * mult[unit])
        except ValueError:
            return 0
    try:
        return int(s)
    except ValueError:
        return 0


def _drives_linux():
    try:
        raw = subprocess.check_output(
            ["lsblk", "-J", "-d", "-o", "NAME,SIZE,RM,TYPE,MODEL"],
            stderr=subprocess.DEVNULL, timeout=10
        ).decode("utf-8", errors="replace")
    except Exception:
        return []
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        return []
    drives = []
    for dev in data.get("blockdevices", []):
        if str(dev.get("rm", "0")) != "1":
            continue
        if dev.get("type", "") != "disk":
            continue
        name  = dev.get("name", "")
        model = (dev.get("model") or name).strip()
        size  = _parse_size(dev.get("size", "0"))
        drives.append(Drive(
            path=f"/dev/{name}", label=model, bus="USB", size_bytes=size
        ))
    return drives


def _drives_macos():
    try:
        import plistlib
        raw = subprocess.check_output(
            ["diskutil", "list", "-plist", "external"],
            stderr=subprocess.DEVNULL, timeout=10
        )
        pl = plistlib.loads(raw)
    except Exception:
        return []
    drives = []
    for item in pl.get("AllDisksAndPartitions", []):
        dev_id = item.get("DeviceIdentifier", "")
        size   = int(item.get("Size", 0))
        label  = _macos_disk_name(dev_id)
        drives.append(Drive(
            path=f"/dev/{dev_id}", label=label, bus="EXTERNAL", size_bytes=size
        ))
    return drives


def _macos_disk_name(dev_id):
    try:
        import plistlib
        raw = subprocess.check_output(
            ["diskutil", "info", "-plist", dev_id],
            stderr=subprocess.DEVNULL, timeout=5
        )
        pl = plistlib.loads(raw)
        return (pl.get("MediaName") or pl.get("IORegistryEntryName") or dev_id)
    except Exception:
        return dev_id


# ── GitHub release info ───────────────────────────────────────────────────────
class ReleaseInfo:
    def __init__(self, tag, assets):
        self.tag    = tag     # e.g. "v0.1.0"
        self.assets = assets  # { name: {"url": ..., "size": int} }

    def asset_url(self, name):
        return self.assets.get(name, {}).get("url")

    def asset_size(self, name):
        return self.assets.get(name, {}).get("size", 0)


def fetch_release():
    req = urllib.request.Request(
        API_URL, headers={"User-Agent": "noxiom-installer/1.0"}
    )
    with urllib.request.urlopen(req, timeout=15) as resp:
        payload = json.loads(resp.read().decode("utf-8"))
    # Releases are returned newest-first; find the first one marked as a pre-release
    releases = payload if isinstance(payload, list) else [payload]
    data = next((r for r in releases if r.get("prerelease")), None)
    if data is None:
        raise RuntimeError("No pre-release found on GitHub. Check the releases page.")
    tag    = data.get("tag_name", "unknown")
    assets = {}
    for a in data.get("assets", []):
        assets[a["name"]] = {
            "url":  a["browser_download_url"],
            "size": int(a.get("size", 0)),
        }
    return ReleaseInfo(tag, assets)


# ── Download ──────────────────────────────────────────────────────────────────
def download_asset(url, dest_path, progress_cb, total_bytes):
    """Stream url to dest_path, calling progress_cb(bytes_downloaded)."""
    req = urllib.request.Request(
        url, headers={"User-Agent": "noxiom-installer/1.0"}
    )
    downloaded = 0
    with urllib.request.urlopen(req, timeout=60) as resp, \
         open(dest_path, "wb") as f:
        while True:
            chunk = resp.read(64 * 1024)
            if not chunk:
                break
            f.write(chunk)
            downloaded += len(chunk)
            progress_cb(downloaded)


# ── Write image to device ─────────────────────────────────────────────────────
def write_image(image_path, device_path, progress_cb, total_bytes):
    """Write image_path raw to device_path, calling progress_cb(bytes_written)."""
    system = platform.system()
    if system == "Windows":
        _write_windows(image_path, device_path, progress_cb, total_bytes)
    else:
        _write_unix(image_path, device_path, progress_cb, total_bytes)


def _write_windows(image_path, device_path, progress_cb, total_bytes):
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateFileW.restype  = ctypes.c_void_p
    kernel32.WriteFile.restype    = ctypes.c_bool
    kernel32.CloseHandle.restype  = ctypes.c_bool

    GENERIC_WRITE    = 0x40000000
    FILE_SHARE_READ  = 0x00000001
    FILE_SHARE_WRITE = 0x00000002
    OPEN_EXISTING    = 3
    INVALID_HANDLE   = ctypes.c_void_p(-1).value

    handle = kernel32.CreateFileW(
        device_path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None, OPEN_EXISTING, 0, None
    )
    if handle == INVALID_HANDLE or handle is None:
        err = ctypes.get_last_error()
        raise OSError(
            f"Cannot open {device_path} (error {err}).\n"
            "Make sure you are running as Administrator and the drive is connected."
        )

    written_total = 0
    try:
        buf = (ctypes.c_char * CHUNK)()
        with open(image_path, "rb") as f:
            while True:
                chunk = f.read(CHUNK)
                if not chunk:
                    break
                # Windows raw drive writes must be exact multiples of 512 bytes.
                # Pad the last chunk to the next sector boundary if needed.
                if len(chunk) % 512 != 0:
                    chunk = chunk + b'\x00' * (512 - len(chunk) % 512)
                ctypes.memmove(buf, chunk, len(chunk))
                written = wintypes.DWORD(0)
                ok = kernel32.WriteFile(
                    handle, buf, len(chunk), ctypes.byref(written), None
                )
                if not ok:
                    err = ctypes.get_last_error()
                    raise OSError(f"WriteFile failed (error {err}).")
                written_total += written.value
                progress_cb(written_total)
    finally:
        kernel32.CloseHandle(handle)


def _write_unix(image_path, device_path, progress_cb, total_bytes):
    written_total = 0
    with open(image_path, "rb") as src, \
         open(device_path, "wb", buffering=0) as dst:
        while True:
            chunk = src.read(CHUNK)
            if not chunk:
                break
            dst.write(chunk)
            written_total += len(chunk)
            progress_cb(written_total)
        dst.flush()
        os.fsync(dst.fileno())


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

        self._build_ui()
        self._check_admin()

        # Kick off async tasks after UI is visible
        self.after(50,  self._fetch_release_async)
        self.after(100, self.refresh_drives)

    # ── UI construction ──────────────────────────────────────────────────────
    def _build_ui(self):
        def sep():
            tk.Frame(self, bg=C_BORDER, height=1).pack(fill="x")

        # ── Header ──────────────────────────────────────────────────────────
        hdr = tk.Frame(self, bg=C_BG2)
        hdr.pack(fill="x")
        tk.Label(
            hdr, text="NOXIOM OS", font=FONT_TITLE, bg=C_BG2, fg=C_ACCENT
        ).pack(side="left", padx=16, pady=10)
        self._ver_lbl = tk.Label(
            hdr, text="fetching version…", font=FONT_SMALL, bg=C_BG2, fg=C_FG2
        )
        self._ver_lbl.pack(side="right", padx=16, pady=10)

        sep()

        # ── Architecture ─────────────────────────────────────────────────────
        arch_frm = tk.Frame(self, bg=C_BG, padx=16, pady=10)
        arch_frm.pack(fill="x")
        tk.Label(
            arch_frm, text="Architecture", font=FONT_LABEL, bg=C_BG, fg=C_FG2
        ).pack(anchor="w")
        rb_row = tk.Frame(arch_frm, bg=C_BG)
        rb_row.pack(anchor="w", pady=(4, 0))
        for val, txt in [
            ("x86_64", "x86_64   (PC / server)"),
            ("arm64",  "arm64    (Raspberry Pi)"),
        ]:
            tk.Radiobutton(
                rb_row, text=txt, variable=self._arch, value=val,
                font=FONT_LABEL, bg=C_BG, fg=C_FG,
                selectcolor=C_BG3,
                activebackground=C_BG, activeforeground=C_ACCENT,
            ).pack(side="left", padx=(0, 24))

        sep()

        # ── Drive list ───────────────────────────────────────────────────────
        drive_hdr = tk.Frame(self, bg=C_BG, padx=16, pady=6)
        drive_hdr.pack(fill="x")
        tk.Label(
            drive_hdr, text="Target Drive", font=FONT_LABEL, bg=C_BG, fg=C_FG2
        ).pack(side="left")
        tk.Button(
            drive_hdr, text="↺  Refresh", font=FONT_SMALL,
            bg=C_BG3, fg=C_ACCENT,
            activebackground=C_BG, activeforeground=C_ACCENT,
            relief="flat", cursor="hand2",
            command=self.refresh_drives,
        ).pack(side="right")

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

        self._no_drive_lbl = tk.Label(
            self, text="",
            font=FONT_SMALL, bg=C_BG, fg=C_FG2
        )
        self._no_drive_lbl.pack(padx=16, anchor="w")

        # ── Warning ──────────────────────────────────────────────────────────
        tk.Label(
            self,
            text="⚠  ALL DATA on the selected drive will be permanently erased.",
            font=FONT_SMALL, bg=C_BG, fg=C_YELLOW,
            wraplength=444, justify="left",
        ).pack(fill="x", padx=16, pady=(2, 8))

        sep()

        # ── Progress ─────────────────────────────────────────────────────────
        prog_frm = tk.Frame(self, bg=C_BG, padx=16, pady=8)
        prog_frm.pack(fill="x")

        self._status_lbl = tk.Label(
            prog_frm, textvariable=self._status,
            font=FONT_SMALL, bg=C_BG, fg=C_FG2, anchor="w",
        )
        self._status_lbl.pack(fill="x")

        style = ttk.Style()
        style.theme_use("default")
        style.configure(
            "Nox.Horizontal.TProgressbar",
            troughcolor=C_BG3, background=C_ACCENT, thickness=14,
        )
        self._progress = ttk.Progressbar(
            prog_frm, orient="horizontal", length=444, mode="determinate",
            style="Nox.Horizontal.TProgressbar",
        )
        self._progress.pack(fill="x", pady=(4, 0))

        sep()

        # ── Install button ───────────────────────────────────────────────────
        btn_frm = tk.Frame(self, bg=C_BG, pady=12)
        btn_frm.pack()
        self._install_btn = tk.Button(
            btn_frm, text="   Install Noxiom OS   ",
            font=FONT_BUTTON, bg=C_ACCENT, fg=C_BG,
            activebackground=C_GREEN, activeforeground=C_BG,
            relief="flat", cursor="hand2", padx=20, pady=8,
            command=self._on_install,
        )
        self._install_btn.pack()

        # ── Admin warning (shown if not elevated) ────────────────────────────
        self._admin_lbl = tk.Label(
            self, text="", font=FONT_SMALL, bg=C_BG, fg=C_RED,
            wraplength=444, justify="center",
        )
        self._admin_lbl.pack(pady=(0, 8))

        self.geometry("480x560")

    # ── Admin check ──────────────────────────────────────────────────────────
    def _check_admin(self):
        if not is_admin():
            self._admin_lbl.config(
                text="⚠  Not running as Administrator / root.\n"
                     "Drive writes will fail without elevated privileges."
            )

    # ── Async release fetch ───────────────────────────────────────────────────
    def _fetch_release_async(self):
        threading.Thread(target=self._fetch_release_worker, daemon=True).start()

    def _fetch_release_worker(self):
        try:
            release = fetch_release()
            self._release = release

            def _ok(tag=release.tag):
                self._ver_lbl.config(text=f"Latest: {tag}", fg=C_GREEN)
                self._status.set("Ready. Select a drive and click Install.")
            self.after(0, _ok)
        except Exception as exc:
            msg = str(exc)
            def _fail(m=msg):
                self._ver_lbl.config(text="version: unavailable", fg=C_RED)
                self._status.set(f"Could not fetch release info: {m}")
            self.after(0, _fail)

    # ── Drive refresh ─────────────────────────────────────────────────────────
    def refresh_drives(self):
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
            messagebox.showerror(
                "Not ready",
                "Release info has not loaded yet.\n"
                "Check your internet connection and try again."
            )
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
            messagebox.showerror(
                "Asset not found",
                f"The release '{self._release.tag}' does not contain '{asset_name}'.\n"
                "Check the GitHub releases page."
            )
            return

        confirmed = messagebox.askyesno(
            "Confirm installation",
            f"Install Noxiom OS ({arch}) to:\n\n"
            f"  {drive}\n\n"
            "⚠  ALL DATA on this drive will be PERMANENTLY ERASED.\n\n"
            "Continue?",
            icon="warning",
        )
        if not confirmed:
            return

        self._install_btn.config(state="disabled")
        self._status_lbl.config(fg=C_FG2)
        self._progress.configure(value=0)

        threading.Thread(
            target=self._install_worker,
            args=(drive, asset_url, asset_size),
            daemon=True,
        ).start()

    def _install_worker(self, drive, url, total_size):
        tmp_fd, tmp_path = tempfile.mkstemp(suffix=".img")
        os.close(tmp_fd)

        try:
            # ── Phase 1: Download (0 → 50 %) ────────────────────────────────
            def dl_cb(downloaded, _total=total_size):
                pct    = (downloaded / _total * 50) if _total else 0
                mb_now = downloaded / (1024 ** 2)
                mb_tot = _total     / (1024 ** 2)
                def _upd(p=pct, n=mb_now, t=mb_tot):
                    self._progress.configure(value=p)
                    self._status.set(f"Downloading…  {n:.1f} / {t:.1f} MB")
                self.after(0, _upd)

            self.after(0, lambda: self._status.set("Starting download…"))
            download_asset(url, tmp_path, dl_cb, total_size)

            # ── Phase 2: Write (50 → 100 %) ─────────────────────────────────
            img_size = os.path.getsize(tmp_path)

            def wr_cb(written, _img=img_size):
                pct    = 50 + (written / _img * 50) if _img else 50
                mb_now = written / (1024 ** 2)
                mb_tot = _img    / (1024 ** 2)
                def _upd(p=pct, n=mb_now, t=mb_tot):
                    self._progress.configure(value=p)
                    self._status.set(f"Writing…  {n:.1f} / {t:.1f} MB")
                self.after(0, _upd)

            self.after(0, lambda: self._status.set("Writing to drive…"))
            write_image(tmp_path, drive.path, wr_cb, img_size)

            self.after(0, self._on_done)

        except Exception as exc:
            msg = str(exc)
            self.after(0, lambda m=msg: self._on_error(m))
        finally:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass

    def _on_done(self):
        self._progress.configure(value=100)
        self._status.set("Installation complete!")
        self._status_lbl.config(fg=C_GREEN)
        self._install_btn.config(state="normal")
        messagebox.showinfo(
            "Done",
            "Noxiom OS has been written to the drive.\n\n"
            "Safely eject the drive, then insert it into your target device."
        )

    def _on_error(self, msg):
        self._status.set(f"Error: {msg}")
        self._status_lbl.config(fg=C_RED)
        self._install_btn.config(state="normal")
        messagebox.showerror("Installation failed", msg)


# ── Entry point ───────────────────────────────────────────────────────────────
def _relaunch_as_admin_windows():
    """On Windows, offer to relaunch the script with UAC elevation."""
    import ctypes
    rc = ctypes.windll.user32.MessageBoxW(
        0,
        "This installer needs Administrator privileges to write to drives.\n\n"
        "Relaunch as Administrator?",
        "NOXIOM OS Installer",
        0x00000034,  # MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
    )
    if rc == 6:   # IDYES
        params = " ".join(f'"{a}"' for a in sys.argv)
        ctypes.windll.shell32.ShellExecuteW(
            None, "runas", sys.executable, params, None, 1
        )
    sys.exit(0)


def main():
    if platform.system() == "Windows" and not is_admin():
        _relaunch_as_admin_windows()
        return

    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
