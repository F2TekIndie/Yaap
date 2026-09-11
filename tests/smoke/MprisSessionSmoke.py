"""Exercise the packaged app on an isolated session bus.

python3 tests/smoke/MprisSessionSmoke.py distribution/linux/Yaap [--niri]
The optional niri checks close only windows belonging to the test process.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import wave


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--niri", action="store_true")
    parser.add_argument("--inside-session", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    executable = str(args.executable.resolve())
    if not args.inside_session:
        return subprocess.call([
            "dbus-run-session", "--", sys.executable, str(Path(__file__).resolve()),
            executable, "--inside-session", *(["--niri"] if args.niri else [])])
    if args.niri and not os.environ.get("NIRI_SOCKET"):
        raise RuntimeError("--niri requires a running niri session")

    with tempfile.TemporaryDirectory(prefix="yaap-mpris-smoke-") as temporary:
        root = Path(temporary)
        env = dict(os.environ, XDG_CONFIG_HOME=str(root / "config"),
                   XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"),
                   QT_QPA_PLATFORM="wayland" if args.niri else "offscreen",
                   QT_QPA_PLATFORMTHEME="generic", QT_QUICK_BACKEND="software")
        media = root / "session-test.wav"
        with wave.open(str(media), "wb") as audio:
            audio.setnchannels(2)
            audio.setsampwidth(2)
            audio.setframerate(48000)
            audio.writeframes(b"\0" * 48000 * 4 * 30)

        def run(command):
            return subprocess.check_output(command, env=env, text=True, stderr=subprocess.STDOUT,
                                           timeout=10).strip()

        def dbus(method, *values):
            return run(["gdbus", "call", "--session", "--dest", "org.mpris.MediaPlayer2.Yaap",
                        "--object-path", "/org/mpris/MediaPlayer2", "--method", method, *values])

        def prop(name):
            return dbus("org.freedesktop.DBus.Properties.Get", "org.mpris.MediaPlayer2.Player", name)

        def wait_for(condition, label):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                try:
                    if condition():
                        return
                except subprocess.CalledProcessError:
                    pass
                time.sleep(0.05)
            raise AssertionError("Timed out: " + label)

        def position():
            return int(re.search(r"int64 (\d+)", prop("Position"))[1])

        def track_id():
            return re.search(r"objectpath '([^']+)'", prop("Metadata"))[1]

        with (root / "application.log").open("w+") as log:
            app = subprocess.Popen([executable, "--background"], env=env, stdout=log, stderr=log)
            try:
                wait_for(lambda: "Stopped" in prop("PlaybackStatus"), "MPRIS registration")

                def windows():
                    return [item for item in json.loads(run(["niri", "msg", "-j", "windows"]))
                            if item.get("pid") == app.pid]

                if args.niri:
                    assert not windows(), "Background startup exposed a window"
                run([executable])
                assert app.poll() is None, "Second launch terminated the primary instance"
                if args.niri:
                    wait_for(lambda: len(windows()) == 1, "relaunch activates one window")

                dbus("org.mpris.MediaPlayer2.Player.OpenUri", media.as_uri())
                wait_for(lambda: "Playing" in prop("PlaybackStatus") and "true" in prop("CanSeek"),
                         "local playback")
                assert "false" in prop("CanGoNext")
                dbus("org.freedesktop.DBus.Properties.Set", "org.mpris.MediaPlayer2.Player",
                     "Volume", "<0.23>")
                assert "0.23" in prop("Volume")
                dbus("org.mpris.MediaPlayer2.Player.Pause")
                wait_for(lambda: "Paused" in prop("PlaybackStatus"), "pause")
                original_id = track_id()
                original_position = position()
                dbus("org.mpris.MediaPlayer2.Player.SetPosition", "/stale", "5000000")
                time.sleep(0.1)
                assert position() == original_position, "Stale track ID changed playback position"
                dbus("org.mpris.MediaPlayer2.Player.SetPosition", original_id, "5000000")
                wait_for(lambda: "Paused" in prop("PlaybackStatus") and position() >= 5000000,
                         "seek while paused")
                assert track_id() == original_id, "Seeking changed the track ID"
                dbus("org.mpris.MediaPlayer2.Player.Play")
                wait_for(lambda: "Playing" in prop("PlaybackStatus"), "resume")

                if args.niri:
                    run(["niri", "msg", "action", "close-window", "--id", str(windows()[0]["id"])])
                    wait_for(lambda: not windows(), "close hides the window")
                    before = position()
                    wait_for(lambda: position() > before + 100000, "playback continues hidden")
                    assert app.poll() is None
                    run([executable])
                    wait_for(lambda: len(windows()) == 1, "hidden instance reactivates")

                run([executable, "--quit"])
                assert app.wait(timeout=10) == 0
                if args.niri:
                    settings = root / "config/Yaap/Yaap.conf"
                    settings.write_text("[application]\nkeepPlayingInBackground=false\n")
                    app = subprocess.Popen([executable], env=env, stdout=log, stderr=log)
                    wait_for(lambda: len(windows()) == 1, "foreground startup")
                    run(["niri", "msg", "action", "close-window", "--id", str(windows()[0]["id"])])
                    assert app.wait(timeout=10) == 0, "Close should quit when background mode is off"
                print("PASS: MPRIS playback, volume, stale-ID rejection, seek, single-instance activation, Quit"
                      + (", niri hide/resume/close preference" if args.niri else ""))
            except BaseException:
                log.flush()
                log.seek(0)
                print(log.read(), file=sys.stderr)
                raise
            finally:
                if app.poll() is None:
                    app.terminate()
                    try:
                        app.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        app.kill()
                        app.wait()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
