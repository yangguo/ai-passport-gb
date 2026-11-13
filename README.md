# ai-passport-gb

在 FoloToy AI Passport（ESP32-C3 + ST7789）上移植原版 Game Boy（DMG）模拟器。仓库包含自制 pattern ROM 和可再分发的测试／自制 ROM，不含商业 ROM 或任天堂 boot ROM。ROM 来源、许可证和固定哈希见 [测试 ROM 说明](docs/test-roms.md)；Blargg 仅在本地下载，不随仓库分发。

项目代码采用 [MIT 许可证](LICENSE)。Peanut-GB、BSP 和测试 ROM 保留各自的许可证，根许可证不取代第三方声明。

主机验证：

```sh
./tools/test_host.sh
./tools/test_video.sh
python3 tools/test_pack_scripts.py
./tools/test_third_party.sh
```

ESP-IDF 5.5.3 环境下，`idf.py -B idf-build build` 默认内嵌 pattern ROM。模拟器复测使用 `./tools/passportsim setup` 和 `./tools/test_sim_roms.sh`；CI 仅手动触发。

当前包含主机测试、1:1 显示固件和设备测试包。真机颜色、按键、SPI 稳定性和性能仍需按 [设备测试说明](docs/device-test.md) 验收；模拟器的虚拟时间不能代替真机性能。审核和验证记录见 [2026-11-13 记录](docs/benchmarks/2026-11-13-review-validation.md)。

设计：`docs/design/2026-11-11-ai-passport-gb-design.md`

计划：`docs/plans/2026-11-11-ai-passport-gb-implementation-plan.md`
