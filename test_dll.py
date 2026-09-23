# -*- coding: utf-8 -*-
import ctypes
import json
from pathlib import Path

DLL_PATH = Path.home() / "source" / "repos" / "Dll1" / "x64" / "Debug" / "Dll1.dll"
JSON_PATH = Path.home() / "source" / "repos" / "Dll1" / "network_state.json"

print(f"DLL: {DLL_PATH}")
print(f"JSON: {JSON_PATH}")
print(f"JSON exists: {JSON_PATH.exists()}")

lib = ctypes.CDLL(str(DLL_PATH))

lib.net_init_with_path.argtypes = [ctypes.c_char_p]
lib.net_init_with_path.restype = None

lib.net_tick.argtypes = []
lib.net_tick.restype = ctypes.c_int

lib.net_get_state_json.argtypes = []
lib.net_get_state_json.restype = ctypes.c_char_p

# Передаём путь к JSON как строку байт
lib.net_init_with_path(str(JSON_PATH).encode("cp1251"))
print("Network initialized with explicit path")

raw = lib.net_get_state_json()
state = json.loads(raw.decode("utf-8"))
print(f"Hour: {state['hour']}, Day: {state['day']}, Nodes: {len(state['nodes'])}")
for node in state['nodes']:
    print(f"  {node['ip']} | type={node['type']} | load={node['current_load']}")