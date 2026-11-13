# AI Passport GB 真机测试说明

面向手持 **FoloToy AI Passport** 板（ESP32-C3 160 MHz、无 PSRAM、8 MiB 外置 Flash、ST7789 240×320、三键 GPIO0 ADC 分压）的 DMG 实机验收文档。

## 烧录

1. 将 USB 连接电脑，确认串口（Linux 多为 `/dev/ttyACM0`，macOS 多为 `/dev/cu.usbmodem*`）。
2. 优先让 USB Serial/JTAG 和 esptool 自动进入下载模式；若失败，按该批次板卡说明使用 **BOOT** 后复位。ESP32-C3 的下载引脚是 GPIO9，面板 **上** 键所在的 GPIO0 ADC 电阻梯不能代替 BOOT。参见 [Espressif 启动模式说明](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/advanced-topics/boot-mode-selection.html)。
3. 使用发布包中的 **合并镜像**（推荐）：

```bash
esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \
  0x0 pattern-merged.bin
```

将 `pattern-merged.bin` 换成对应 ROM 名称（`dmg-acid2-merged.bin`、`galactic-armada-merged.bin`、`rubik-merged.bin`、`mooneye-daa-merged.bin` 等）。各文件 SHA-256 见包内 `*.sha256` 清单。

完整设备包同时包含 `LICENSE`、`notices/` 和 `sources/`，包括 Rubik 与 HomebrewOwl.GB 的固定源码和构建说明。复制设备包时保留这些材料；在包根目录运行 `sha256sum -c manifest.sha256` 可核对全部发布文件。

4. 分块烧录（可选）：

```bash
esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \
  0x0 pattern-bootloader.bin \
  0x8000 pattern-partition-table.bin \
  0x10000 pattern-app.bin
```

## 采集内容

| 项目 | 要求 |
| --- | --- |
| UART 日志 | 上电后 **≥60 秒** 完整串口输出（115200，USB-JTAG CDC） |
| 屏幕照片 | 正对屏幕、光线充足，需看清四阶灰度 |
| 按键 | 依次短按 **上 / 下 / 确定**，注明是否有反应 |
| 备注 | 板子批次、烧录的 ROM 文件名、串口设备名 |

## 通过标准（Checklist）

- [ ] 启动日志中 `gb_boot` 显示 `flash_bytes=8388608`（8 MiB）。
- [ ] 表中 **验收帧** 的 `gb_hash sha256=` 与期望值 **完全一致**（shade buffer，非 PNG；与 `tools/test_sim_roms.sh` / `GB_HASH_FRAME` 一致）。
- [ ] 画面四阶灰 **由浅到深** 自然，**无** 整体发绿/发洋红（若有则怀疑 RGB565 字节序需调换）。
- [ ] 80 MHz SPI 刷新无可见撕裂、闪烁。
- [ ] 三键 ADC 分压响应正常（松开导通应为高阻/高电压档）。
- [ ] 运行中出现 `gb_perf` 行，记录 `fps` 与 `frame_us_avg`（实机 `esp_timer` 测量；与 passportsim 虚拟时间无关）。

### 各 ROM 验收帧与期望 `gb_hash`

与维护者包内 `sim/*-expected-gb_hash.txt` 及 `sim/*-expected-gb_hash-frame.txt` 一致（由 `./tools/build_device_pack.sh` 生成）。

| ROM | 验收帧 | 期望 SHA-256（shade） |
| --- | ---: | --- |
| pattern.gb | 3 | `b272c10c352f8ae1793b177f5fe08eb6de29d6c77d4f251bf3b8416602642743` |
| dmg-acid2.gb | 10 | `f844ea760a6f1fe137f7f992c7ab1c72d34c7fcd3a807b4174a78eb04a32a458` |
| GalacticArmada.gb | 90 | `1465249ea7857719a735a361069c29ad661bdc1c6b963a90852aa6cd5b5d1dc0` |
| rubik.gb | 160 | `3c40d8633d138936589645dca803918ee5a51bde4b18e6a5e546fba241a95cc2` |
| mooneye/daa.gb | 23 | `ae6bfbe8094f4162964e5c2fa4c04846546adff1ea97a0059946569c155dc885` |

各 ROM 哈希 **互不相同**，用于区分烧录的镜像与卡带。

带 `GB_INPUT_AT` 的镜像（Galactic Armada、Rubik）在 `GB_INPUT_AT..GB_HASH_FRAME` 范围内会**自动**施加 `GB_OK_JOY`，模拟器和真机都会如此，使 `gb_hash` 与 `gb-host --hold` 一致。自动输入不能证明实体按键有效：待验收帧之后再测试按键，或使用 `GB_INPUT_AT=0` 的镜像单独验证。

`gb_perf` 示例（每 60 帧一行）：

```text
gb_perf frames=60 emu_us_avg=… emu_us_max=… present_us_avg=… present_us_max=… frame_us_avg=… fps=… heap_min=…
```

## 结果填写模板

| 字段 | 内容 |
| --- | --- |
| 测试人 | |
| 日期 | |
| ROM / 镜像 SHA-256 前缀 | |
| `gb_boot flash_bytes` | |
| 验收帧号 | |
| `gb_hash`（实测） | |
| `gb_perf` fps（任意一行） | |
| `gb_perf` frame_us_avg（µs） | |
| 灰阶/色偏 | 通过 / 绿偏 / 洋红偏 / 其他 |
| 撕裂/闪烁 | 无 / 有 |
| 按键 | 上 / 下 / 确定 |
| 备注 | |

## 说明

- 固件 **不含** Blargg 二进制；`cpu_instrs` 仅维护者在本地 `tools/fetch_test_roms.sh` 后自行构建，**不得** 提交或分发 ROM 文件。
- `passportsim` 仅用于 CI/维护者预检；其 `emu_us` 为访客虚拟时钟，**不能** 代替上表实机性能。
- `GB_RUN_FRAMES=0` 的设备包持续运行，当前存档只在有限帧运行结束时写入 NVS；持续运行包尚无手动保存入口，不能据此宣称掉电存档已验收。
