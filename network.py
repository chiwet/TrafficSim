import ctypes
import json
from pathlib import Path

DLL_PATH = Path.home() / "source" / "repos" / "Dll1" / "x64" / "Debug" / "Dll1.dll"
JSON_PATH = Path.home() / "source" / "repos" / "Dll1" / "network_state.json"


class NetworkWrapper:
    def __init__(self):
        if not DLL_PATH.exists():
            raise FileNotFoundError(f"DLL not found: {DLL_PATH}")

        self.lib = ctypes.CDLL(str(DLL_PATH))

        # init
        self.lib.net_init_with_path.argtypes = [ctypes.c_char_p]
        self.lib.net_init_with_path.restype = None

        # tick
        self.lib.net_tick.argtypes = []
        self.lib.net_tick.restype = ctypes.c_int

        # get state
        self.lib.net_get_state_json.argtypes = []
        self.lib.net_get_state_json.restype = ctypes.c_char_p

        # add node: type, ip, capacity, parent_ip
        self.lib.net_add_node.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
        self.lib.net_add_node.restype = ctypes.c_int

        # remove node
        self.lib.net_remove_node.argtypes = [ctypes.c_char_p]
        self.lib.net_remove_node.restype = ctypes.c_int

        # save
        self.lib.net_save_state.argtypes = [ctypes.c_char_p]
        self.lib.net_save_state.restype = ctypes.c_int

        # cp1251 Ч важно дл€ C++ std::ifstream
        self.lib.net_init_with_path(str(JSON_PATH).encode("cp1251"))

    def tick(self) -> int:
        return self.lib.net_tick()

    def get_state(self) -> dict:
        raw = self.lib.net_get_state_json()
        return json.loads(raw.decode("utf-8"))

    def add_node(self, node_type: int, ip: str, capacity: int, parent_ip: str) -> int:
        return self.lib.net_add_node(
            node_type,
            ip.encode("utf-8"),
            capacity,
            parent_ip.encode("utf-8"),
        )

    def remove_node(self, ip: str) -> int:
        return self.lib.net_remove_node(ip.encode("utf-8"))

    def save(self) -> int:
        return self.lib.net_save_state(str(JSON_PATH).encode("cp1251"))