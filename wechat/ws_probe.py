"""探查 9999 端口的 WebSocket 协议"""
import asyncio
import json
import sys

try:
    import websockets
except ImportError:
    print("Installing websockets...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "websockets"])
    import websockets


async def probe():
    uri = "ws://127.0.0.1:9999"
    print(f"Connecting to {uri}")
    try:
        async with websockets.connect(uri, max_size=None) as ws:
            print("Connected!")
            # Send hello
            hello = {"command": "WMP.connection", "data": {}}
            await ws.send(json.dumps(hello))
            print(f"Sent: {hello}")

            # Listen for messages
            for i in range(20):
                try:
                    msg = await asyncio.wait_for(ws.recv(), timeout=3.0)
                    if isinstance(msg, bytes):
                        print(f"[{i}] RECV bytes ({len(msg)}B): {msg[:200]}")
                    else:
                        print(f"[{i}] RECV: {msg[:500]}")
                except asyncio.TimeoutError:
                    print(f"[{i}] timeout")
                    break
    except Exception as e:
        print(f"Error: {e}")


asyncio.run(probe())
