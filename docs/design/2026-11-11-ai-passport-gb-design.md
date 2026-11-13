# AI Passport GB 详细设计

日期：2026-11-11。状态：设计，供审阅。本轮不写模拟器代码、不刷机、不改分区、不合并。

后续状态（2026-11-13）：本文保留设计阶段的决策记录。现已加入主机／固件实现，以及经固定来源和许可证记录的测试 ROM；当前 ROM 范围以 [测试 ROM 说明](../test-roms.md) 为准。真机验收仍待完成。

目标：在 FoloToy AI Passport 上跑通原版 Game Boy（DMG）模拟器。顺序是先证明能跑，再测量，再决定要不要为流畅付出跳帧、缩放或音频。

## 1. 审阅摘要

- 核心用 Peanut-GB（MIT，单头文件，DMG）。它按行回调，像素是 2 bit 色号加调色板位，不是 RGB565，默认不含 APU。
- 板子是单核 ESP32-C3，最高 160 MHz，板上配置 8 MB flash、无 PSRAM；屏是 ST7789P3 240×320。这些是芯片手册和已锁定 BSP 的事实。模拟器能跑多少帧，现在没有任何本板测量。
- 引脚宏请求 SPI 80 MHz。同一上游历史上曾是 40 MHz，而且没有本板验收记录。设计不把 80 MHz 当成已经出现在线上的时钟。
- 现有 LVGL 代码用 40 行 DMA 缓冲（240×40×2 = 19200 字节）。硬件指南仍写 20 行 / 约 9.6 KB。以代码宏为准。GB 路径不启用 LVGL。
- 输入只有 GPIO0 上的三键电阻梯，不能同时按两键。用户提过的 8 个 GPIO 与现有占用全部冲突。
- 已决定的路径（2026-11-11，Vincent，第 14 节）：分区表不动，固件内嵌自制 ROM；画面 1:1 居中；音频关闭；三键加暂停菜单换层；仓库许可证 MIT。

## 2. 证据等级

文中每条数字属于且只属于下面一类。

| 标记 | 含义 |
| --- | --- |
| 事实 | 已读到的源码、数据手册或许可证文本。附路径或链接。 |
| 计算 | 由事实做的算术。给出式子。它不是示波器或帧率计的读数。 |
| 推断 | 由事实推出的工程判断。效果未测时，旁边标待测。 |
| 待确认 | 材料互相矛盾，或必须真机 / 首次构建才能知道。写明用哪条观测关闭它。 |
| 目标 | 希望达到的产品结果。在测到之前不是验收线。 |
| 设计选择 | 实现时采用的取舍。第 14 节的 8 项已于 2026-11-11 由 Vincent 定下。 |

不采用 PocketStar（另一块 ESP32-C3 小 OLED 掌机）的社区帧率。面板、分辨率、SPI 和固件都不同，本仓库也没有复现。

不把 RP2040-GB 的帧率按主频比例换算到 C3。

## 3. 来源与锁定版本

本轮阅读日期：2026-11-11。

| 来源 | 锁定 | 用途 |
| --- | --- | --- |
| [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport/tree/33d3d1d93a1125b356b47b6d83a7a60121be801e) `33d3d1d93a1125b356b47b6d83a7a60121be801e`（2026-10-04） | 与 [ai-passport-anki `IMPORT.md`](https://github.com/yangguo/ai-passport-anki/blob/09847914533da79e05325365eff65c700c4633ec/components/bsp/IMPORT.md)、[ai-passport-chess `docs/sources.md`](https://github.com/yangguo/ai-passport-chess/blob/7990267b0762a6f0b2d3ffe747bac968d1022170/docs/sources.md) 相同 | BSP 行为与分区的上游 |
| [bsp_pins.h @ 33d3d1d](https://github.com/FoloToy/ai-passport/blob/33d3d1d93a1125b356b47b6d83a7a60121be801e/components/bsp/include/bsp_pins.h) | 同上 | 引脚与 `BSP_LCD_PCLK_HZ` 的单一事实来源 |
| [AI_HARDWARE_DEVELOPMENT_GUIDE.md @ 33d3d1d](https://github.com/FoloToy/ai-passport/blob/33d3d1d93a1125b356b47b6d83a7a60121be801e/docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md) | 文内代码审计日期 2026-09-14 | 板级说明；与代码冲突时以代码为准 |
| [partitions.csv @ 33d3d1d](https://github.com/FoloToy/ai-passport/blob/33d3d1d93a1125b356b47b6d83a7a60121be801e/partitions.csv) | 同上 | 分区 |
| [sdkconfig.defaults @ 33d3d1d](https://github.com/FoloToy/ai-passport/blob/33d3d1d93a1125b356b47b6d83a7a60121be801e/sdkconfig.defaults) | 同上 | 目标芯片、8 MB、USB 控制台 |
| [yangguo/ai-passport-chess `docs/hardware.md`](https://github.com/yangguo/ai-passport-chess/blob/7990267b0762a6f0b2d3ffe747bac968d1022170/docs/hardware.md) | 文内核实日期 2026-10-06；仓库 HEAD `7990267b0762a6f0b2d3ffe747bac968d1022170`（2026-10-30） | 集成边界。其中 9600 字节与现行代码不一致，见 4.4 |
| [yangguo/ai-passport-anki](https://github.com/yangguo/ai-passport-anki/tree/09847914533da79e05325365eff65c700c4633ec) `09847914533da79e05325365eff65c700c4633ec`（2026-11-01） | BSP 副本 | 确认 40 行缓冲和按键 API 仍在 |
| [Peanut-GB](https://github.com/deltabeard/Peanut-GB/tree/d0bcca771c83a2638c93a9ae61f3d51f226dc905) `d0bcca771c83a2638c93a9ae61f3d51f226dc905`（2026-09-21） | `peanut_gb.h` | 核心 |
| [RP2040-GB](https://github.com/deltabeard/RP2040-GB/tree/3f366d79dc988f8c6bb884d7a56cc9972ac5a062) `3f366d79dc988f8c6bb884d7a56cc9972ac5a062`（2022-06-25） | README | 只作移植形态参考 |
| ESP32-C3 Series Datasheet v2.4 | Espressif，修订记录写明 2026-05-06 发布 v2.4 | 芯片事实 |
| ESP-IDF v5.5.3 文档与 `components/soc/esp32c3` 头文件 | 与上游硬件指南写的 IDF 5.5.3 对齐 | MMU、NVS、LCD API |

后续实现继续钉这些提交。不要跟随任一仓库的 `main` 漂移。

## 4. 已验证事实

### 4.1 芯片

ESP32-C3 Series Datasheet v2.4：

- 32 位 RISC-V 单核，ISA 为 RV32IMC，最高 160 MHz（§4.1.1.1）。产品概述另给 CoreMark：1 核 160 MHz 为 483.27 CoreMark，3.02 CoreMark/MHz。这是芯片基准，不能换算成 GB 帧率。
- SRAM 400 KB，其中 16 KB 用作 cache；另有 RTC SRAM 8 KB；ROM 384 KB（特性表，CPU and Memory）。
- 功能框图里的 USB 块是 USB Serial/JTAG。§2.3.4 把 GPIO18、GPIO19 标为该接口，且该接口只能用优先级 1 的固定脚。本文在该数据手册的功能列表里没有看到 USB Host。板上 USB-C 是下载和日志口，不是手柄口。
- §3：strapping 脚是 GPIO2、GPIO8、GPIO9。GPIO9 低电平进入 joint download；GPIO8 在该模式下必须为高；GPIO2 不决定 SPI/下载模式，但手册建议上拉。复位锁存之后这些脚可以当普通 GPIO。
- §2.3.4 优先级 3（谨慎）：GPIO2/8/9 strapping，GPIO18/19 USB，GPIO4/5/6/7 JTAG，GPIO20/21 UART0，GPIO11 默认是 flash 的 VDD_SPI。优先级 4（不建议挪作他用）：GPIO12–17 是片内 flash 的 SPI0/1。
- 有 DMA 的外设包括 SPI2、I2S、ADC（§4.1.1.2）。

ESP-IDF v5.5.3 `components/soc/esp32c3/include/soc/soc_caps.h`：

- `SOC_CPU_CORES_NUM` 为 1。
- `SOC_SHARED_IDCACHE_SUPPORTED` 为 1，注释写明指令和数据共用 cache。
- `SOC_CACHE_MEMORY_IBANK_SIZE` 为 `0x4000`（16 KiB）。与数据手册“16 KB for cache”一致。

`components/soc/esp32c3/include/soc/ext_mem_defs.h`：

- `SOC_MMU_ENTRY_NUM` 为 128。注释写明 I/D 共用 MMU 线性地址。
- `SOC_MMU_MAX_PADDR_PAGE_NUM` 为 256。同文件注释：256 × 64 KB = 16 MB，这是 MMU 能编址的物理上限，不是同时能映射的页数。
- 指令侧内部 SRAM 窗口 `0x4037C000`–`0x403E0000`（差 `0x64000` = 400 KiB）。数据侧窗口 `0x3FC80000`–`0x3FCE0000`（差 `0x60000` = 384 KiB）。这是同一块 SRAM 的两条总线窗口，不能相加。
- 指令 flash cache 窗口 `0x42000000`–`0x42800000`（8 MiB）。数据 flash cache 窗口 `0x3C000000`–`0x3C800000`（8 MiB）。

`components/hal/esp32c3/include/hal/mmu_ll.h` 注释：ESP32-C3 的 MMU 页大小固定 64 KB。

`components/esp_mm/include/esp_cache.h`（v5.5.3）对外能看到的是 `esp_cache_msync`。本轮没有找到公开的 cache 命中计数 API。观测方法见第 11 节，不用一个不存在的计数器。

上游 `sdkconfig.defaults` 设置 `CONFIG_IDF_TARGET="esp32c3"`，并注释 ESP32-C3 无 PSRAM、不设 `CONFIG_SPIRAM`。它没有设置 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ`。160 MHz 是芯片上限，不是这份 defaults 里已经写死的运行频率。

### 4.2 板级引脚

`bsp_pins.h` @ `33d3d1d`，anki `09847914` 与 chess `7990267` 的同名宏一致：

| 宏 | 值 |
| --- | --- |
| `BSP_LCD_W` / `BSP_LCD_H` | 240 / 320 |
| `BSP_LCD_SPI_HOST` | `SPI2_HOST` |
| `BSP_LCD_MOSI` / `SCLK` / `CS` / `DC` | GPIO9 / GPIO8 / GPIO1 / GPIO20 |
| `BSP_LCD_RST` | `-1`（软复位） |
| `BSP_LCD_BL` | GPIO21，LEDC |
| `BSP_LCD_PCLK_HZ` | `80 * 1000 * 1000` |
| `BSP_LCD_SPI_MODE` | 0 |
| `BSP_BTN_GPIO` | GPIO0，ADC1 通道 0，三键 |
| `BSP_BTN_SHORT_PRESS_MS` / `LONG_PRESS_MS` | 180 / 500 |
| `BSP_I2C_SDA` / `SCL` | GPIO10 / GPIO7 |
| `BSP_I2C_ES8311_ADDR` / `CW2017_ADDR` | `0x18` / `0x63` |
| `BSP_I2S_DOUT` / `DIN` / `BCLK` / `WS` / `MCLK` | GPIO2 / GPIO4 / GPIO5 / GPIO3 / GPIO6 |

`bsp_display.c` 把 `.pclk_hz` 设为 `BSP_LCD_PCLK_HZ`，`.max_transfer_sz = BSP_LCD_W * 80 * 2`。这是驱动请求的时钟和单次传输上限，不是测到的 SCLK。

硬件指南（英文与中文，同一提交）写 SPI2、80 MHz、mode 0、无 MISO。提交 `20668230f6064b1008911fcc975b6fd510c4b34d`（2026-09-20，标题 `docs(display): document 80 MHz LCD SPI clock`，是 `33d3d1d` 的祖先）把宏和指南从 40 MHz 改成 80 MHz。该提交信息没有测量数据。

chess `docs/hardware.md`（核实日期 2026-10-06）写明：源码与部分旧文档的 SPI 频率有 40/80 MHz 差异，设计不按旧文字写死速度，要在真机记录实际初始化值并验证稳定性。无 MISO、无触摸、无 TE。

控制台：`sdkconfig.defaults` 使用 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`，并注释 UART0 TX 默认 GPIO21 与背光冲突，因此禁用 UART 控制台。指南写 GPIO18/19 留给 USB Serial/JTAG。

按键电路（`bsp_pins.h` 注释与指南 §6）：3.3 V 经外部 10 kΩ 上到 GPIO0；UP 0 Ω、DOWN 1 kΩ、OK 2.2 kΩ 到地。窗口 `{0,150}`、`{150,447}`、`{447,1900}` mV，半开区间。不能改用内部上拉。

FoloToy 指南写 GPIO0 是 “boot-related pin”。数据手册 v2.4 §3 的 strapping 表只列 GPIO2、GPIO8、GPIO9。这两份材料对 GPIO0 的启动角色不一致。本设计只把 GPIO0 当成按键 ADC。不把它写成数据手册里的 strapping 脚。改这根脚之前要对照原理图和复位电平（待确认）。

`bsp_button.h`：`BSP_BTN_PRESS` 的注释是“按下瞬间（低延迟，适合游戏类即时响应）”；`BSP_BTN_CLICK` 是按下并抬起。回调跑在 button 组件的 `esp_timer` 任务里，只能入队。`bsp_button.c` 注释写 `adc_oneshot` 自带锁，`bsp_button_read_mv()` 与轮询可以交错。轮询在 1000 µs 内复用同一次平均采样。这些是代码注释里的机制，端到端延迟没有测量。

### 4.3 分区

`partitions.csv` @ `33d3d1d`，chess 与 anki 相同：

| 名 | 类型 | 偏移 | 大小 |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x9000` | `0x6000`（24576 字节） |
| `phy_init` | data/phy | `0xf000` | `0x1000`（4096 字节） |
| `factory` | app/factory | `0x10000` | `0x7F0000` |

`0x10000 + 0x7F0000 = 0x800000`。上游把 flash 配成 8 MB（`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`），分区表的末尾就是 `0x800000`。表里没有 ROM 分区，也没有 OTA。

这份 `sdkconfig.defaults` 没有改 `CONFIG_PARTITION_TABLE_OFFSET`。ESP-IDF v5.5.3 `components/partition_table/Kconfig.projbuild` 里该选项默认 `0x8000`。这与 nvs 从 `0x9000` 开始相符。首次构建后以 `flash_args` 为准。

上游 `docs/development/engineering/build-and-test.md` @ `33d3d1d`：合并的 `0x0` 全镜像可能重置 NVS；要保留 NVS 时用分段 `idf.py flash`。

### 4.4 显示缓冲：文档与代码不一致

同一提交 `33d3d1d` 的 `bsp_display_lvgl.c`：

- `BSP_LVGL_DRAW_BUFFER_LINES` 为 40。
- 注释：“C3 无 PSRAM，DMA 只能用内部 RAM。40 行单缓冲约 19.2KB”；`double_buffer = false`；`buff_dma = true`；`swap_bytes = true`（注释：LVGL 小端 RGB565，ST7789 要高字节在前）。

计算：`240 * 40 * 2 = 19200` 字节。注释里的 19.2 KB 是 `19200/1000`，不是 KiB。`19200/1024 = 18.75` KiB。

同一份硬件指南仍写“一个 `240 × 20` 的 RGB565 缓冲，约 9.6 KB”，LVGL 池 24 KB。`240 * 20 * 2 = 9600`。指南文首审计日期是 2026-09-14。把缓冲改成 40 行的提交是 `d0d8c1670dbdfbb1f31ef02be07de00e4d7699b1`（2026-09-20，`perf(display): improve LVGL refresh throughput`），指南里的 20 行句子没有跟着改。

chess `docs/hardware.md` 与 `docs/sources.md` 仍写 9600 字节，但 chess `bsp_display_lvgl.c` @ `7990267` 已是 40 行。anki @ `09847914` 同样是 40 行。

GB 固件不调用 `bsp_lvgl_init()`，因此不分配这 19200 字节，也不占用 24 KB LVGL 池。引用这两笔只是为了说明现有 UI 路径的 RAM 开销，以及哪份文字过时。

`bsp_display.h` 的 `BSP_LVGL_SCREEN_RADIUS` 为 30。指南说圆角遮罩打在 LVGL flush 上。不用 LVGL 时，软件遮罩不存在；玻璃本身的圆角要真机看。

### 4.5 Peanut-GB

`peanut_gb.h` @ `d0bcca77`，文件头是 MIT，版权 2018–2023 Mahyar Koshkouei。头文件声明其中两处来自 SameBoy，并写明摘取时 SameBoy 为 MIT，版权 2015–2019 Lior Halphon。本设计只原样收录 Peanut-GB，不另收录今天的 SameBoy。GitHub 对 LIJI32/SameBoy 的 license API 返回 `NOASSERTION`，所以不把“当前 SameBoy 仍是 MIT”写成事实。

行为事实：

- 只支持小端。大端会 `#error`。x86_64 主机和 ESP32-C3 都是小端。
- `ENABLE_SOUND` 默认 0。注释写明声音必须由外部库提供 `audio_read` / `audio_write`。
- `ENABLE_LCD` 默认 1。`PEANUT_GB_12_COLOUR` 默认 1。`PEANUT_GB_HIGH_LCD_ACCURACY` 默认 1。
- `LCD_WIDTH` 160，`LCD_HEIGHT` 144，`LCD_VERT_LINES` 154，`LCD_LINE_CYCLES` 456。因此 `LCD_FRAME_CYCLES = 456 * 154 = 70224`。
- `gb_frame` 的注释写明一帧对应 `0.016742706298828125` 秒。该小数等于 `70224/4194304`。`4194304` 出现在同仓库 `examples/sdl2/minigb_apu/minigb_apu.h` 的 `DMG_CLOCK_FREQ`。
- 内嵌数组：`wram[0x2000]`、`vram[0x2000]`、`oam[0xA0]`、`hram_io[0x100]`。计算：`8192+8192+160+256 = 16800` 字节。`sizeof(struct gb_s)` 含对齐和位域，要在 P0 编译后打印，现在不猜。
- cart RAM 不在 `struct gb_s` 里，由前端提供。`gb_get_save_size_s()` 的表是 0、`0x800`、`0x2000`、`0x8000`、`0x20000`、`0x10000` 字节。MBC2 固定返回 `0x200`。非法尺寸返回 -1。
- `lcd_draw_line(gb, pixels, line)`：160 字节。bit 1–0 是颜色，bit 5–4 是层（OBJ0 / OBJ1 / BG）。其余位未定义。注释写 line 保证在 0–144（含）。状态机只在 `IO_LY < LCD_HEIGHT`（144）时进入 OAM/绘制。适配层仍要丢掉 `line >= 144`。
- `gb_init_lcd()` 把 `direct.frame_skip` 和 `direct.interlace` 设为 false。`frame_skip` 每帧翻转一次（注释写只支持 30 fps 跳帧）。`interlace` 丢弃一半扫描线。
- `gb_reset()` 把 `direct.joypad` 设为 `0xFF`。读 `P1` 时把 `direct.joypad` 的对应半字节 OR 进寄存器。复位值全 1，因此在这个字节里 1 是松开、0 是按下。`JOYPAD_A` 0x01、`B` 0x02、`SELECT` 0x04、`START` 0x08、`RIGHT` 0x10、`LEFT` 0x20、`UP` 0x40、`DOWN` 0x80。
- `gb_error` 返回是未定义行为，头文件写明会导致 `SIGABRT`。
- 头文件内的 TODO 写明 MBC7、口袋相机、HuC1、HuC3 不支持；MMM01、MBC6 未测试。不支持的类型返回 `GB_INIT_CARTRIDGE_UNSUPPORTED`。校验和失败返回 `GB_INIT_INVALID_CHECKSUM`。
- 不设置 `gb_bootrom_read` 时没有 boot ROM。任天堂 boot ROM 有版权，本仓库不收录。

README @ 同一提交：LCD 和声音可在编译期关闭；启用声音时要外接 APU；`examples/sdl2/minigb_apu` 是一份快的 APU；SDL 示例里 MiniGB APU 跑在另一线程，时序不准确。GIF 那句“模拟跑在原生约 60 fps”说的是作者机器上的 SDL 示例，没有给出 CPU，不能当作 C3 的成绩。

### 4.6 RP2040-GB

README @ `3f366d79` 原文：ILI9225 屏；无音频超过 70 fps；跳帧加隔行最高约 120 fps。没有写测试时钟。ROM 用 `xxd` 嵌进 `rom.c`，并放在 flash 而不是 RAM。GBC 游戏会被迫以 DMG 模式启动。许可证 MIT。

这些 fps 只描述那份 RP2040 + ILI9225 固件。本设计不外推。

### 4.7 输入方案与引脚冲突

用户提过用 GPIO 0/1/3/7/10/4/5/6 做 8 键。对照 4.1 和 4.2，每一根都已占用：

| GPIO | 现占用 |
| --- | --- |
| 0 | 三键 ADC，外部 10 kΩ |
| 1 | LCD CS |
| 3 | I2S WS |
| 4 | I2S DIN；数据手册上的 JTAG 脚 |
| 5 | I2S BCLK；JTAG |
| 6 | I2S MCLK；JTAG |
| 7 | I2C SCL；JTAG |
| 10 | I2C SDA |

关掉音频初始化不会把已经焊到 codec 上的线变成空闲按键。数据手册把 GPIO11–17 留给 flash 电源或 flash SPI。FoloToy 指南写：表里没有的 GPIO 不自动空闲。本设计不提出一组“空闲脚”。

电阻梯在任一时刻只有一个电压，不能依赖和弦或同时按键。这是电路事实，不是软件没写好。

## 5. 计算

下面全部是算术。时钟用的是“请求值”或“历史宏”，不是测到的 SCLK。SPI 时间只算 RGB565 载荷字节，不含命令、CASET/RASET、DMA 间隙和 CPU 停顿。所以它是载荷下限时间：真实传输只会更慢或持平，不会比这个更快。这个方向是推断，定量差距待测。

DMG 帧：

- 周期 `70224`（事实，第 4.5 节）。
- 时钟 `4194304` Hz（minigb_apu.h 的 `DMG_CLOCK_FREQ`，与 Peanut 注释里的帧长时间一致）。
- 帧周期 `70224/4194304 = 0.016742706298828125` s = 16.742706298828125 ms。
- 帧率 `4194304/70224` Hz = 59.72750057… Hz。下文写作 59.7275 Hz 时，指的就是这个分数。

字节数：

| 画面 | 式子 | 字节 |
| --- | --- | --- |
| GB 整帧 RGB565 | `160*144*2` | 46080 |
| 3:2 最近邻 | `240*216*2` | 103680 |
| 全屏 | `240*320*2` | 153600 |
| 现用 LVGL 40 行 | `240*40*2` | 19200 |
| 指南旧 20 行 | `240*20*2` | 9600 |

3:2 的来源：`160*(3/2)=240`，`144*(3/2)=216`。它是每 2 个源像素变成 3 个目标像素的最近邻，不是整数倍。整数倍里能放进 240 宽度的只有 1 倍。2 倍是 320×288，宽度 320 大于 240，放不下。

请求 80 MHz 时，理想字节率 `80e6/8 = 10e6` 字节/秒：

| 载荷 | 时间 | 占 16.7427 ms 的比例 |
| --- | --- | --- |
| 46080 | 4.608 ms | 27.5% |
| 103680 | 10.368 ms | 61.9% |
| 153600 | 15.360 ms | 91.7% |

历史宏 40 MHz 时字节率减半：

| 载荷 | 时间 | 占一帧的比例 |
| --- | --- | --- |
| 46080 | 9.216 ms | 55.0% |
| 103680 | 20.736 ms | 123.9% |

在 40 MHz 这组假设下，3:2 全帧载荷本身已经超过一帧。这仍不是实测。它只说明：在时钟被确认之前，不应把 3:2 全帧刷新当成默认。

1:1 居中的窗口：

- `x0 = (240-160)/2 = 40`，`y0 = (320-144)/2 = 88`。
- 像素范围 `x ∈ [40, 200)`，`y ∈ [88, 232)`。
- 半径 30 的圆角带只落在 `y < 30` 或 `y ≥ 290`。画面的 y 从 88 到 232，不在这个带里。水平方向画面也缩在距左右边缘 40 px 以内，大于半径 30。

3:2 居中：`y0 = (320-216)/2 = 52`，`y ∈ [52, 268)`，同样不在圆角带里，但 x 用满 0–239。这是几何，玻璃是否吃像素要真机看。

NVS：分区 24576 字节。ESP-IDF v5.5.3 `nvs_set_blob` 文档写最大长度为 508000 字节或 `(97.6% of the partition size - 4000)` 字节中的较小者。

`0.976 * 24576 - 4000 = 19986.176` 字节。这是文档上限表达式，不是 `nvs_get_stats` 的剩余空间。页头和其他键还要占。

与 cart RAM 对照：32768（32 KiB）和更大的档大于整个 24576 字节的 NVS 分区，不可能放进这块 NVS。8192（8 KiB）和 2048（2 KiB）低于 19986 的公式上限，能否写入仍待 `nvs_set_blob` 的返回值。

MMU 页数，按 64 KB 一页、向上取整：1 MiB ROM 是 16 页，4 MiB ROM 是 64 页。总条目 128，且 I/D 共用。应用程序自己的代码也占条目。剩下多少要等链接之后看 `esp_mmu_map_dump_mapped_blocks()`。

minigb_apu 默认 `AUDIO_SAMPLE_RATE` 32768。`AUDIO_SAMPLES` 是 `(unsigned)(32768 / (4194304/70224))`。`4194304/32768 = 128`，`70224/128 = 548.625`，转成 `unsigned` 后是 548。立体声 `int16`：`548*2*2 = 2192` 字节/帧。`2192 * 4194304 / 70224 = 130922.68` 字节/秒。这是宏算出来的 PCM 体积，不是 APU 的 CPU 时间。

## 6. 推断

这些判断用来选默认路径。每一条的速度或手感都还没有本板数据。

1. 解释器若放在 flash（IROM），ROM 数据若经数据 cache 读取，会抢同一块 16 KB cache（`SOC_SHARED_IDCACHE_SUPPORTED` 与数据手册的 16 KB cache）。把热点放进 IRAM 有可能减少这种互踩。收益待测。IRAM 是否放得下，等 `idf.py size`。
2. 无 TE 脚（chess 硬件说明）。局部刷新可能撕裂。默认每帧只重画 160×144 窗口，边框只在启动时涂一次黑，这样 SPI 载荷停在 46080 字节那一档，而不是每帧 153600。撕裂是否可接受，待真机。
3. `swap_bytes` 是 LVGL 端口的开关。直接调用 `esp_lcd_panel_draw_bitmap` 时，驱动按内存顺序送 `color_data`（ESP-IDF v5.5.3 `esp_lcd_panel_ops.h`）。C3 是小端，`uint16_t` 的 `0xAD55` 在内存里是 `55 AD`。面板要高字节在前，所以 LUT 要存 `AD 55` 和 `52 AA`（第 9.3 节）。shade 0 和 3 看不出写反。中间档是否该用这个顺序，由 Q-SPI-SWAP 的四档条照片关闭。
4. 三键游戏手柄若走 `BUTTON_SINGLE_CLICK`，会等 180 ms 的短按窗口。电平应来自每帧的 `bsp_button_read_mv()`。长按换层若与“OK 即 A”叠在同一次按下上，这次按下在分类完成前不能写进 joypad，否则游戏会看到一段 A。这是手势定义，延迟待测。
5. 自制小 ROM 放在 app 的 DROM 里，不需要新的 MMU 映射，也不改分区表。可更换的大 ROM 才需要 `esp_partition_mmap`。ESP-IDF v5.5.3 分区文档：`esp_partition_mmap` 不要求调用者传入 64 KB 对齐的偏移；若 flash 地址未对齐，基址会向下对齐到 64 KB，返回指针再加回偏移。未对齐分区可能多占一页。
6. 单核上，Peanut README 所说的“APU 独立线程”只是另一个 FreeRTOS 任务，不是第二颗核。音频会占用本来给模拟和 SPI 的时间。所以首期不初始化 I2S。

## 7. 待确认

| 编号 | 问题 | 怎样关闭 |
| --- | --- | --- |
| Q-CLK | 线上 SCLK 是不是宏里的 80 MHz，屏在该速度下是否稳定 | 真机读初始化日志里的实际 `pclk`，必要时用逻辑分析仪看 SCLK。记录花屏与否 |
| Q-CPU | 运行频率是不是 160 MHz | 首次构建的 sdkconfig 里读 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ`，启动日志再打一次 `esp_clk_cpu_freq()` |
| Q-HEAP | 链接后 IRAM、内部 DRAM、最大连续块 | `idf.py size` 与启动时 `heap_caps_get_free_size` / `heap_caps_get_largest_free_block` |
| Q-MMU | app 映射之后还剩多少 MMU 条目 | `esp_mmu_map_dump_mapped_blocks()` |
| Q-FLASH | 芯片丝印是哪一档；flash 是否真是 8 MiB | 真机 `esptool.py flash_id`。指南和 sdkconfig 写 8 MB，本轮没有读到丝印 |
| Q-SPI-SWAP | 直接画 bitmap 时字节序是否必须交换 | 主机测试锁住第 9.3 节的内存字节。`pattern.gb` 只用 shade 0 和 3，这两档交换前后都是 `FF FF` / `00 00`，不能发现中间档写反。真机看启动时画在左边框上的四档条：中间两档应是灰，而不是偏绿或偏品红。若偏色，对调整个 LUT 并改主机测试 |
| Q-TEAR | 只刷新 160×144 窗口时撕裂是否可接受 | 真机视频 |
| Q-BEZEL | 物理圆角是否吃到 1:1 或 3:2 画面 | 真机照片。几何上软件圆角带吃不到居中的 1:1 |
| Q-LAT | 从 ADC 进入窗口到 joypad 掩码变化的时间 | `esp_timer_get_time()` 差值，真机记录。不预设毫秒数 |
| Q-NVS | 2 KiB、8 KiB blob 在这块 24 KiB NVS 里能否 `nvs_commit` | P4 的返回值。32 KiB 及以上不必试，分区本身不够 |
| Q-SAVE-PWR | 硬关机会丢最后一段时间的存档 | chess 硬件说明已写电源键没有可靠的最后保存回调。本项目同样不承诺 |
| Q-BLARGG | Blargg 测试 ROM 的许可证 | 本轮读了 [retrio/gb-test-roms README](https://github.com/retrio/gb-test-roms)，没有找到 license / copyright / public domain 字样。确认前不下载、不提交 |
| Q-VARIANT | 板上 ESP32-C3 具体订购型号 | 不阻塞 P0。只在 flash 容量与 sdkconfig 不符时阻塞刷写 |

## 8. 架构

ESP-IDF 工程，目标 `esp32c3`，IDF 版本与上游硬件指南一致，用 5.5.3。主机测试不依赖 IDF。

```text
app_main
  ├─ gb_rom      取出 ROM 字节（先内嵌，后可选 mmap）
  ├─ gb_core     包装 Peanut-GB。唯一允许 include peanut_gb.h 的适配层
  ├─ gb_video    2bit → RGB565，1:1 窗口，SPI DMA
  ├─ gb_input    每帧读 ADC，维护层与 joypad 按下掩码
  ├─ gb_sched    用 esp_timer 决定本轮跑几帧，并分开计时
  ├─ gb_save     cart RAM ↔ NVS 或以后的分区
  ├─ gb_audio    首期空实现，返回“未启用”
  └─ gb_bench    每秒一行统计
```

模块边界：

- `gb_core` 不调用 `esp_lcd`、GPIO、NVS。主机测试和固件链接同一份。
- `gb_video` 不解释指令。它只接收 160 字节约 2 bit 行。
- `gb_input` 不跑模拟。它产出 `uint8_t` 按下掩码，位定义与 `JOYPAD_*` 相同，1 表示按下。
- `gb_rom` 不知道模拟器。越界读由 `gb_core` 的回调返回 `0xFF`。
- Peanut-GB 头文件放在 `third_party/peanut-gb/`，不改内容。编译开关在包含它的那个 `.c` 里定义，全工程一致。
- 不初始化 LVGL、Wi-Fi、BLE、I2S。显示只用 `bsp_display_init()` 和 `bsp_display_panel()`。

建议目录（实现时才创建）：

```text
CMakeLists.txt
sdkconfig.defaults
partitions.csv                      # 与 33d3d1d 相同。P0–P3 不改，第 14 节第 2 项已决定
main/app_main.c
main/CMakeLists.txt
components/bsp/                     # 从锁定的 anki BSP 复制，附 IMPORT.md
components/gb_core/
components/gb_video/
components/gb_input/
components/gb_sched/
components/gb_save/
components/gb_audio/
components/gb_bench/
third_party/peanut-gb/peanut_gb.h
third_party/peanut-gb/LICENSE
roms/homebrew/                      # 只有自制 ROM
tools/gb_host.c
tools/build_rom.sh
docs/benchmarks/                    # P3 才填数字
```

编译开关，主机和设备必须相同，否则帧哈希不能对照：

```text
ENABLE_SOUND=0
ENABLE_LCD=1
PEANUT_GB_12_COLOUR=1
PEANUT_GB_HIGH_LCD_ACCURACY=1
PEANUT_GB_USE_INTRINSICS=1
```

## 9. 接口草案

类型用 `stdint.h` 与 `esp_err.h`。主机侧 `gb_video` 不编译。错误码全模块共用：

```c
typedef enum {
    GB_OK = 0,
    GB_ERR_BAD_ARG,
    GB_ERR_CHECKSUM,
    GB_ERR_UNSUPPORTED,
    GB_ERR_NO_MEM,
    GB_ERR_SAVE,
    GB_ERR_DISABLED
} gb_err_t;
```

### 9.1 `gb_core`

```c
typedef struct gb_core gb_core_t;

typedef void (*gb_line_fn)(void *ctx, const uint8_t *pixels160, uint8_t line);

/* rom 在 gb_core_run_frame 期间必须保持有效。
 * cart_ram 可为 NULL 当且仅当卡带 RAM 尺寸为 0。
 * 尺寸与 gb_core_save_size 不一致时返回 GB_ERR_BAD_ARG。 */
gb_err_t gb_core_init(gb_core_t *core,
                      const uint8_t *rom, size_t rom_size,
                      uint8_t *cart_ram, size_t cart_ram_size);

/* 成功时 *out 等于 gb_get_save_size_s 的结果：
 * 0、0x200（MBC2）、0x800、0x2000、0x8000、0x10000、0x20000。 */
gb_err_t gb_core_save_size(const uint8_t *rom, size_t rom_size, size_t *out);

/* pressed_mask 的 1 表示按下，位与 JOYPAD_* 相同。
 * 适配层写入 direct.joypad = (uint8_t)~pressed_mask。 */
void gb_core_set_pressed(gb_core_t *core, uint8_t pressed_mask);

/* 跑完 Peanut 的一帧。draw 可为 NULL（只计时，不留像素）。
 * line >= 144 的回调由适配层直接丢掉。 */
void gb_core_run_frame(gb_core_t *core, gb_line_fn draw, void *ctx);

void gb_core_set_frame_skip(gb_core_t *core, bool enable);
void gb_core_set_interlace(gb_core_t *core, bool enable);

uint8_t *gb_core_cart_ram(gb_core_t *core);
size_t gb_core_cart_ram_size(const gb_core_t *core);
```

`gb_core_t` 内含 `struct gb_s`。调用方用静态存储或堆，不放任务栈。错误回调里记录地址后 `abort()`（主机）或 `esp_restart()`（设备），不返回。

取色：`shade = pixels[x] & 0x03`。bit 5–4 在灰阶路径忽略。不屏蔽就会把层标志当成色号。

### 9.2 `gb_rom`

```c
typedef struct {
    const uint8_t *data;
    size_t size;
} gb_rom_t;

/* P0–P3：返回链接进 app 的自制 ROM。 */
gb_err_t gb_rom_open_embedded(gb_rom_t *rom);
```

以后若增加分区映射，另加 `gb_rom_open_partition(const char *name, gb_rom_t *rom)`，用 `esp_partition_mmap(..., ESP_PARTITION_MMAP_DATA, ...)`。P0–P3 不声明这个函数，避免还没决定分区就写出调用。

### 9.3 `gb_video`

```c
typedef enum {
    GB_SCALE_1X_CENTER = 0,
    GB_SCALE_3_2 = 1
} gb_scale_t;

typedef struct {
    gb_scale_t scale;
} gb_video_config_t;

esp_err_t gb_video_init(const gb_video_config_t *cfg);
void gb_video_push_line(const uint8_t *pixels160, uint8_t line);
```

`gb_video_init` 调用已有的 `bsp_display_init()`，把背光设到一个固定百分比（设计选择：50，不是测出来的最佳值），把边框涂黑一次。

灰阶 LUT 是设计选择，对齐 dmg-acid2 README 对灰阶图的 8 位值 `$00,$55,$AA,$FF`，并按 DMG 习惯把 shade 0 当最亮、shade 3 当最暗：

| shade | 8 位灰 | 目标 RGB565 | 按地址顺序送出的内存字节（推断，待 Q-SPI-SWAP） |
| --- | --- | --- | --- |
| 0 | `0xFF` | `0xFFFF` | `FF FF` |
| 1 | `0xAA` | `0xAD55` | `AD 55` |
| 2 | `0x55` | `0x52AA` | `52 AA` |
| 3 | `0x00` | `0x0000` | `00 00` |

`0xAD55` 与 `0x52AA` 的算法：`R5 = gray>>3`，`G6 = gray>>2`，`B5 = gray>>3`。这是设计选择。C3 上 `uint16_t` 的 `0xAD55` 在内存里是 `55 AD`。`esp_lcd_panel_draw_bitmap` 按地址顺序送字节，面板要高字节在前，所以表里要存 `AD 55`，不能存原生小端。shade 0 和 3 两种顺序相同。真机若中间档偏色，对调这两档的字节并改测试。

1:1 路径对 GB 第 `line` 行调用：

```c
esp_lcd_panel_draw_bitmap(panel,
                          40, 88 + line,
                          200, 89 + line,
                          row160_rgb565);
```

终点是开区间，所以一行的高度是 1。DMA 缓冲两行（`160*2*2 = 640` 字节），一行在送、一行在填。缓冲用内部 RAM、DMA 对齐。3:2 路径在 P3 之前不实现；接口里的枚举保留，初始化时若不是 `GB_SCALE_1X_CENTER` 就返回 `ESP_ERR_NOT_SUPPORTED`。

### 9.4 `gb_input`

```c
typedef enum {
    GB_LAYER_DPAD_A = 0,   /* UP=Up, DOWN=Down, OK=A */
    GB_LAYER_LR_B,         /* UP=Left, DOWN=Right, OK=B */
    GB_LAYER_SYS           /* UP=Select, DOWN=Start, OK 无效 */
} gb_layer_t;

typedef struct {
    uint8_t pressed_mask;  /* JOYPAD_* 位，1=按下 */
    gb_layer_t layer;
    bool menu_open;
} gb_input_state_t;

esp_err_t gb_input_init(void);
void gb_input_poll(gb_input_state_t *out, int64_t now_us);
```

`gb_input_poll` 每帧调用一次。解码用 `BSP_BTN_MV_TABLE` 的半开区间，与 BSP 相同。读失败（`bsp_button_read_mv` 返回 -1）时本帧掩码为 0，不当成 UP。

已决定的手势（2026-11-11，Vincent）：

- 层 0：UP→Up，DOWN→Down，OK→A。
- 层 1：UP→Left，DOWN→Right，OK→B。
- 层 2：UP→Select，DOWN→Start。OK 不进 joypad。
- 长按 OK 达到 `BSP_BTN_LONG_PRESS_MS`（宏是 500）时打开或关闭暂停菜单。这次按下在分类完成前不写入 joypad。菜单打开时，UP/DOWN/OK 只移动菜单，掩码为 0。
- 菜单项至少有：换层、保存（P4 之后）、重置、帧跳过开关（默认关）。

任意时刻最多一个 GB 键为按下。需要方向加 A 的游戏不能完整玩。这是电路限制。

### 9.5 `gb_sched` 与 `gb_bench`

```c
typedef struct {
    int64_t emu_us;       /* 只含 gb_core_run_frame，draw 写入 RAM */
    int64_t present_us;   /* 只含把该帧推到面板 */
    uint32_t emu_frames;
    uint32_t present_frames;
} gb_bench_window_t;

void gb_sched_init(int64_t now_us);

/* 返回本轮应该模拟的帧数：0 或 1。
 * 落后时不追赶积压，避免越欠越多。 */
int gb_sched_due(int64_t now_us);
```

目标节拍是 `1000000 * 70224 / 4194304` 微秒/帧，即 16742.706… µs。用 64 位整数累加，避免每帧用浮点。`esp_timer_get_time()` 是 IDF 的微秒计时。

统计每秒打一行：`emu_us`、`present_us`、两个帧计数、空闲堆、最大块。模拟帧/秒和推屏帧/秒分开，禁止合成一个“fps”对外。日志宏在帧循环内关闭；这一行可以每秒一次。

### 9.6 `gb_save`

```c
gb_err_t gb_save_store(const uint8_t *ram, size_t size);
gb_err_t gb_save_load(uint8_t *ram, size_t size);
```

`size == 0` 时两个函数返回 `GB_OK`。`size > 2048` 时，在独立存档分区落地之前返回 `GB_ERR_SAVE`，不截断写入。2048 是设计选择的保守上限：它低于第 5 节的 NVS 公式上限，也等于 Peanut 尺寸表里的 `0x800`（2 KiB）。MBC2 的 `0x200`（512 字节）更小，同样走 NVS。8 KiB 是否放得进这块 NVS 是 Q-NVS，不在默认路径里赌。

写入用 `nvs_set_blob` + `nvs_commit`。失败把错误码打到日志并返回 `GB_ERR_SAVE`。触发点是设计选择：菜单里的“保存”，以及每 30 个实际跑完的模拟帧尝试一次。`30 * 70224 / 4194304 = 0.502` 秒模拟时间，不是墙钟保证。硬关机可能丢掉最后一次成功 commit 之后的数据。

验收顺序：`gb_save_load` 必须在本轮任何 `gb_core_run_frame` 之前完成，并先打印读回的第 0 字节。自制 ROM 每次启动都会把该字节写成 `0xA5`。若等 ROM 跑完再看，保存失败和保存成功会得到同一个 `0xA5`。主机测试在 `store` 之后把缓冲清零，再 `load`，中间不跑帧。真机第一次启动（没有键）打印的必须不是 `A5`；保存并分段重启后，第一帧之前的那一行必须是 `A5`。

### 9.7 `gb_audio`

```c
gb_err_t gb_audio_init(void); /* 首期返回 GB_ERR_DISABLED，不调用 bsp_audio_init */
```

P5 之前不定义 `ENABLE_SOUND=1`，不把 minigb_apu 编进固件。

## 10. 关键决策与备选

### 10.1 ROM 放哪

已决定：P0–P3 不改 `partitions.csv`。自制 ROM 用 `target_add_binary_data` 或等价方式链进 app。工厂分区仍是 `0x10000/0x7F0000`。

理由：上游、chess、anki 都用这张表；改表并刷全镜像可能清掉 NVS；自制 ROM 远小于 app 分区。

下面的分区草案不是 P0–P3 的工作。只有在以后另作决定、并且 `idf.py size` 已经量过固件体积时才考虑。表对齐到 64 KB，总和停在 `0x800000` 以内，彼此不重叠：

| 名 | 偏移 | 大小 | 说明 |
| --- | --- | --- | --- |
| nvs | `0x9000` | `0x6000` | 不动 |
| phy_init | `0xf000` | `0x1000` | 不动 |
| factory | `0x10000` | `0x200000`（2 MiB） | 给不含 LVGL 的 GB 固件。2 MiB 是否够，等 `idf.py size` |
| gb_rom | `0x210000` | `0x400000`（4 MiB） | 64 KB 对齐。超过 4 MiB 的 ROM 直接拒绝 |
| gb_save | `0x610000` | `0x20000`（128 KiB） | 覆盖 Peanut 最大的 cart RAM 档 |

`0x630000`–`0x800000` 还剩 `0x1D0000` 字节。这是空着的，不是第二套 ROM。

这张表是计算草案，不是本轮的改动。app 实际体积未知之前，不把 factory 砍到 2 MiB。

### 10.2 画面

已决定：1:1 居中，只刷新 GB 窗口，边框启动时涂黑。LUT 见 9.3。3:2 留到测完时钟和 `present_us` 之后。

备选：3:2（240×216）居中。等 Q-CLK 和 P3 的 `present_us` 有了再做。重复模式（每两个源像素输出三个）在实现时写死一种并放进测试，不在本文假装已经选了 A,A,B 还是 A,B,B。

不采用：2 倍放大（宽度放不下）；全屏每帧刷新当默认（载荷 153600 字节，占请求 80 MHz 下理想时间的 91.7%，还没算 CPU）。

### 10.3 输入

已决定：第 9.4 节的三键加菜单。任意时刻最多一个 GB 键。不改板。

备选，都不是 P2：

- 改板把现有脚改成独立按键。8 键方案与现占用全部冲突，还要处理 strapping 和 flash 脚。需要新的原理图审查。
- 共享 I2C 上加 IO 扩展器。地址不能是 `0x18` 或 `0x63`。板上现在没有这颗芯片。延迟待测。
- BLE HID。芯片有 Bluetooth LE，但 chess/anki 的 defaults 把 `CONFIG_BT_ENABLED` 关掉。RAM 和延迟都待测。

不采用：把 USB-C 当 USB Host 手柄口（第 4.1 节）。

### 10.4 音频

已决定：首期关闭。`gb_audio_init` 返回 `GB_ERR_DISABLED`，不碰 I2S，不把 minigb_apu 编进固件。P5 若要做，须另有决定。即便做，也先在主机上证明 PCM 尺寸，再在设备上单独测 `emu_us` 增加了多少。Peanut README 已说明 SDL 示例的 APU 时序不准确。

### 10.5 存档

已决定：仅 `size <= 2048` 走现有 NVS；更大返回 `GB_ERR_SAVE`。P0–P3 不新增 `gb_save` 分区。

不在掉电中断里“最后写一次”。没有这个回调。

### 10.6 性能工作的顺序

P3 按这个顺序各测一版，不把几项优化叠在同一次记录里冒充单一原因：

1. 基线：跳帧关、隔行关、音频关、1:1、帧循环里无日志。优化级别用构建默认值，并把该 Kconfig 符号写进记录。
2. 只改优化：`CONFIG_COMPILER_OPTIMIZATION_SIZE`（`-Os`）与 `CONFIG_COMPILER_OPTIMIZATION_PERF`（`-O2`）各一轮。
3. 只改放置：热点 `IRAM_ATTR` 与全在 flash 各一轮。放不下就记录链接错误，不要裁功能硬塞。
4. 只改 DMA：确认双行缓冲相对单行的 `present_us`。
5. 最后才打开 Peanut 的 `frame_skip` 或 `interlace`，单独成行。默认仍关。

每一行都有 `emu_us` 和 `present_us`。没有“达到 60”这种门槛。跳帧和隔行默认关，在 P3 的表填上之前不是验收线。测完之后是否打开，不在第 14 节这 8 项里。

## 11. 性能预算与测量

预算只约束“测什么”，不约束“必须达到多少”。

| 量 | 定义 | 工具 |
| --- | --- | --- |
| 模拟时间 | `gb_core_run_frame` 的墙钟，回调只把 160 字节约进 RAM | `esp_timer_get_time` |
| 推屏时间 | 该帧所有 `esp_lcd_panel_draw_bitmap` 返回之后的墙钟 | 同上 |
| 节拍 | 应有帧的时间点。`1000000*70224/4194304 = 16742` 余 `2962432`。每帧加 16742 µs，余数累加 2962432，满 4194304 再加 1 µs | `gb_sched` |
| cache 压力 | 同一 ROM、同一帧数，IRAM 与 flash 两版的模拟时间差 | 对照实验。不用不存在的命中计数 |
| 堆 | 启动后与每秒统计时的 free / largest | `heap_caps_*` |

主机侧不测 SPI。主机只证明核心和哈希。设备侧两项都测。云端不能刷本机，设备栏由持有板子的人填写，步骤在计划 P1。

参考移植的 70 fps 与 120 fps 不进入这张表。

## 12. 阶段出口

细节和命令在 `docs/plans/2026-11-11-ai-passport-gb-implementation-plan.md`。出口门槛：

| 阶段 | 做到 | 客观证据 | 谁执行 |
| --- | --- | --- | --- |
| P0 | 主机无头跑 Peanut-GB 加自制 ROM | 构建日志；约定像素；SHA-256 文件；坏校验和返回 `GB_ERR_CHECKSUM` | CI / 开发机 |
| P1 | C3 固件把同一 ROM 画到屏上，串口哈希与主机一致 | 构建日志；`flash_id`；串口哈希；照片或短视频 | 构建可在 CI；刷机和拍照由持有板子的人做 |
| P2 | 三键改变 joypad 掩码，菜单能换层 | 串口打印的掩码与层；真机按键视频 | 真机 |
| P3 | 第 10.6 节每一行都有数字，或写明该行没跑的原因 | `docs/benchmarks/` 里的表 | 真机填数字 |
| P4 | 2 KiB RAM 保存后，在 ROM 执行前读回；超限返回错误 | 主机：清零后再 load，中间不跑帧。真机：第一帧之前的日志 | 两边 |
| P5 | 可选。音频仍默认关，除非有单独的测量记录 | 打开音频前后的 `emu_us` | 真机；不做也算出 P0–P4 完成 |

P0 的像素合同（自制 ROM，不是猜的画面）：

- 标题 `AIPATTERN`，无 MBC，无 cart RAM，`rgbfix -v` 通过。
- tile 1 为色号 3，地图 (0,0) 指向 tile 1，其余地图为 tile 0（色号 0），BGP 为 `0xE4`。
- 跑完第 2 帧和第 3 帧时，两帧哈希相同；shade(0,0)=3，shade(8,0)=0，shade(159,143)=0。shade 取像素的低 2 位。

## 13. 风险

| ID | 风险 | 依据 | 处理 |
| --- | --- | --- | --- |
| R1 | 把 80 MHz 当成已验收 | 宏是 80，历史是 40，chess 写明未验收 | 默认 1:1；Q-CLK |
| R2 | 共享 16 KB cache 把模拟拖慢 | 数据手册与 `SOC_SHARED_IDCACHE_SUPPORTED` | P3 对照，不预写加速比 |
| R3 | 三键玩不了方向+A | 电阻梯 | 已接受的产品限制（第 14 节第 1 项） |
| R4 | 改分区清掉 NVS | 上游 build-and-test.md | P0–P3 不改表 |
| R5 | 大 cart RAM 分配失败 | 400 KB SRAM 含 16 KB cache，IDF 占用未测 | 失败返回 `GB_ERR_NO_MEM` |
| R6 | 商业 ROM 进仓库 | 范围 | 不下载、不提交、不把路径写进默认构建 |
| R7 | GPL 的 mooneye-gb 模拟器混进工程 | 该仓库 LICENSE 是 GPL-3.0 | 不收录。测试套件是另一个 MIT 仓库，见第 15 节 |
| R8 | 硬关机丢档 | chess 硬件说明 | 不承诺；菜单保存 |
| R9 | 改 GPIO8/9 导致无法下载 | 数据手册 strapping | 不改这组脚 |
| R10 | 用别人的 fps 排期 | RP2040 README 未写时钟；PocketStar 不是本板 | 第 2 节禁止 |
| R11 | 按指南分配 9600 字节缓冲 | 代码已是 19200；且 GB 不用 LVGL | 以宏为准 |
| R12 | Peanut 行注释写 0–144 含端点 | 绘制路径只在 LY&lt;144 | 适配层丢弃 `line>=144` |
| R13 | 直接画 bitmap 的字节序 | LVGL 路径开了 `swap_bytes` | Q-SPI-SWAP |

## 14. 已决定（2026-11-11，Vincent）

下面 8 项全部采用当时的建议默认值。实现按这些决定做。

1. 输入。接受现有三键和暂停菜单换层，并接受任意时刻最多一个 GB 键。这是 P2。改板、I2C 扩展和 BLE 不进入 P2。
2. ROM 分区。P0–P3 保持官方分区表，只内嵌自制 ROM。第 10.1 节的分区草案不在这几阶段实施。
3. 缩放。默认 1:1 居中。3:2 只在测完时钟和 `present_us` 之后再做。
4. 仓库许可证。MIT，版权所有者为 yangguo，年份 2026。根目录 `LICENSE` 即该文本。第三方声明另见第 15 节，根 `LICENSE` 不取代它们。
5. 流畅的定义。看到 P3 的表之前，跳帧和隔行不是验收线。两者默认关。
6. 音频。首期不初始化 I2S。P5 可选，不做不影响前面各阶段。
7. 第三方测试 ROM。仓库只提交自制 ROM 的源和产物。dmg-acid2、mooneye-test-suite、mealybug-tearoom-tests、age-test-roms 只作为文档中的可选用项，默认不入库。Blargg 在 Q-BLARGG 关闭前不用。
8. 存档。只有 ≤2048 字节的 cart RAM 走现有 NVS。更大直接失败，返回 `GB_ERR_SAVE`。P0–P3 不为此改分区表。

此前已定、本次未改：只做 DMG；不做 GBC/GBA；不收录商业 ROM 和任天堂 boot ROM。

## 15. 许可证与复用义务

### 15.1 要复用的代码

| 作品 | 许可证 | 复用时要做的事 |
| --- | --- | --- |
| Peanut-GB `peanut_gb.h` @ `d0bcca77` | 文件头 MIT。版权 2018–2023 Mahyar Koshkouei。声明含 SameBoy 片段，版权 2015–2019 Lior Halphon，摘取时为 MIT | 见 15.1.1。不改这个头文件 |
| minigb_apu @ 同一 Peanut-GB 树，`examples/sdl2/minigb_apu/LICENSE` | MIT。版权 2017 Alex Baines，2019 Mahyar Koshkouei | 首期不复制、不编进固件（第 14 节第 6 项）。P5 若另有决定要做，复制时带上该 `LICENSE` 和 `minigb_apu.h` 顶部的说明 |
| FoloToy BSP @ `33d3d1d`，anki 副本 `components/bsp/LICENSE` | MIT。版权 2026 FoloToy | 复制整个 `components/bsp` 时保留 `LICENSE`。写 `IMPORT.md`，记下上游提交、anki 提交、以及相对上游多出来的 deep-sleep 文件（anki `IMPORT.md` 已列出 `bsp_power.c`）。不要只跟随 FoloToy `main` |
| RGBDS | MIT。版权 1996–2025 Carsten Sørensen 与贡献者 | 只作构建工具。不把 RGBDS 源码 vendoring 进本仓库。自制 ROM 源码是本项目的作品 |

### 15.1.1 Peanut-GB 的 MIT 署名

Peanut-GB 文件头的 MIT 条件是：所有副本和实质性部分都必须带上上述版权声明和许可声明。本仓库因此做到这三件事：

- 收入 `peanut_gb.h` 时保留它的整段文件头，包括 “Copyright (c) 2018-2023 Mahyar Koshkouei”，以及其中关于 SameBoy、版权 2015–2019 Lior Halphon 的段落。
- `third_party/peanut-gb/LICENSE` 是这段 MIT 正文的副本，不改句子。
- 根目录 `LICENSE` 只覆盖本仓库自己的代码和自制 ROM，版权为 Copyright (c) 2026 yangguo。它不替换、不删除 Peanut-GB、SameBoy 片段、minigb_apu 或 FoloToy BSP 的声明。

### 15.1.2 本仓库许可证

根 `LICENSE` 已按第 14 节第 4 项加入：MIT，Copyright (c) 2026 yangguo。

### 15.2 候选测试 ROM

本轮只核实了仓库里的许可证文本，没有把 ROM 下载进本工作区。

| 候选 | 来源 | 许可证 | 本设计的用法 |
| --- | --- | --- | --- |
| 自制 `AIPATTERN` | 计划里的 `roms/homebrew/pattern.asm`，RGBDS 组装 | 本仓库 MIT，Copyright (c) 2026 yangguo。源码由本项目编写 | P0 起用。可以提交源码和构建产物 |
| dmg-acid2 | [mattcurrie/dmg-acid2](https://github.com/mattcurrie/dmg-acid2) `8a98ce731f96dde032ffb22ec36dc985d78fdb18`（2020-04-21） | 仓库 `LICENSE` 为 MIT，版权 2020 Matt Currie | 可选的主机 PPU 对照。默认不入库。若要提交构建产物，必须同时带上该 MIT 声明 |
| mealybug-tearoom-tests | [mattcurrie/mealybug-tearoom-tests](https://github.com/mattcurrie/mealybug-tearoom-tests) | 仓库 `LICENSE` 为 MIT，版权 2018 Matt Currie | 可选，更严的 PPU 时序。不是“能跑”的门槛。默认不入库 |
| mooneye-test-suite | [Gekkio/mooneye-test-suite](https://github.com/Gekkio/mooneye-test-suite) `31510e12eea6286d36eea060a6adde755e1067aa` | 该仓库 `LICENSE` 是 MIT，版权 2014–2022 Joonas Javanainen | 可选主机精度测试。默认不入库 |
| age-test-roms | [c-sp/age-test-roms](https://github.com/c-sp/age-test-roms) | 仓库 `LICENSE` 为 MIT，版权 2021 Christoph Sprenger | 可选。默认不入库。文件名里的 `cgb` 项不做 |
| Blargg gb-tests | [retrio/gb-test-roms](https://github.com/retrio/gb-test-roms) | 待确认。README 未写许可证 | 不用 |
| mooneye-gb 模拟器 | [Gekkio/mooneye-gb](https://github.com/Gekkio/mooneye-gb) | GPL-3.0 | 不收录。它和上面的 test-suite 不是同一个仓库 |
| BullyGB | [Hacktix/BullyGB](https://github.com/Hacktix/BullyGB) | GitHub license API 报告 MIT | 可选，非门槛。本轮没有通读每个源文件的头，默认不入库 |
| little-things-gb | [pinobatch/little-things-gb](https://github.com/pinobatch/little-things-gb) | 待确认。本轮在 README 前列没有找到许可证段落 | 不用 |
| cgb-acid2 及任何 GBC 专用 ROM | — | — | 不做 |
| 商业游戏 ROM | — | 无再分发许可 | 不下载、不提交、不在文档里写获取方式 |

dmg-acid2 README 要求灰阶对照使用 `$00,$55,$AA,$FF`。那是可选测试的比较方法，不是 P0 自制 ROM 的哈希输入。P0 哈希的是 2 bit shade。

## 16. 不做

- 不做 GBC、GBA、SGB 边框、掌机连线对战。
- 不在 P5 之前做声音。
- 不把 LVGL 菜单嵌进帧循环。暂停菜单用同一套 bitmap 画几行字，或先用串口命令代替画面菜单；P2 的验收是串口上的层和掩码，画面菜单可以是随后的小步。
- 不修改 Peanut-GB 头文件来“优化”。放置到 IRAM 用适配层的编译单元和链接脚本，而不是改第三方源码。若链接脚本做不到，P3 记录失败，再另开决定。
- 不发布固件、不部署、不改 GitHub 仓库设置。

## 17. 真机步骤（设计层）

云端代理不能做这些。持有板子的人在 P1 计划的命令之外，至少记录：

1. `esptool.py flash_id` 的容量。
2. 启动日志里的 CPU 频率和 `BSP_LCD_PCLK_HZ`。
3. 串口 `gb_hash` 与主机 `pattern.sha256` 是否相同。
4. 一张亮着的屏的照片，能看出左上角一块深色、其余为浅色。
5. 若花屏，把时钟从 80 MHz 改到 40 MHz 再拍一张，并在基准表里写明用的是哪一个请求值。这一步是诊断，不是默认配置。
