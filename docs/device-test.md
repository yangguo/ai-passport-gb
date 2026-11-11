# AI Passport GB 真机测试说明

面向手持 **FoloToy AI Passport** 板（ESP32-C3 160 MHz、无 PSRAM、8 MiB 外置 Flash、ST7789 240×320、三键 GPIO0 ADC 分压）的 DMG 实机验收文档。

## 烧录

1. 将 USB 连接电脑，确认串口（Linux 多为 `/dev/ttyACM0`，macOS 多为 `/dev/cu.usbmodem*`）。
2. 进入下载模式：按住 **上** 键上电，或按板卡说明进入 Boot。
3. 使用发布包中的 **合并镜像**（推荐）：

```bash
esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \
  0x0 pattern-merged.bin
```

将 `pattern-merged.bin` 换成对应 ROM 名称（`dmg-acid2-merged.bin`、`galactic-armada-merged.bin`、`rubik-merged.bin`、`mooneye-daa-merged.bin` 等）。各文件 SHA-256 见包内 `*.sha256` 清单。

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
- [ ] 第 3 帧 `gb_hash sha256=` 与下表 **完全一致**（shade buffer，非 PNG）。
- [ ] 画面四阶灰 **由浅到深** 自然，**无** 整体发绿/发洋红（若有则怀疑 RGB565 字节序需调换）。
- [ ] 80 MHz SPI 刷新无可见撕裂、闪烁。
- [ ] 三键 ADC 分压响应正常（松开导通应为高阻/高电压档）。
- [ ] 运行中出现 `gb_perf` 行，记录 `fps` 与 `frame_us_avg`（实机 `esp_timer` 测量；与 passportsim 虚拟时间无关）。

### 各 ROM 期望 `gb_hash`（第 3 帧）

| ROM | 期望 SHA-256（shade，固定第 3 帧） |
| --- | --- |
| pattern.gb | `b272c10c352f8ae1793b177f5fe08eb6de29d6c77d4f251bf3b8416602642743` |
| dmg-acid2.gb | `46e2096b907947368d310929303a04005b39c4a278e3a7de2225c355b4522694` |
| GalacticArmada.gb | `46e2096b907947368d310929303a04005b39c4a278e3a7de2225c355b4522694` |
| rubik.gb | `46e2096b907947368d310929303a04005b39c4a278e3a7de2225c355b4522694` |
| mooneye/daa.gb | `46e2096b907947368d310929303a04005b39c4a278e3a7de2225c355b4522694` |

第 3 帧时多数可卡带仍处于前导画面/空白 LCD 缓冲，故后四行哈希相同属预期；`pattern.gb` 为专用测试 ROM，哈希独立。

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
