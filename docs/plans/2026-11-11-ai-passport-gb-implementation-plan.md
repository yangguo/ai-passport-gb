# AI Passport GB Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 FoloToy AI Passport 上把 DMG 模拟器从“主机哈希一致”做到“可测量的上屏”，音频默认关闭。

**Architecture:** Peanut-GB 只活在 `gb_core` 适配层里。ROM、画面、按键、节拍、存档分开。P0 在 Linux 上证明核心；P1 起才链接 ESP-IDF。1:1 居中是唯一实现的缩放，直到 P3 测完。

**Tech Stack:** RGBDS（MIT，仅作工具）、C11、Peanut-GB `d0bcca771c83a2638c93a9ae61f3d51f226dc905`、ESP-IDF 5.5.3、目标 `esp32c3`。

设计说明：`docs/design/2026-11-11-ai-passport-gb-design.md`。接口以该文档第 9 节为准。本文复述签名，避免实现任务时两边各写一套名字。本文不预写模拟器函数体。

## Global Constraints

- 只做 DMG。不实现 GBC/GBA。
- 不下载、不提交、不链接商业 ROM 或任天堂 boot ROM。
- 不修改 `third_party/peanut-gb/peanut_gb.h`。
- 主机与设备使用同一组宏：`ENABLE_SOUND=0`、`ENABLE_LCD=1`、`PEANUT_GB_12_COLOUR=1`、`PEANUT_GB_HIGH_LCD_ACCURACY=1`、`PEANUT_GB_USE_INTRINSICS=1`。
- `partitions.csv` 与上游 `33d3d1d` 保持一致，直到设计第 14 节问题 2 被否决。
- 不初始化 LVGL、Wi-Fi、BLE、I2S。
- 性能数字写入 `docs/benchmarks/` 时必须带单位和构建配置。没有测到的格子写 `待测`，不填估计值。
- 真机步骤标为“用户真机”。云端代理不刷写、不把缺照片当成实现失败。
- 按下掩码 1 表示按下，位与 Peanut 的 `JOYPAD_*` 相同。适配层写入 `direct.joypad = (uint8_t)~pressed_mask`。

---

## 文件地图

| 路径 | 责任 | 哪一任务创建 |
| --- | --- | --- |
| `third_party/peanut-gb/peanut_gb.h` | 上游头文件，原样 | Task 1 |
| `third_party/peanut-gb/LICENSE` | MIT 全文 | Task 1 |
| `third_party/peanut-gb/SOURCE.md` | 提交号与日期 | Task 1 |
| `roms/homebrew/pattern.asm` | 自制 ROM 源 | Task 2 |
| `roms/homebrew/pattern.gb` | `rgbfix -v` 产物 | Task 2 |
| `roms/README.md` | 哪些 ROM 可以入库 | Task 2 |
| `tools/build_rom.sh` | 调 RGBDS | Task 2 |
| `tools/gb_host.c` | 无头跑帧、哈希、坏 ROM | Task 3 |
| `tools/test_host.sh` | 一条命令跑主机验收 | Task 3 |
| `roms/homebrew/pattern.sha256` | 第 3 帧 shade 的 SHA-256 | Task 3 |
| `components/gb_core/` | 适配层 | Task 3 的逻辑，Task 7 收成组件 |
| `CMakeLists.txt`、`sdkconfig.defaults`、`partitions.csv`、`main/` | IDF 工程 | Task 5 |
| `components/bsp/` | 锁定的显示与按键 | Task 6 |
| `components/gb_video/` | 1:1 推屏 | Task 8 |
| `components/gb_sched/`、`components/gb_bench/` | 节拍与统计 | Task 9 |
| `components/gb_input/` | 三键 | Task 10 |
| `docs/benchmarks/2026-11-11-template.md` | 空表 | Task 11 |
| `components/gb_save/` | NVS | Task 12 |
| `components/gb_audio/` | 返回未启用 | Task 13 |

P0 的主机程序可以先直接包含适配层的 `.c`，Task 7 再把它移进 `components/gb_core`，符号保持不变。

---

### Task 1: 钉住 Peanut-GB

**Files:**
- Create: `third_party/peanut-gb/peanut_gb.h`
- Create: `third_party/peanut-gb/LICENSE`
- Create: `third_party/peanut-gb/SOURCE.md`

**Interfaces:**
- Consumes: 无
- Produces: 原样的 `peanut_gb.h`。后续任务包含它，不复制进别的目录。

- [ ] **Step 1: 取出指定提交的两个文件**

```bash
rm -rf /tmp/peanut-gb
git init /tmp/peanut-gb
git -C /tmp/peanut-gb remote add origin https://github.com/deltabeard/Peanut-GB.git
git -C /tmp/peanut-gb fetch --depth 1 origin d0bcca771c83a2638c93a9ae61f3d51f226dc905
git -C /tmp/peanut-gb checkout FETCH_HEAD
mkdir -p third_party/peanut-gb
cp /tmp/peanut-gb/peanut_gb.h third_party/peanut-gb/peanut_gb.h
```

该提交的许可证在 `peanut_gb.h` 文件头，不在单独的 `LICENSE`。把文件头从 `/**` 到结束的 `*/` 中的 MIT 正文写入 `third_party/peanut-gb/LICENSE`，不要改写句子。

- [ ] **Step 2: 写 SOURCE.md**

内容只包括：上游 URL、提交 `d0bcca771c83a2638c93a9ae61f3d51f226dc905`、提交日期 2026-09-21、本仓库不修改该头文件、SameBoy 片段的版权声明留在头文件内。

- [ ] **Step 3: 核对没有混入 ROM 或 minigb_apu**

```bash
find third_party -type f -print
git -C /tmp/peanut-gb rev-parse HEAD
```

Expected: 只有 `peanut_gb.h`、`LICENSE`、`SOURCE.md`。`rev-parse` 打印 `d0bcca771c83a2638c93a9ae61f3d51f226dc905`。

- [ ] **Step 4: Commit**

```bash
git add third_party/peanut-gb
git commit -m "chore: vendor Peanut-GB header at d0bcca77"
```

---

### Task 2: 自制 ROM

**Files:**
- Create: `roms/homebrew/pattern.asm`
- Create: `roms/README.md`
- Create: `tools/build_rom.sh`
- Create: `roms/homebrew/pattern.gb`
- Test: 像素合同在 Task 3 执行

**Interfaces:**
- Consumes: 无
- Produces: `roms/homebrew/pattern.gb`。无 MBC、无 RAM。第 2 帧与第 3 帧的 shade 图相同：`(0,0)` 为 3，`(8,0)` 为 0，`(159,143)` 为 0。

- [ ] **Step 1: 写 pattern.asm**

不包含 `hardware.inc`。入口在 `$100`，代码在 `$150`。关中断；若 LCD 开着，等到 `[$FF44] >= 144` 再把 `[$FF40]` 清零。把 `$8000` 起 16 字节写成 0（tile 0，色号 0）。把 `$8010` 起 16 字节写成 `$FF`（tile 1，色号 3）。把 `$9800` 起 32×32 字节写成 0，然后把 `$9800` 写成 1。`[$FF47] = $E4`。`[$FF40] = $91`。然后 `halt` / `nop` 循环。

`rgbfix` 负责头校验和、标题和 ROM 尺寸字节。源文件不必手写 0x8000 字节。

- [ ] **Step 2: 写构建脚本并生成 ROM**

`tools/build_rom.sh`：

```bash
#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
rgbasm -o roms/homebrew/pattern.o roms/homebrew/pattern.asm
rgblink -o roms/homebrew/pattern.gb roms/homebrew/pattern.o
rgbfix -v -p 0 -t AIPATTERN roms/homebrew/pattern.gb
rm -f roms/homebrew/pattern.o
```

```bash
chmod +x tools/build_rom.sh
tools/build_rom.sh
```

Expected: 退出码 0。`rgbfix -v` 不报校验和错误。若系统没有 `rgbasm`，安装发行版的 `rgbds` 包后再跑；不要改用下载来的 `.gb`。

- [ ] **Step 3: 写 roms/README.md**

写明：本目录只有自制 ROM；商业 ROM 禁止入库；可选的 MIT 测试 ROM（dmg-acid2、mealybug、mooneye-test-suite、age-test-roms）默认不入库，许可证与提交号见设计第 15.2 节；Blargg 待确认，不用。

- [ ] **Step 4: Commit**

```bash
git add roms tools/build_rom.sh
git commit -m "test: add homemade DMG pattern ROM"
```

---

### Task 3: 主机无头跑通与帧哈希

**Files:**
- Create: `components/gb_core/include/gb_core.h`
- Create: `components/gb_core/gb_core.c`
- Create: `tools/gb_host.c`
- Create: `tools/test_host.sh`
- Create: `roms/homebrew/pattern.sha256`
- Test: `tools/test_host.sh`

**Interfaces:**
- Consumes: Task 1 的头文件，Task 2 的 `pattern.gb`
- Produces: 设计第 9.1 节的全部 `gb_core_*` 符号。`gb_host` 退出码 0 表示像素合同和哈希都成立。

- [ ] **Step 1: 写会失败的主机测试**

`tools/gb_host.c` 的行为，而不是把核心提前写进这个文件：

- `--rom PATH --frames N` 加载 ROM，调用 `gb_core_save_size`，断言 RAM 尺寸为 0，再 `gb_core_init`。
- 每帧 `gb_core_run_frame`。回调里 `line >= 144` 则返回；否则把 160 字节的 `pixels[x] & 0x03` 写入 `144*160` 的缓冲。
- 第 2 帧和第 3 帧：`shade[0] == 3`，`shade[8] == 0`，`shade[143*160+159] == 0`，两帧 SHA-256 相同。
- 打印一行 `sha256 <64 hex>`。
- `--check FILE` 时与文件中的十六进制比较，不一致则退出码 1。
- `--expect-bad PATH` 把 ROM 的 `0x14D` 加 1 后初始化，必须得到 `GB_ERR_CHECKSUM`，否则退出码 1。

`tools/test_host.sh`：

```bash
#!/bin/sh
set -eu
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -O2 \
  -DENABLE_SOUND=0 \
  -I components/gb_core/include -I third_party/peanut-gb \
  -o build/gb-host \
  tools/gb_host.c components/gb_core/gb_core.c
./build/gb-host --rom roms/homebrew/pattern.gb --frames 3 \
  --check roms/homebrew/pattern.sha256
./build/gb-host --expect-bad roms/homebrew/pattern.gb
```

在 `gb_core.c` 仍是空文件时先跑一次。

- [ ] **Step 2: 确认现在失败**

```bash
chmod +x tools/test_host.sh
tools/test_host.sh
```

Expected: 编译失败，提示 `gb_core_init` 或相关符号未定义。

- [ ] **Step 3: 实现适配层**

`gb_core.h` 只放设计第 9.1 节的 typedef 和函数。结构体放在 `gb_core.c`：`struct gb_core { struct gb_s gb; const uint8_t *rom; size_t rom_size; uint8_t *cart_ram; size_t cart_ram_size; gb_line_fn draw; void *draw_ctx; };`。

`gb_core.c` 在包含 `peanut_gb.h` 之前定义 Global Constraints 里的五个宏。ROM 回调越界返回 `0xFF`。RAM 回调在 `cart_ram_size == 0` 或地址越界时读返回 `0xFF`、写忽略。`gb_error` 回调向 stderr 打地址后 `abort()`。`gb_core_run_frame` 把本次的 `draw` 存进 `core`，再调用 `gb_run_frame`。LCD 回调转给 `draw`。`gb_core_set_pressed` 写 `(uint8_t)~pressed_mask`。`gb_core_save_size` 用临时的 `struct gb_s` 做一次 `gb_init`（RAM 尺寸先按 0）再 `gb_get_save_size_s`；校验和或 MBC 失败映射到 `GB_ERR_CHECKSUM` / `GB_ERR_UNSUPPORTED`。

初始化成功后调用 `gb_init_lcd`。不要打开 `frame_skip` 或 `interlace`。

- [ ] **Step 4: 记录哈希并确认通过**

像素断言通过后，先不带 `--check` 跑一遍，把打印的 SHA-256 写入 `roms/homebrew/pattern.sha256`（一行 64 个十六进制字符）：

```bash
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -O2 \
  -DENABLE_SOUND=0 \
  -I components/gb_core/include -I third_party/peanut-gb \
  -o build/gb-host \
  tools/gb_host.c components/gb_core/gb_core.c
./build/gb-host --rom roms/homebrew/pattern.gb --frames 3
```

然后再跑：

```bash
tools/test_host.sh
```

Expected: 两次 `gb-host` 都退出 0。再跑一次 `tools/test_host.sh` 仍然退出 0。另外打印 `sizeof(struct gb_s)` 到构建日志里，数字记进 `third_party/peanut-gb/SOURCE.md` 的一行“主机 sizeof”，不要手填猜测。

- [ ] **Step 5: Commit**

```bash
git add components/gb_core tools/gb_host.c tools/test_host.sh roms/homebrew/pattern.sha256 third_party/peanut-gb/SOURCE.md
git commit -m "test: hash homemade ROM frames on the host"
```

---

### Task 4: 主机负例

**Files:**
- Modify: `tools/gb_host.c`
- Modify: `tools/test_host.sh`
- Test: `tools/test_host.sh`

**Interfaces:**
- Consumes: `gb_core_init`、`gb_core_save_size`
- Produces: 截断 ROM 与非法 MBC 的失败路径。不改变 `pattern.sha256`。

- [ ] **Step 1: 扩展 test_host.sh**

在现有两条命令之后加：

- 把 `pattern.gb` 截成 0x100 字节，`--rom` 必须退出码非 0（`GB_ERR_CHECKSUM` 或 `GB_ERR_BAD_ARG` 都可以，但必须是失败）。
- 把一份拷贝的 cart type 字节 `0x147` 改成 `0xFE`，`gb_core_save_size` 或 init 返回 `GB_ERR_UNSUPPORTED`。

- [ ] **Step 2: 跑测试，看到新断言失败**

```bash
tools/test_host.sh
```

Expected: 在补上这两支之前失败。

- [ ] **Step 3: 在 gb_host.c 实现这两支，核心逻辑仍只调用 gb_core**

不在测试里再写一套校验和。

- [ ] **Step 4: 通过**

```bash
tools/test_host.sh
```

Expected: 退出码 0。

- [ ] **Step 5: Commit**

```bash
git add tools/gb_host.c tools/test_host.sh
git commit -m "test: reject truncated and unsupported ROMs"
```

---

### Task 5: ESP-IDF 骨架，不改分区

**Files:**
- Create: `CMakeLists.txt`
- Create: `sdkconfig.defaults`
- Create: `partitions.csv`
- Create: `main/CMakeLists.txt`
- Create: `main/app_main.c`
- Create: `main/idf_component.yml`（若 BSP 需要组件管理器；Task 6 再填依赖）

**Interfaces:**
- Consumes: 无
- Produces: `idf.py build` 能产出 esp32c3 镜像。`app_main` 只打印一行 `ai-passport-gb boot`。

- [ ] **Step 1: sdkconfig.defaults**

至少包含：

```text
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

不要打开 `CONFIG_SPIRAM`、`CONFIG_BT_ENABLED`、Wi-Fi。不要在这一任务里改优化级别。

- [ ] **Step 2: partitions.csv**

复制上游 `33d3d1d` 的三行：nvs `0x9000/0x6000`，phy `0xf000/0x1000`，factory `0x10000/0x7F0000`。文件头注释写明来源提交，以及“在设计第 14 节问题 2 答复前不改”。

- [ ] **Step 3: 构建**

```bash
. $IDF_PATH/export.sh
idf.py --version
idf.py set-target esp32c3
idf.py build
```

Expected: `idf.py --version` 显示 5.5.3。构建日志含 `esp32c3`。镜像落在 factory 分区内（IDF 的分区检查通过）。把 `sdkconfig` 里 `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` 的实际值抄进构建说明注释或 `docs/benchmarks` 的草稿，标成“构建配置，不是运行测量”。

若环境没有 IDF 5.5.3，安装该版本后再构建。不要改用别的目标芯片来换取通过。

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt sdkconfig.defaults partitions.csv main
git commit -m "build: add ESP-IDF esp32c3 skeleton"
```

---

### Task 6: 导入 BSP

**Files:**
- Create: `components/bsp/**`（自 anki `09847914533da79e05325365eff65c700c4633ec` 的 `components/bsp` 复制）
- Create: `components/bsp/IMPORT.md`（覆盖或续写，不要丢掉上游许可证）
- Modify: `main/idf_component.yml`、`main/CMakeLists.txt`

**Interfaces:**
- Consumes: BSP 已有的 `bsp_display_init`、`bsp_display_panel`、`bsp_display_backlight`、`bsp_button_init`、`bsp_button_read_mv`
- Produces: 上述符号能被 `main` 链接。本任务的 `app_main` 仍不调用它们。

- [ ] **Step 1: 复制并记录**

`IMPORT.md` 写明：

- 上游 `https://github.com/FoloToy/ai-passport` `33d3d1d93a1125b356b47b6d83a7a60121be801e`
- 直接来源 anki `09847914533da79e05325365eff65c700c4633ec`
- 保留 `LICENSE`（MIT，版权 2026 FoloToy）
- 相对上游多出的 deep-sleep 文件若随 anki 副本而来，按 anki `IMPORT.md` 逐个列出，不在本任务里改它们

不要复制 anki 的 LVGL 演示 `main/`。`bsp_display_lvgl.c` 若随 BSP 一起编译，本应用不调用 `bsp_lvgl_init`。如果链接 LVGL 迫使工程依赖 LVGL，就从本工程的 BSP `CMakeLists.txt` 去掉 LVGL 源文件，并在 `IMPORT.md` 写明删了哪个文件、为什么。引脚头文件不动。

- [ ] **Step 2: 构建**

```bash
idf.py build
```

Expected: 仍通过。日志里不应出现“启用 PSRAM”。

- [ ] **Step 3: Commit**

```bash
git add components/bsp main
git commit -m "chore: import AI Passport BSP at the anki snapshot"
```

---

### Task 7: 把核心收成 IDF 组件

**Files:**
- Create: `components/gb_core/CMakeLists.txt`
- Modify: `components/gb_core/gb_core.c`（错误回调在设备上 `esp_restart`，主机仍 `abort`）
- Modify: `main/CMakeLists.txt`、`main/app_main.c`
- Test: `tools/test_host.sh`

**Interfaces:**
- Consumes: Task 3 的符号
- Produces: 固件里能调用 `gb_core_init` / `gb_core_run_frame`。行为与主机测试相同。

- [ ] **Step 1: 组件 CMake**

`REQUIRES` 只放 libc 和 IDF 的 `esp_system`（为了 `esp_restart`）。不要 `REQUIRES` LCD。用 `#if defined(ESP_PLATFORM)` 区分错误回调。主机编译不定义 `ESP_PLATFORM`。

- [ ] **Step 2: app_main 跑 3 帧并把哈希打到串口**

ROM 用 `target_add_binary_data` 链入 `roms/homebrew/pattern.gb`。`gb_rom_open_embedded` 可以先写成 `main` 里的一个静态函数，签名与设计 9.2 相同；若要独立组件，放到 `components/gb_rom/`。3 帧之后打印 `gb_hash sha256=<64 hex>`。不调用显示。

- [ ] **Step 3: 两边都验证**

```bash
tools/test_host.sh
idf.py build
```

Expected: 主机脚本退出 0。固件构建通过。串口对照留到 Task 9 的真机步骤；本任务的构建日志能看到 `pattern.gb` 被打进镜像即可。

- [ ] **Step 4: Commit**

```bash
git add components/gb_core main
git commit -m "feat: run Peanut-GB in the esp32c3 image"
```

---

### Task 8: 1:1 上屏

**Files:**
- Create: `components/gb_video/include/gb_video.h`
- Create: `components/gb_video/gb_video_lut.c`
- Create: `components/gb_video/gb_video.c`
- Create: `components/gb_video/CMakeLists.txt`
- Create: `tools/test_lut.c`
- Modify: `main/app_main.c`
- Test: `tools/test_lut.c` 锁 LUT 字节；`pattern.sha256` 不覆盖中间灰阶。显示用真机

**Interfaces:**
- Consumes: `bsp_display_init`、`bsp_display_panel`、`esp_lcd_panel_draw_bitmap`；`gb_line_fn` 的 160 字节行
- Produces: 设计第 9.3 节。`GB_SCALE_3_2` 返回 `ESP_ERR_NOT_SUPPORTED`。

- [ ] **Step 1: 实现 1:1**

`gb_video_lut.c` 不包含 IDF 头。`gb_video_shade_bytes(unsigned shade)` 返回设计 9.3 的两个字节：shade 1 是 `AD 55`，shade 2 是 `52 AA`。这是高字节在前，不是 `uint16_t` 的原生小端（原生小端会是 `55 AD` 和 `AA 52`）。

`tools/test_lut.c` 断言这两对字节，以及 shade 0 为 `FF FF`、shade 3 为 `00 00`。

```bash
cc -std=c11 -Wall -Wextra -Werror -I components/gb_video/include \
  -o build/test-lut tools/test_lut.c components/gb_video/gb_video_lut.c
./build/test-lut
```

Expected: 退出码 0。

`gb_video_init` 调 `bsp_display_init`，背光 50，用黑色填满 240×320 一次。然后在左边框用这张表画四块 8×8：坐标 `(0,88)`、`(0,96)`、`(0,104)`、`(0,112)`，shade 依次为 0、1、2、3。GB 窗口从 x=40 开始，这四块不盖住画面。行缓冲两份，各 `160*2` 字节，内部 RAM，DMA 对齐。`gb_video_push_line` 忽略 `line >= 144`，目标矩形为 `x [40,200)`、`y [88+line, 89+line)`。

- [ ] **Step 2: 帧循环把回调接到 push_line**

仍只跑自制 ROM。循环先不要做节拍，连续跑即可，避免在显示未证明之前引入等待。

- [ ] **Step 3: 构建**

```bash
idf.py build
tools/test_host.sh
./build/test-lut
```

Expected: 都成功。`pattern.sha256` 不变。`test-lut` 通过只说明内存字节与 9.3 一致，不能代替真机对面板字节序的判断。

- [ ] **Step 4: 用户真机**

持有板子的人执行，不要用会擦除整片 flash 的全片擦除，除非接受 NVS 被清空：

```bash
esptool.py --port PORT flash_id
idf.py -p PORT flash monitor
```

记录：

- `flash_id` 容量。与 8 MiB 不符就停，不要改 `sdkconfig` 去凑。
- 监视器里的 `gb_hash` 与 `roms/homebrew/pattern.sha256`。
- 一张照片：左上角相对深，其余相对浅，画面不贴着屏幕最上沿（居中留边）。
- 同一张或另一张照片要能看见左边框从上到下四块：亮、较亮、较暗、暗。中间两块如果发绿或发品红，说明 LUT 字节序反了。那时对调 shade 1 和 2 的字节，改 `test-lut` 的期望，并在 Q-SPI-SWAP 记下实际顺序。`pattern.gb` 只有 shade 0 和 3，不能发现这个问题。
- 若花屏：把 `BSP_LCD_PCLK_HZ` 临时改为 `40 * 1000 * 1000` 再刷一次，照片和“用了哪个宏”一起留下。默认仓库在原因写明之前保持 80 MHz 宏。

云端没有板子时，本任务以构建通过为止，PR 里写“真机未跑”。

- [ ] **Step 5: Commit**

```bash
git add components/gb_video tools/test_lut.c main
git commit -m "feat: present DMG frames 1:1 on the ST7789"
```

---

### Task 9: 节拍与分开的计时

**Files:**
- Create: `components/gb_sched/include/gb_sched.h`
- Create: `components/gb_sched/gb_sched.c`
- Create: `components/gb_sched/CMakeLists.txt`
- Create: `components/gb_bench/include/gb_bench.h`
- Create: `components/gb_bench/gb_bench.c`
- Create: `components/gb_bench/CMakeLists.txt`
- Modify: `main/app_main.c`

**Interfaces:**
- Consumes: `gb_core_run_frame`、`gb_video_push_line`、`esp_timer_get_time`
- Produces: 设计第 9.5 节。每秒一行 `emu_us` 与 `present_us`。`gb_sched_due` 只返回 0 或 1。

- [ ] **Step 1: 实现节拍**

间隔用整数微秒。`1000000 * 70224 = 16742 * 4194304 + 2962432`。每帧给累加器加 16742，另把 2962432 加进余数；余数满 4194304 就再加 1 微秒并减掉 4194304。落后超过一帧时丢掉积压，只补 1 帧。

测量时，先用一个只写 RAM 的回调跑 `gb_core_run_frame` 得到 `emu_us`，再把该帧的 144 行推到面板得到 `present_us`。不要在同一次回调里既模拟又量 SPI，以免两个数缠在一起。实现可以用两遍行缓冲：第一遍存 144 行的 shade（`144*160` 字节），第二遍推屏。`144*160 = 23040` 字节，来自分辨率算术，允许作为测量缓冲；稳态路径若以后改成单遍，测量定义不能改。

- [ ] **Step 2: 构建与主机回归**

```bash
idf.py build
tools/test_host.sh
```

Expected: 通过。固件日志格式固定为：

```text
gb_bench emu_us=<u64> present_us=<u64> emu_frames=<u32> present_frames=<u32> free=<u32> largest=<u32>
```

- [ ] **Step 3: 用户真机**

监视器里抄 10 行 `gb_bench`。这 10 行是样本，不是目标帧率。写入 `docs/benchmarks/` 时标“P1 基线，未改优化级别”。没有板子就留空表。

- [ ] **Step 4: Commit**

```bash
git add components/gb_sched components/gb_bench main docs/benchmarks
git commit -m "feat: pace frames and split emu from present time"
```

---

### Task 10: 三键

**Files:**
- Create: `components/gb_input/include/gb_input.h`
- Create: `components/gb_input/gb_input.c`
- Create: `components/gb_input/gb_input_decode.c`
- Create: `components/gb_input/CMakeLists.txt`
- Create: `tools/test_input_decode.c`
- Modify: `main/app_main.c`
- Test: 主机上可测的纯解码函数；真机验证手感

**Interfaces:**
- Consumes: `bsp_button_init`、`bsp_button_read_mv`、`BSP_BTN_MV_TABLE`、`BSP_BTN_LONG_PRESS_MS`
- Produces: 设计第 9.4 节的 `gb_input_init` / `gb_input_poll`。`gb_core_set_pressed` 使用 `out->pressed_mask`。

- [ ] **Step 1: 把电压窗口解码写成纯函数并在主机测试**

```c
/* 与 BSP 半开区间一致：命中返回 0,1,2；都不是返回 -1。 */
int gb_input_decode_mv(int mv);
```

主机测试：`mv` 为 -1、0、149、150、446、447、1899、1900、3300。期望：-1→-1，0 与 149→0，150 与 446→1，447 与 1899→2，1900 与 3300→-1。

```bash
cc -std=c11 -Wall -Wextra -Werror -I components/gb_input/include \
  -o build/test-input tools/test_input_decode.c components/gb_input/gb_input_decode.c
./build/test-input
```

把解码放在无 IDF 依赖的 `gb_input_decode.c`，方便主机编译。Expected: 退出码 0。

- [ ] **Step 2: 设备侧轮询**

`gb_input_init` 调用 `bsp_button_init`。回调只可入队；joypad 不在回调里改。每帧 `gb_input_poll` 读一次 mV。长按 OK 满 500 ms（用 `now_us` 与宏 `BSP_BTN_LONG_PRESS_MS` 比较）切换 `menu_open`。该次按下在松开前 `pressed_mask` 不含 OK 对应的 GB 键。菜单打开时掩码为 0，UP/DOWN 改变层，层的映射与设计 9.4 一致。

每秒额外打印 `gb_input mask=<02x> layer=<u> menu=<u>`。

- [ ] **Step 3: 构建**

```bash
idf.py build
./build/test-input
tools/test_host.sh
```

Expected: 全部退出 0。

- [ ] **Step 4: 用户真机**

按 UP、DOWN、OK，各按一次短按和一次长按。视频或监视器日志要能看出：短按 OK 在层 0 时掩码出现 `JOYPAD_A`（0x01），长按 OK 打开菜单且该次按住期间掩码没有 A。记录从按下到掩码变化的 `esp_timer` 差值，标为待测样本，不写成规格。

- [ ] **Step 5: Commit**

```bash
git add components/gb_input tools/test_input_decode.c main
git commit -m "feat: map the three-key ladder onto GB buttons"
```

---

### Task 11: 性能对照

**Files:**
- Create: `docs/benchmarks/README.md`
- Create: `docs/benchmarks/template.md`
- Modify: `sdkconfig.defaults` 仅当某一轮需要显式优化级别；每一轮用单独提交说明改了哪一个符号
- Modify: 热点函数的属性或链接片段，只在“IRAM 那一轮”

**Interfaces:**
- Consumes: Task 9 的 `gb_bench` 行
- Produces: 一张表。列是：构建（优化级别、是否 IRAM、是否双缓冲、frame_skip、interlace、请求的 PCLK）、`emu_us` 中位数、`present_us` 中位数、样本行数。没有的格子写 `待测`。

- [ ] **Step 1: 写空表**

行对应设计第 10.6 节的五步。单元格预填 `待测`。README 写明禁止把 RP2040-GB 的 70/120 和任何 PocketStar 数字抄进表。

- [ ] **Step 2: 确认帧循环里没有 ESP_LOG**

每秒一行可以留着。在 `gb_core_run_frame` 和 `gb_video_push_line` 路径上搜 `ESP_LOG`。Expected: 无匹配。

- [ ] **Step 3: 用户真机按五步各采 10 行**

每一步只改一个因素，刷写用分段 `idf.py -p PORT flash`，避免无故全片擦除。IRAM 链接失败就在该格写链接错误摘要，不要删测试 ROM 来腾地方。`-Os` 与 `-O2` 用：

```text
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_OPTIMIZATION_PERF=y
```

两轮不要同时为 y。

- [ ] **Step 4: Commit**

```bash
git add docs/benchmarks
git commit -m "docs: record GB timing measurements"
```

没有真机数据时，提交只含模板和 `待测`，提交说明里写“device column pending”。

---

### Task 12: 存档

**Files:**
- Create: `roms/homebrew/saver.asm`
- Create: `roms/homebrew/saver.gb`
- Create: `components/gb_save/include/gb_save.h`
- Create: `components/gb_save/gb_save.c`
- Create: `components/gb_save/CMakeLists.txt`
- Modify: `tools/gb_host.c`、`tools/test_host.sh`
- Modify: `main/app_main.c` 的菜单“保存”

**Interfaces:**
- Consumes: `gb_core_cart_ram`、`gb_core_cart_ram_size`、`nvs_set_blob`、`nvs_commit`
- Produces: 设计第 9.6 节。`size == 0` 返回 `GB_OK`。`size > 2048` 返回 `GB_ERR_SAVE` 且不写 NVS。

- [ ] **Step 1: 自制 2 KiB RAM 的 ROM**

第二个 RGBDS 程序。链接后用 RGBDS 手册里的这两个开关（`rgbfix(1)`：`-m` 写 `0x147`，`-r` 写 `0x149`）：

```bash
rgbfix -v -p 0xFF -t AISAVE -m 0x03 -r 1 roms/homebrew/saver.gb
```

`0x03` 是 MBC1+RAM+BATTERY。`0x149 = 1` 在 Peanut 的表里是 `0x800` 字节。程序在 cart RAM 第 0 字节写入 `0xA5`。这个写入只证明 ROM 会改 RAM。不要手改校验和。把这条命令放进 `tools/build_rom.sh`。

- [ ] **Step 2: 主机上的尺寸闸门**

对 `saver.gb`，`gb_core_save_size` 返回 2048。跑 3 帧后 cart RAM 第 0 字节是 `0xA5`。然后：

1. 用主机桩 `gb_save_store` 保存这 2048 字节。
2. 把缓冲全部写成 `0x00`。
3. `gb_save_load` 读回。
4. 断言第 0 字节是 `0xA5`。
5. 第 2 步和第 4 步之间禁止调用 `gb_core_run_frame`。ROM 自己会再写 `0xA5`，跑帧之后的断言不能证明 load。

对一个把头里 RAM size 改成 32 KiB 档的拷贝，`gb_save_store` 在不链接 NVS 的主机桩上返回 `GB_ERR_SAVE`，并且桩的存储不被写入。

```bash
tools/test_host.sh
```

Expected: 退出码 0。

- [ ] **Step 3: 设备 NVS**

`gb_save_store` 在 `size <= 2048` 时 `nvs_set_blob` + `nvs_commit`。键名 `ram`，命名空间 `gb`。失败把 `esp_err_to_name` 打进日志并返回 `GB_ERR_SAVE`。启动顺序固定为：分配并清零 cart RAM，`gb_core_init`，`gb_save_load`，打印，然后才允许 `gb_core_run_frame`。没有键时保持清零后的 RAM，返回 `GB_OK`，打印 `gb_save loaded byte0=00 missing`。有键时打印 `gb_save loaded byte0=<02x>`。菜单保存成功后另打印 `gb_save ok bytes=<size>`。

- [ ] **Step 4: 用户真机**

刷 `saver` 构建（或用菜单选 ROM；P0–P3 只有内嵌 ROM 时，这一步就是单独刷带 saver 的镜像）。第一次启动的日志里，第一帧之前必须是 `gb_save loaded byte0=00 missing`。保存后分段重启。重启后同样要在第一帧之前看到 `gb_save loaded byte0=a5`。出现在帧循环之后的 `a5` 不算通过，因为 ROM 每次启动都会写这个字节。再刷回 `pattern.gb` 的镜像时不要用全片擦除，除非打算清掉这次实验存档。

- [ ] **Step 5: Commit**

```bash
git add roms/homebrew/saver.asm roms/homebrew/saver.gb components/gb_save tools main
git commit -m "feat: store 2 KiB cart RAM in NVS"
```

---

### Task 13: 音频保持关闭

**Files:**
- Create: `components/gb_audio/include/gb_audio.h`
- Create: `components/gb_audio/gb_audio.c`
- Create: `components/gb_audio/CMakeLists.txt`
- Modify: `main/app_main.c`（启动时调用一次并忽略 `GB_ERR_DISABLED`）

**Interfaces:**
- Consumes: 无 I2S
- Produces: `gb_err_t gb_audio_init(void);` 永远返回 `GB_ERR_DISABLED`，函数体内不引用 `bsp_audio_init`。

- [ ] **Step 1: 写这个桩**

不要复制 `minigb_apu`。设计第 14 节问题 6 若被否决，另开计划再引入 `third_party/minigb_apu` 及其 `LICENSE`。

- [ ] **Step 2: 确认声音宏仍是 0**

```bash
rg -n "ENABLE_SOUND" components main tools
```

Expected: 生产编译里只有 `-DENABLE_SOUND=0` 或 `#define ENABLE_SOUND 0`。没有 `=1`。

- [ ] **Step 3: 构建与主机回归**

```bash
idf.py build
tools/test_host.sh
```

Expected: 通过。

- [ ] **Step 4: Commit**

```bash
git add components/gb_audio main
git commit -m "feat: leave GB audio disabled"
```

---

## 计划自检

| 设计要求 | 任务 |
| --- | --- |
| 主机先跑通，帧哈希 | Task 2、3、4 |
| 不改分区的最小上屏 | Task 5、6、7、8 |
| 模拟时间与推屏时间分开 | Task 9 |
| 三键，不依赖和弦 | Task 10 |
| IRAM、DMA、关日志、`-O2`/`-Os` | Task 11 |
| cart RAM 到 NVS，超限失败 | Task 12 |
| 音频预留且默认关 | Task 13 |
| 真机步骤单独标出 | Task 8、9、10、11、12 |
| 许可证文件随第三方代码落地 | Task 1、6；minigb_apu 不在本计划落地 |
| 每个性能数字有来源或标待测 | Task 11 的表；设计第 5 节的计算不写进基准表冒充测量 |

P5 音频不在本计划的完成定义里。Task 13 只保证它没有被打开。
