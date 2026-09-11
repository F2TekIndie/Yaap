"""Check the packaged tray against a private StatusNotifier host on niri.

Requires dbus-next. Run: python3 tests/smoke/TraySessionSmoke.py distribution/linux/Yaap
"""
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from dbus_next import Variant
from dbus_next.aio import MessageBus
from dbus_next.service import ServiceInterface, method, signal, dbus_property
from dbus_next.constants import PropertyAccess

WATCHER = "org.kde.StatusNotifierWatcher"

class Watcher(ServiceInterface):
    def __init__(self):
        super().__init__(WATCHER)
        self.items = []

    @method()
    def RegisterStatusNotifierItem(self, service: 's'):
        if service not in self.items:
            self.items.append(service)
            self.StatusNotifierItemRegistered(service + "/StatusNotifierItem")

    @method()
    def RegisterStatusNotifierHost(self, service: 's'):
        pass

    @dbus_property(access=PropertyAccess.READ)
    def ProtocolVersion(self) -> 'i':
        return 0

    @dbus_property(access=PropertyAccess.READ)
    def RegisteredStatusNotifierItems(self) -> 'as':
        return [item + "/StatusNotifierItem" for item in self.items]

    @dbus_property(access=PropertyAccess.READ)
    def IsStatusNotifierHostRegistered(self) -> 'b':
        return True

    @signal()
    def StatusNotifierItemRegistered(self, item: 's') -> 's':
        return item

    @signal()
    def StatusNotifierHostRegistered(self):
        pass

async def run(*command):
    proc = await asyncio.create_subprocess_exec(*command, stdout=asyncio.subprocess.PIPE,
                                               stderr=asyncio.subprocess.PIPE)
    out, err = await proc.communicate()
    if proc.returncode:
        raise RuntimeError(err.decode())
    return out.decode()

async def wait_for(predicate, label):
    for _ in range(100):
        result = predicate()
        if asyncio.iscoroutine(result):
            result = await result
        if result:
            return result
        await asyncio.sleep(0.1)
    raise AssertionError("Timed out: " + label)

async def main(executable):
    assert os.environ.get("NIRI_SOCKET"), "Run this smoke test in a niri session"
    bus = await MessageBus().connect()
    watcher = Watcher()
    bus.export("/StatusNotifierWatcher", watcher)
    await bus.request_name(WATCHER)
    with tempfile.TemporaryDirectory(prefix="yaap-tray-smoke-") as temporary:
        root = Path(temporary)
        env = dict(os.environ, XDG_CONFIG_HOME=str(root / "config"),
                   XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"),
                   QT_QPA_PLATFORM="wayland",
                   QT_QPA_PLATFORMTHEME=os.environ.get("YAAP_TRAY_TEST_PLATFORM_THEME", "generic"),
                   QT_QUICK_BACKEND="software")
        with (root / "app.log").open("w+") as log:
            app = await asyncio.create_subprocess_exec(executable, "--background", env=env,
                                                       stdout=log, stderr=log)
            try:
                await wait_for(lambda: watcher.items, "tray registration")
                service = watcher.items[0]
                tree = await bus.introspect(service, "/StatusNotifierItem")
                item = bus.get_proxy_object(service, "/StatusNotifierItem", tree)
                properties = item.get_interface("org.freedesktop.DBus.Properties")
                tray = item.get_interface("org.kde.StatusNotifierItem")
                assert not (await properties.call_get("org.kde.StatusNotifierItem", "ItemIsMenu")).value
                icon = await properties.call_get("org.kde.StatusNotifierItem", "IconPixmap")
                icon_name = await properties.call_get("org.kde.StatusNotifierItem", "IconName")
                assert icon.value or icon_name.value, "Tray has no displayable icon"
                menu_path = (await properties.call_get("org.kde.StatusNotifierItem", "Menu")).value
                menu_tree = await bus.introspect(service, menu_path)
                menu = bus.get_proxy_object(service, menu_path, menu_tree).get_interface("com.canonical.dbusmenu")
                _, layout = await menu.call_get_layout(0, -1, [])
                entries = {}
                for child in layout[2]:
                    node = child.value
                    label = node[1].get("label", Variant('s', '')).value
                    if label:
                        entries[label] = node[0]
                assert set(entries) == {"Open", "Open miniplayer", "Quit"}, entries

                async def windows():
                    return [w for w in json.loads(await run("niri", "msg", "-j", "windows"))
                            if w.get("pid") == app.pid]
                async def closed():
                    return not await windows()
                async def close():
                    for window in await windows():
                        await run("niri", "msg", "action", "close-window", "--id", str(window["id"]))
                    await wait_for(closed, "window hides")
                async def click(label):
                    await menu.call_event(entries[label], "clicked", Variant('i', 0), 0)

                assert await closed()
                await tray.call_activate(0, 0)
                full = (await wait_for(windows, "left click opens full player"))[0]
                await close()
                await click("Open miniplayer")
                mini = (await wait_for(windows, "menu opens miniplayer"))[0]
                assert mini["layout"]["window_size"] == [480, 112], mini["layout"]
                await click("Open")
                async def full_again():
                    current = await windows()
                    return current and current[0]["layout"]["window_size"][1] >= 420
                await wait_for(full_again, "Open restores full player")
                await close()
                watcher.items.clear()
                await bus.release_name(WATCHER)
                await asyncio.sleep(0.3)
                await bus.request_name(WATCHER)
                watcher.StatusNotifierHostRegistered()
                await wait_for(lambda: watcher.items, "tray re-registers after host restart")
                # Re-registration may replace the tray service and menu objects.
                service = watcher.items[0]
                tree = await bus.introspect(service, "/StatusNotifierItem")
                item = bus.get_proxy_object(service, "/StatusNotifierItem", tree)
                properties = item.get_interface("org.freedesktop.DBus.Properties")
                menu_path = (await properties.call_get("org.kde.StatusNotifierItem", "Menu")).value
                tree = await bus.introspect(service, menu_path)
                menu = bus.get_proxy_object(service, menu_path, tree).get_interface("com.canonical.dbusmenu")
                _, layout = await menu.call_get_layout(0, -1, [])
                quit_id = next(child.value[0] for child in layout[2]
                               if child.value[1].get("label", Variant('s', '')).value == "Quit")
                await menu.call_event(quit_id, "clicked", Variant('i', 0), 0)
                assert await asyncio.wait_for(app.wait(), 10) == 0
                assert await closed()
                print("PASS: tray registration, exported menu, left click, full/miniplayer actions, host restart, Quit")
            except BaseException:
                log.flush()
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if app.returncode is None:
                    app.terminate()
                    await asyncio.wait_for(app.wait(), 5)
    bus.disconnect()

if __name__ == "__main__":
    executable = str(Path(sys.argv[1]).resolve())
    if "--inside-session" not in sys.argv:
        sys.exit(subprocess.call(["dbus-run-session", "--", sys.executable,
                                  str(Path(__file__).resolve()), executable, "--inside-session"]))
    asyncio.run(main(executable))
