"""BlueZ D-Bus downlink advertiser for sleep updates."""

# pyright: reportUndefinedVariable=false, reportInvalidTypeForm=false

from __future__ import annotations

import asyncio
from dataclasses import dataclass
import subprocess

from dbus_next import Variant
from dbus_next.aio import MessageBus
from dbus_next.constants import BusType, PropertyAccess
from dbus_next.errors import DBusError
from dbus_next.service import ServiceInterface, dbus_property, method

from core.config import DOWNLINK_ADV_DEVICE_ID, DOWNLINK_ADV_NAME

ADV_PATH = "/org/lrima/downlink_adv0"
DEFAULT_DOWNLINK_DURATION_MS = 250
DEFAULT_DOWNLINK_BURST_COUNT = 5
DEFAULT_DOWNLINK_INTER_BURST_MS = 120


@dataclass
class DownlinkFrame:
    target_node_id: int
    sleep_duration_sec: int

    def encode_vendor_payload(self) -> bytes:
        target = int(self.target_node_id) & 0xFFFF
        sleep_value = int(self.sleep_duration_sec) & 0xFFFFFFFF
        return bytes(
            [
                target & 0xFF,
                (target >> 8) & 0xFF,
                sleep_value & 0xFF,
                (sleep_value >> 8) & 0xFF,
                (sleep_value >> 16) & 0xFF,
                (sleep_value >> 24) & 0xFF,
            ]
        )


class _Advertisement(ServiceInterface):
    def __init__(self, payload: bytes, adv_type: str = "peripheral", local_name: str | None = DOWNLINK_ADV_NAME):
        super().__init__("org.bluez.LEAdvertisement1")
        self._payload = payload
        self._adv_type = adv_type
        self._local_name = local_name

    @method()
    def Release(self):
        return

    @dbus_property(access=PropertyAccess.READ)
    def Type(self) -> "s":
        return self._adv_type

    @dbus_property(access=PropertyAccess.READ)
    def LocalName(self) -> "s":
        return self._local_name or ""

    @dbus_property(access=PropertyAccess.READ)
    def ManufacturerData(self) -> "a{qv}":
        return {DOWNLINK_ADV_DEVICE_ID: Variant("ay", self._payload)}


class BluezDownlinkAdvertiser:
    def __init__(self, adapter: str | None = None, scan_adapter: str | None = None) -> None:
        self._adapter = self._select_advertising_adapter(adapter, scan_adapter)
        self._bus: MessageBus | None = None
        self._manager = None
        self._lock = asyncio.Lock()

    def _select_advertising_adapter(self, preferred_adapter: str | None, scan_adapter: str | None) -> str:
        if preferred_adapter:
            return preferred_adapter

        adapters: list[str] = []
        try:
            result = subprocess.run(["hciconfig"], stdout=subprocess.PIPE, text=True, check=True)
            for line in result.stdout.splitlines():
                line = line.strip()
                if not line.startswith("hci"):
                    continue
                adapter_name = line.split(":", 1)[0]
                adapters.append(adapter_name)
        except Exception:
            pass

        for adapter_name in adapters:
            if scan_adapter and adapter_name == scan_adapter:
                continue
            return adapter_name

        if scan_adapter:
            print(
                f"[Downlink Adapter Selection] No alternate adapter found, using scan adapter {scan_adapter}"
            )
            return scan_adapter

        return "hci0"

    async def _ensure_bus(self) -> None:
        if self._bus is not None:
            return

        self._bus = await MessageBus(bus_type=BusType.SYSTEM).connect()

    async def _get_manager(self, adapter: str):
        await self._ensure_bus()
        assert self._bus is not None

        if self._manager is not None:
            return self._manager

        adapter_path = f"/org/bluez/{adapter}"
        introspection = await self._bus.introspect("org.bluez", adapter_path)
        obj = self._bus.get_proxy_object("org.bluez", adapter_path, introspection)
        manager = obj.get_interface("org.bluez.LEAdvertisingManager1")
        self._manager = manager
        return manager

    async def _register_and_pulse(self, manager, ad: _Advertisement, duration_ms: int) -> bool:
        assert self._bus is not None

        self._bus.export(ADV_PATH, ad)
        await manager.call_register_advertisement(ADV_PATH, {})
        await asyncio.sleep(max(duration_ms, 50) / 1000.0)
        await manager.call_unregister_advertisement(ADV_PATH)
        self._bus.unexport(ADV_PATH)
        return True

    async def emit_sleep_update(
        self,
        target_node_id: int,
        sleep_duration_sec: int,
        duration_ms: int = DEFAULT_DOWNLINK_DURATION_MS,
        burst_count: int = DEFAULT_DOWNLINK_BURST_COUNT,
        inter_burst_ms: int = DEFAULT_DOWNLINK_INTER_BURST_MS,
    ) -> bool:
        frame = DownlinkFrame(target_node_id=target_node_id, sleep_duration_sec=sleep_duration_sec)
        payload = frame.encode_vendor_payload()

        if burst_count < 1:
            burst_count = 1
        if inter_burst_ms < 0:
            inter_burst_ms = 0

        async with self._lock:
            last_error = None
            sent_once = False
            adapter = self._adapter
            try:
                manager = await self._get_manager(adapter)
            except (DBusError, OSError, AssertionError) as exc:
                print(f"[Downlink Adapter Unavailable] adapter={adapter} detail={exc}")
                return False

            for adv_type, local_name in (("peripheral", "LRIMa GW"), ("broadcast", None)):
                for burst_index in range(burst_count):
                    ad = _Advertisement(payload=payload, adv_type=adv_type, local_name=local_name)
                    try:
                        await self._register_and_pulse(manager, ad, duration_ms)
                        sent_once = True
                    except DBusError as exc:
                        last_error = exc
                        err_name = getattr(exc, "type", "unknown")
                        err_text = getattr(exc, "text", str(exc))
                        print(
                            f"[Downlink Advertisement Register Failed] adapter={adapter} type={adv_type} "
                            f"burst={burst_index + 1}/{burst_count} error={err_name} detail={err_text}"
                        )
                    except OSError as exc:
                        last_error = exc
                        print(
                            f"[Downlink Advertisement OSError] adapter={adapter} type={adv_type} "
                            f"burst={burst_index + 1}/{burst_count} detail={exc}"
                        )
                    finally:
                        try:
                            await manager.call_unregister_advertisement(ADV_PATH)
                        except Exception:
                            pass
                        try:
                            if self._bus is not None:
                                self._bus.unexport(ADV_PATH)
                        except Exception:
                            pass

                    if burst_index + 1 < burst_count and inter_burst_ms > 0:
                        await asyncio.sleep(inter_burst_ms / 1000.0)

                if sent_once:
                    return True

            if last_error is not None:
                print(f"[Downlink Advertisement Error] {last_error}")
            return sent_once
