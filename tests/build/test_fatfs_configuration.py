"""Verify target FAT settings that implement SD-009 and SD-010."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_fatfs_defaults_support_normative_long_names_and_locking() -> None:
    defaults = (ROOT / "sdkconfig.defaults").read_text(encoding="utf-8")

    assert "CONFIG_FATFS_CODEPAGE_437=y" in defaults
    assert "CONFIG_FATFS_LFN_HEAP=y" in defaults
    assert "CONFIG_FATFS_MAX_LFN=255" in defaults
    assert "CONFIG_FATFS_LFN_NONE=y" not in defaults
    assert "CONFIG_FATFS_FS_LOCK=16" in defaults
    assert "CONFIG_FATFS_SECTOR_4096=y" in defaults
    assert "CONFIG_FATFS_TIMEOUT_MS=10000" in defaults
