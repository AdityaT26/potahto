#!/usr/bin/env python3
"""
Potahto PC companion: feeds the PC Monitor app, keeps the clock in sync,
tells the Macro Pad which app you are in (so it can switch to the VS Code /
Chrome / PowerPoint profile on its own), runs {pc ...} macros and sends
notifications from scripts. Works over USB, Bluetooth or WiFi.

    pip install psutil pyserial            # required
    pip install bleak                      # optional: --ble
    pip install nvidia-ml-py               # optional: NVIDIA GPU load + temperature

    python potahto_companion.py                     # USB, auto-detect the port
    python potahto_companion.py --port COM7         # USB, explicit port
    python potahto_companion.py --ble               # Bluetooth (the same link the keys use)
    python potahto_companion.py --wifi potahto.local --pin 123456
    python potahto_companion.py --allow-run         # let {pc ...} macros run commands here
    python potahto_companion.py --notify "Build finished" --title "CI"   # one-shot

USB: the web app and the companion can't share the port at the same time. Use
--ble or --wifi for the companion if you keep the web app open over USB.

Bluetooth: pair Potahto with the PC first (Settings > Bluetooth > Pair). Over a
paired link no PIN is needed; otherwise pass --pin.

--allow-run: a macro like {pc shutdown /h} or {pc code C:\\work} is sent to
the companion, which runs it with your user rights and no window. Without the
flag those requests are only printed. Anyone who can edit Potahto's macros
(needs the web PIN or physical access) can run commands on this PC while the
flag is on, so only use it on your own machines.
"""
import argparse, json, os, queue, socket, subprocess, sys, threading, time, urllib.request, urllib.parse

try:
    import psutil
except ImportError:
    sys.exit("psutil is missing:  pip install psutil")

ESPRESSIF_VID = 0x303A
NUS_SVC = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"     # we write here
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"     # Potahto notifies here


# ---- links ----------------------------------------------------------------------------
class UsbLink:
    def __init__(self, port):
        try:
            import serial, serial.tools.list_ports
        except ImportError:
            sys.exit("pyserial is missing:  pip install pyserial")
        if not port:
            cands = [p.device for p in serial.tools.list_ports.comports() if p.vid == ESPRESSIF_VID]
            if not cands:
                raise ConnectionError("no Potahto found on USB (plug it in, or use --port / --ble / --wifi)")
            port = cands[0]
        self.ser = serial.Serial(port, 115200, timeout=0.2, write_timeout=2)
        try:
            self.ser.dtr = True          # Potahto only answers once the port is "open" (DTR)
            self.ser.rts = False
        except OSError:
            pass
        self.id = 1
        self.events = []
        self.name = f"USB {port}"

    def _line(self, r):
        if r.get("event"):
            self.events.append(r)
            return None
        return r

    def _read(self):
        line = self.ser.readline().decode(errors="ignore").strip()
        if line.startswith("{"):
            try:
                return self._line(json.loads(line))
            except ValueError:
                pass
        return None

    def call(self, cmd, **args):
        self.id += 1
        self.ser.write((json.dumps({"id": self.id, "cmd": cmd, **args}) + "\n").encode())
        deadline = time.time() + 3
        while time.time() < deadline:
            r = self._read()
            if r and r.get("id") == self.id:
                return r
        raise TimeoutError("no answer from Potahto")

    def poll_events(self):
        while self.ser.in_waiting:
            self._read()
        out, self.events = self.events, []
        return out


class WifiLink:
    def __init__(self, host, pin):
        if not pin:
            sys.exit("--pin is required for WiFi (Settings > WiFi on the device shows it)")
        self.url = f"http://{host}/api?pin={urllib.parse.quote(pin)}"
        self.id = 1
        self.polled = 0.0
        self.name = f"WiFi {host}"

    def call(self, cmd, **args):
        self.id += 1
        body = json.dumps({"id": self.id, "cmd": cmd, **args}).encode()
        req = urllib.request.Request(self.url, data=body, method="POST")
        with urllib.request.urlopen(req, timeout=4) as r:
            out = json.loads(r.read())
        if out.get("err") == "auth":
            sys.exit("Potahto rejected the PIN")
        return out

    def poll_events(self):
        if time.time() - self.polled < 0.5:          # don't hammer the device over HTTP
            return []
        self.polled = time.time()
        return self.call("events").get("events", [])


class BleLink:
    """Nordic UART service on Potahto, driven by bleak in a background thread."""

    def __init__(self, want, pin):
        try:
            import asyncio, bleak  # noqa: F401
        except ImportError:
            sys.exit("bleak is missing:  pip install bleak")
        import asyncio
        self.asyncio = asyncio
        self.loop = asyncio.new_event_loop()
        threading.Thread(target=self.loop.run_forever, daemon=True).start()
        self.want = (want or "").lower()
        self.buf = b""
        self.id = 1
        self.waiting = {}
        self.events = queue.Queue()
        self.client = None
        self.lost = False
        try:
            self._run(self._connect(), 30)
        except BaseException:
            self.close()
            raise
        hello = self.call("hello")
        if hello.get("needPin"):
            if not pin:
                self.close()
                raise ConnectionError("this Bluetooth link isn't paired: pair Potahto with the PC or pass --pin")
            r = self.call("auth", pin=pin)
            if not r.get("ok"):
                self.close()
                sys.exit(f"Potahto rejected the PIN ({r.get('err')})")

    def _run(self, coro, timeout):
        return self.asyncio.run_coroutine_threadsafe(coro, self.loop).result(timeout)

    def _match(self, name, uuids=()):
        n = (name or "").lower()
        if self.want:
            return self.want in n
        return "potahto" in n or NUS_SVC in [u.lower() for u in uuids]

    async def _find(self):
        from bleak import BleakScanner
        from bleak.backends.device import BLEDevice
        found = await BleakScanner.discover(timeout=5, return_adv=True)
        for d, adv in found.values():
            if self._match(adv.local_name or d.name, adv.service_uuids):
                return d
        # Potahto is already connected to this PC as a keyboard/mouse, so it is not
        # advertising. Windows still knows it as a paired device: connect by address.
        if sys.platform == "win32":
            try:
                from winrt.windows.devices.bluetooth import BluetoothLEDevice
                from winrt.windows.devices.enumeration import DeviceInformation
                sel = BluetoothLEDevice.get_device_selector_from_pairing_state(True)
                for info in await DeviceInformation.find_all_async_aqs_filter(sel):
                    if not self._match(info.name):
                        continue
                    dev = await BluetoothLEDevice.from_id_async(info.id)
                    if dev is None:
                        continue
                    a = f"{dev.bluetooth_address:012X}"
                    return BLEDevice(":".join(a[i:i + 2] for i in range(0, 12, 2)), info.name, None)
            except Exception as e:
                print(f"(paired-device lookup failed: {e})")
        return None

    async def _connect(self):
        from bleak import BleakClient
        dev = await self._find()
        if dev is None:
            raise ConnectionError("no Potahto found over Bluetooth (turn Bluetooth on in its Settings)")
        self.client = BleakClient(dev, disconnected_callback=lambda c: setattr(self, "lost", True))
        self.lost = False
        await self.client.connect(timeout=15)
        await self.client.start_notify(NUS_TX, self._rx)
        self.chunk = max(20, min(180, (self.client.mtu_size or 23) - 3))
        self.name = f"Bluetooth {dev.name or dev.address}"

    def _rx(self, _ch, data):
        self.buf += bytes(data)
        while b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            line = line.strip()
            if not line.startswith(b"{"):
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if r.get("event"):
                self.events.put(r)
            elif r.get("id") in self.waiting:
                self.waiting.pop(r["id"]).put(r)

    async def _write(self, data):
        for i in range(0, len(data), self.chunk):
            await self.client.write_gatt_char(NUS_RX, data[i:i + self.chunk], response=True)

    def call(self, cmd, **args):
        if self.lost:
            raise ConnectionError("Bluetooth link dropped")
        self.id += 1
        q = queue.Queue()
        self.waiting[self.id] = q
        self._run(self._write((json.dumps({"id": self.id, "cmd": cmd, **args}) + "\n").encode()), 5)
        try:
            return q.get(timeout=4)
        except queue.Empty:
            self.waiting.pop(self.id, None)
            raise TimeoutError("no answer from Potahto")

    def poll_events(self):
        out = []
        while not self.events.empty():
            out.append(self.events.get())
        return out

    def close(self):
        try:
            if self.client:
                self._run(self.client.disconnect(), 5)
        except Exception:
            pass
        self.loop.call_soon_threadsafe(self.loop.stop)


# ---- PC readings ------------------------------------------------------------------------
def gpu_reader():
    try:
        import pynvml
        pynvml.nvmlInit()
        h = pynvml.nvmlDeviceGetHandleByIndex(0)

        def read():
            u = pynvml.nvmlDeviceGetUtilizationRates(h).gpu
            t = pynvml.nvmlDeviceGetTemperature(h, pynvml.NVML_TEMPERATURE_GPU)
            return float(u), float(t)
        return read
    except Exception:
        return lambda: (-1.0, -1.0)


def cpu_temp():
    try:
        temps = psutil.sensors_temperatures()          # not available on Windows
        for key in ("coretemp", "k10temp", "cpu_thermal", "acpitz"):
            if temps.get(key):
                return float(temps[key][0].current)
    except Exception:
        pass
    return -1.0


def foreground_app():
    """Executable name of the app in front, e.g. 'Code.exe', or '' if unknown."""
    try:
        if sys.platform == "win32":
            import ctypes
            u32 = ctypes.windll.user32
            hwnd = u32.GetForegroundWindow()
            pid = ctypes.c_ulong()
            u32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            return psutil.Process(pid.value).name() if pid.value else ""
        if sys.platform == "darwin":
            out = subprocess.run(["osascript", "-e", 'tell application "System Events" to get name of first process whose frontmost is true'],
                                 capture_output=True, text=True, timeout=2)
            return out.stdout.strip()
        out = subprocess.run(["xdotool", "getactivewindow", "getwindowpid"], capture_output=True, text=True, timeout=2)
        return psutil.Process(int(out.stdout.strip())).name() if out.stdout.strip() else ""
    except Exception:
        return ""


def run_command(cmd, allow):
    if not allow:
        print(f"macro asked to run: {cmd!r}  (start with --allow-run to allow)")
        return
    print(f"running: {cmd}")
    kw = {"creationflags": 0x08000000} if sys.platform == "win32" else {"start_new_session": True}   # no console window
    try:
        subprocess.Popen(cmd, shell=True, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)
    except Exception as e:
        print(f"  failed: {e}")


# ---- main -------------------------------------------------------------------------------
def connect(a):
    if a.wifi:
        return WifiLink(a.wifi, a.pin)
    if a.ble is not None:
        return BleLink(a.ble, a.pin)
    return UsbLink(a.port)


def main():
    ap = argparse.ArgumentParser(description="Potahto PC companion")
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--ble", nargs="?", const="", metavar="NAME", help="use Bluetooth (optional: device name, default 'Potahto')")
    ap.add_argument("--wifi", metavar="HOST", help="use WiFi, e.g. potahto.local or 192.168.1.42")
    ap.add_argument("--pin", help="web PIN (WiFi, or an unpaired Bluetooth link)")
    ap.add_argument("--interval", type=float, default=1.0, help="seconds between updates")
    ap.add_argument("--allow-run", action="store_true", help="run {pc ...} macro commands on this PC")
    ap.add_argument("--no-app", action="store_true", help="don't report the foreground app")
    ap.add_argument("--notify", metavar="TEXT", help="send one notification and exit")
    ap.add_argument("--title", default=socket.gethostname(), help="notification title")
    a = ap.parse_args()

    if a.notify:
        link = connect(a)
        r = link.call("notify", title=a.title[:23], text=a.notify[:99], src="pc")
        print("sent" if r.get("ok") else f"failed: {r.get('err')}")
        return

    gpu = gpu_reader()
    host = socket.gethostname()[:23]
    link = None
    last_net = psutil.net_io_counters()
    last_t = time.time()
    last_app, app_sent, app_checked = None, 0.0, 0.0
    psutil.cpu_percent(None)
    if a.allow_run:
        print("--allow-run is on: {pc ...} macros will run commands on this PC")
    while True:
        try:
            if link is None:
                link = connect(a)
                link.call("time.set", epoch=time.time())
                print(f"connected via {link.name}")
                last_app = None
            # handle device events (several times per interval so {pc} feels instant)
            t_end = time.time() + a.interval
            while True:
                for ev in link.poll_events():
                    if ev.get("event") == "pc.run" and ev.get("cmd"):
                        run_command(ev["cmd"], a.allow_run)
                if not a.no_app and time.time() - app_checked >= 0.5:
                    app_checked = time.time()
                    fg = foreground_app()
                    if fg and (fg != last_app or time.time() - app_sent > 60):
                        link.call("pc.app", exe=fg[:31])
                        last_app, app_sent = fg, time.time()
                left = t_end - time.time()
                if left <= 0:
                    break
                time.sleep(min(0.25, left))
            now = time.time()
            net = psutil.net_io_counters()
            dt = max(0.001, now - last_t)
            up = (net.bytes_sent - last_net.bytes_sent) / 1024 / dt
            down = (net.bytes_recv - last_net.bytes_recv) / 1024 / dt
            last_net, last_t = net, now
            g_load, g_temp = gpu()
            link.call("pc.stats", host=host, cpu=psutil.cpu_percent(None), ram=psutil.virtual_memory().percent,
                      gpu=g_load, gpuT=g_temp, cpuT=cpu_temp(), up=round(up, 1), down=round(down, 1),
                      disk=psutil.disk_usage(os.path.abspath(os.sep)).percent)
        except KeyboardInterrupt:
            break
        except SystemExit:
            raise
        except Exception as e:
            print(f"link lost ({e}); retrying in 3 s")
            if isinstance(link, BleLink):
                link.close()
            link = None
            time.sleep(3)


if __name__ == "__main__":
    main()
