# Battle City — C + SDL3

基于 Famicom/NES《Battle City》的可玩重制：全部 35 个关卡、单人/双人模式和关卡推进。游戏逻辑用 C11 重写，SDL3 负责窗口、输入、显示和音频输出。画面直接读取 ROM 的 CHR 图块；关卡地形、敌人编成与出生时序来自 ROM 数据；声音通过 libgme 运行 ROM 中的原版声音驱动，游戏规则不运行 NES CPU。

## ROM 与仓库内容

本仓库**不包含** `BattleCity.nes`，也**不包含**从 ROM 或模拟器导出的资源：PNG、PPU/RAM 快照、精灵裁剪、CSV 证据以及 `stages.h`、`stage1.h`、`result_screen.h`、`gameover_screen.h`。这些文件都由 `tools/` 下的脚本从你自备的 ROM 生成，已加入 `.gitignore`。

请通过合法途径取得 Battle City (Famicom/NES) ROM。当前关卡表、标题和调色板适配的 ROM SHA-256：

```text
a869aead5b6957fc62002ff9636e048cc34baf0100d629b07dc51aa18f220c0c
```

ROM 读取器验证 iNES/NROM 布局；导出脚本还会校验哈希，同布局的其他游戏或修改版不保证适配。

## 法律声明

Battle City © 1985 Namco Ltd.。本项目与 Namco/Bandai Namco 无关联，不提供、分发或授权任何 ROM、CHR 图块、音乐或其他原版资源。导出的头文件、截图和音频证据仅用于本地开发与验证，请勿提交到公开仓库。源码采用 [MIT License](LICENSE)，该许可不适用于原版 ROM 或从 ROM 导出的资源。

## 快速开始

构建依赖：

- C11 编译器、CMake、pkg-config
- SDL3 开发库（3.2 或更新）、libgme 开发库（0.6.3 或更新）
- Python 3
- FCEUX（带 Lua 支持），仅用于从 ROM 捕获标题、结算和 GAME OVER 名称表

把 ROM 放在仓库根目录，或使用任意路径。先导出生成的 C 头文件：

```sh
python3 tools/export_all.py --rom /path/to/BattleCity.nes --out build/generated
```

然后配置和构建：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DBATTLECITY_ROM=/path/to/BattleCity.nes
cmake --build build -j
./build/battlecity --rom /path/to/BattleCity.nes
```

默认生成目录是 `build/generated`。如果跳过手动导出，CMake 在构建时会自动调用同一脚本；只要提供 `BATTLECITY_ROM` 即可。

`BATTLECITY_FCEUX` 可以指定 FCEUX 可执行文件路径。未指定 `BATTLECITY_ROM` 时，编译出的程序默认查找仓库根目录的 `BattleCity.nes`，仍可用 `--rom` 覆盖。没有任何 ROM 或生成头文件时，CMake 会给出明确的导出提示并停止配置。

运行游戏本身不需要 Python、FCEUX、SDL_image 或 SDL_mixer；它们只在导出资源时使用。

| 按键 | 操作 |
| --- | --- |
| Enter | 标题开始 / 关卡选择开始 / 暂停 / 继续 / 通关后选下一关 / 跳过战场 GAME OVER、失败结算或砖块 GAME OVER / 退出 Construction |
| 标题：上 / 下 / 左 / 右 / Tab | 在 1 PLAYER / 2 PLAYERS / CONSTRUCTION 之间移动光标 |
| 1 / 2 | 在标题画面直接开始单人 / 双人 |
| 关卡选择：方向键 | 选择 1–35 关 |
| 关卡选择：Enter / Space | 开始当前关 |
| Construction：方向键 / WASD | 以 16 像素格移动编辑光标 |
| Construction：Space / J / Z | 放置方块；静止时后续按下切换到下一种方块 |
| Construction：X / Backspace | 放置方块；静止时后续按下切换到上一种方块 |
| P1：WASD | 移动 |
| P1：Space / J / Z | 开火，可按住 |
| P2：方向键 | 移动 |
| P2：K / RShift / 小键盘 0 | 开火 |
| 手柄：第一个 / 第二个 | 分别控制 P1 / P2；十字键或左摇杆移动，A 开火，B 在 Construction 中反向循环 |
| 手柄：Start / Back | 开始、暂停、确认 / 在标题切换菜单光标 |
| P | 暂停 / 继续 |
| R | 重开当前关 |
| F12 | 保存当前画面到 `battlecity.bmp` |
| Escape | 退出 |

命令行 `--stage N`（1–35）可以从指定关卡开始新游戏，`--players 2` 直接启动双人模式。

SDL3 手柄支持热插拔：启动时和运行中插入的手柄会按连接顺序分配，第一个控制 P1，第二个控制 P2；单个手柄在单人模式和 Construction 中同时接受十字键与左摇杆。键盘输入始终可用，并与手柄输入合并。

窗口支持缩放，采用 256 × 240 逻辑分辨率、整数倍像素显示。游戏逻辑固定 60 Hz，失去窗口焦点自动暂停。

## 当前完成度

- 从 ROM 的 `$F07A` 解码全部 35 关：26 × 26 个 8px 地形格、砖墙半块图案、钢墙变体、水／树林／冰面，以及单独绘制的基地和鹰墙。
- 从 ROM 的 `$E4EC`／`$E578` 读取每关敌人编成，按基础／快速／强力／装甲顺序生成；出生点按原版中／右／左轮换，最多 4 辆在场。
- 从 ROM 实时解码标题、字体、坦克、子弹、地形、出生闪光、护盾和爆炸图块；标题恢复原版画面。
- 标题全屏 61,440 像素、关卡选择画面全屏、第一关静态地图与 HUD 的 60,512 像素与 FCEUX 基准零差异；第一关明确排除动态精灵覆盖的 928 像素。第 2、3、16、35 关的名称表地形与 FCEUX 快照逐格一致（战斗造成的破坏除外）。
- 标题支持 1 PLAYER / 2 PLAYERS / CONSTRUCTION 三档光标；关卡选择画面还原原版灰色幕布、`STAGE` 字样和 1–35 关数字，方向键选关，Enter/Space 开始。
- Construction 模式：按 ROM 的 13 × 13 个 16 像素块编辑战场，灰框、裸鹰和砖墙布局与原版一致，编辑光标按原版 16 帧显示／16 帧隐藏的节奏闪烁。方向键以 16 像素格移动光标，首次按键立即移动、长按 20 帧后每 5 帧重复；A 键绘制并向前循环 14 种 TSA 地形块，B 键绘制并向后循环，长按方向时可连续绘制。Start 保存地图返回标题，随后从 1P/2P 进入关卡选择并开始即可游玩自制地图；与原版一样，该地图只用于下一关，之后恢复 ROM 关卡数据，出生点下方的方块会在坦克出生时清除。
- 使用 ROM 中各方向的坦克、子弹和 8×16 精灵布局；四方向玩家、基础敌人、出生闪光、护盾、向上子弹的选定截图区域与原版零差异。
- 道具系统：剩余 17／10／3 辆时生成的闪烁敌人被击毁后掉落道具；包含头盔、时钟、铁锹、星星、手雷和额外生命六种，拾取得 500 分并立即生效。
- 头盔提供 640 帧护盾，时钟冻结敌人 640 帧，铁锹把鹰墙换成钢墙 1280 帧，星星提升玩家等级（1–2 星 4px 子弹、3 星可击穿钢墙），手雷清除在场敌人，额外生命增加一条命。
- 原版声音驱动生成开场音乐、开火、砖墙／钢墙命中、敌人／玩家／基地爆炸、暂停、道具、结算逐辆计分音、通关音乐、失败音乐及行驶／怠速音频，SDL3 播放，支持无窗口 WAV 导出。
- 玩家移动按 ROM 节奏每 4 帧移动 3 像素；普通敌人每 2 帧 1 像素、快速敌人每帧 1 像素；普通子弹每帧 2 像素。
- 敌人方向只在 8px 网格上以 1/16 概率重选，目标按关卡内时间从玩家切换为随机游走再切换为鹰基地；敌人开火为每帧 1/32 概率。
- 砖墙按 4px 子块破坏，钢墙阻挡，双方子弹均能击毁基地；水阻挡坦克、树林覆盖坦克、冰面保持滑行。
- 双人模式：P1/P2 独立移动、开火、分数、生命、复活和星星等级；2P 最多 6 辆敌人，出生间隔按 ROM 缩短 20 帧。
- SDL3 手柄：前两个连接的手柄分别控制 P1/P2，支持十字键、左摇杆、A/B、Start/Back 和运行中热插拔；键盘与手柄输入可同时使用。
- 出生闪光、出生保护、复活、暂停、失败、关卡完成与进入下一关；暂停时冻结战场并显示原版五枚 8×16 精灵拼成的 `PAUSE`，每人 3 条生命，分数与生命跨关保留。
- 关卡结算：按敌人类型统计击杀数和得分，显示 HI-SCORE、STAGE、1P/2P 分数与 TOTAL；每个类型组内逐辆间隔 9 帧累加，类型组之间间隔 30 帧（1P 1/2/3/4 击杀示例的类型起始为 33/72/120/177，TOTAL 在第 251 帧出现），逐辆累加时播放原版 ROM 槽位 19/20 的计分音。2P 行 9 还原原版“P1 分数 + P2 分数”的组合显示及右侧重复的 P2 分数。
- 失败流程：损失发生后战场继续运行约 127 帧，随后场地中央显示原版四枚 8×16 精灵拼成的 `GAME OVER` 文字约 130 帧；之后进入结算，结算完成后显示砖块大字 `GAME OVER` 约 114 帧，再按原版 1 像素/帧速度把标题画面从下往上卷入。
- 高分在同一进程内更新，标题和结算画面都会显示当前高分。
- 奖命规则：鹰基地存活时，任一玩家分数达到 20000 分即增加一条生命，每名玩家每局仅一次（对应 ROM `$D13A`）。
- 确定性随机数、无窗口自动演示、截图、核心回归测试和 SDL3 集成测试。

这是一版可玩的重制，仍在逐帧校准：随机 AI 的具体路径、敌人射击与出生时序在长时间回放中不会与原版完全一致。音频复用原版驱动，但多事件混音、声道切换与触发时序尚未逐采样比对；结算逐辆计分音已通过 ROM 槽位 19/20 接入。Construction 地图和原版一样只保存在进程内存中，不能导出或持久化；手柄映射按 SDL3 标准布局，特殊第三方控制器依赖 SDL 的游戏手柄数据库。验证范围见 [ALIGNMENT.md](assets/reference/ALIGNMENT.md)。

## 验证

```sh
ctest --test-dir build --output-on-failure
./build/battlecity --headless --demo --frames 1200 --screenshot build/demo.bmp
./build/battlecity --headless --demo --frames 1200 --audio-out build/demo.wav
./build/battlecity --headless --stage 16 --frames 1 --screenshot build/stage16.bmp
./build/test_sound build/sound-preview.wav
```

`--headless` 使用 SDL3 dummy 视频驱动和软件渲染，不依赖桌面会话；`--frames` 以确定性更新方式执行指定帧数。`--demo` 自动移动射击，`--start` 跳过标题。更多选项见 `--help`。

普通运行默认有声；`--mute` 关闭设备播放。`--headless` 与 `--frames` 不打开音频设备，仍可通过 `--audio-out` 导出 44.1 kHz、16 位单声道 WAV。音频试听文件依次包含开场、怠速、行驶、开火、砖墙、钢墙、敌人、玩家、基地、暂停、道具、结算逐辆计分音、通关音乐和失败音乐。

`reference_pixels` 自动输出 `build/reference-validation/report.json`、实际 PNG 和静态画面差异图（紫色为差异，灰色为排除的精灵区域）；当前覆盖标题（1P 与 CONSTRUCTION 光标）、标题卷入动画、砖块 GAME OVER、战场 GAME OVER 文字裁剪、暂停 PAUSE 裁剪、Construction 光标显示帧／隐藏帧／空地图／绘制后地图、1P/2P 结算动画的多个逐辆累加与 TOTAL 帧、关卡选择全屏和第一关静态画面。参照帧不随仓库提供。先生成完整参照目录：

```sh
python3 tools/capture_all_references.py --rom /path/to/BattleCity.nes --out build/reference
```

捕获先写入临时目录，验证全部场景和精灵裁剪后才会更新目标目录；缺帧或失败时返回非零并保留部分输出，不会把旧截图当作新结果。再配置 `-DBATTLECITY_REFERENCE_TESTS=ON -DBATTLECITY_REFERENCE_DIR=build/reference`。显式启用时，ROM、Python 3 或参照目录任一缺失都会让 CMake 配置失败；比较脚本开始前也会列出所有缺失的 PNG、RAM 和精灵裁剪。`menu` 通过实际的菜单按键处理验证 Construction 中 Space 绘制而不退出、Enter 保存退出，以及标题开始 1P/2P 时菜单人数同步；`gamepad` 通过注入设备枚举隔离物理手柄，验证第三只手柄在拔掉已分配手柄后补位并控制 P2；`sound` 检查原版声音请求（含结算逐辆计分音）、音乐与短音效的混音、确定性、停止和静音；`sdl3_smoke` 同时生成 `smoke.bmp` 和 `smoke.wav`。

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan -j
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

`menu` 通过实际按键处理覆盖 Construction 的 Space/Enter 行为、地图保存和标题人数同步；`gamepad` 覆盖三只手柄的连接、移除和补位；`capture_pipeline` 用故障注入验证缺帧捕获失败且不更新旧参照；核心测试覆盖边界与地形碰撞、砖墙分层破坏、钢墙阻挡、击杀计分、基地受击、出生保护、复活、通关、子弹抵消、暂停冻结与 PAUSE 精灵、35 关地形与敌人编成、出生顺序、玩家与子弹速度、双人独立移动/计分/生命、六种道具效果、分类击杀统计、1P/2P 结算累加时序与逐辆计分音、Construction 光标移动/自动重复/方块循环/绘制与自制地图进入关卡、战场 GAME OVER 文字阶段、GAME OVER／标题卷入流程、20000 分奖命（含死亡后下一帧奖命取消失败）、基地击毁后同帧击杀的高分更新、与槽位无关的子弹抵消、一星单弹与二星/三星双弹、六位高分显示、树林透明孔洞和非零像素遮挡、树林下的 PAUSE/战场 GAME OVER 提示、结算高分更新、声音事件及 5000 次更新的确定性检查。SDL3 集成测试实际渲染 1200 帧并保存 BMP 和 WAV。Sanitizer 命令关闭 SDL/系统库的泄漏检查，地址检查和未定义行为检查保持启用。

## ROM 分析与参照数据

所有导出脚本、参数和可选 FCEUX 捕获流程见 [tools/README.md](tools/README.md)。构建所需的生成头文件由：

```sh
python3 tools/export_all.py --rom /path/to/BattleCity.nes --out build/generated
```

写入 `build/generated`，并在配置时通过 `BATTLECITY_GENERATED_DIR` 指定。CMake 也可以在提供 `BATTLECITY_ROM` 时自动执行同一脚本。

生成的 C 头文件、PNG、PPU/RAM 快照、精灵裁剪和 CSV 均被 `.gitignore` 排除，不属于仓库内容：

- `build/generated/stages.h`：ROM `$F07A` 的 35 关地形与 `$E4EC`／`$E578` 敌人编成。
- `build/generated/stage1.h`：标题、第一关名称表、调色板和 FCEUX RGB 表。
- `build/generated/result_screen.h`、`gameover_screen.h`：结算和砖块 GAME OVER 的静态背景。
- `assets/reference/` 下的 PNG、`*-ppu.bin`、`*-ram.bin`、`sprites/`、`tally-ticks-*.csv`、`sound-*.csv`：仅在运行像素/音频参照测试或人工核对时本地生成。
- 校验脚本：`tools/inspect_rom.py`、`tools/compare_reference.py`、`tools/export_trace.py`、`tools/capture_*.lua`。

- `src/game.c`：独立于 SDL 的规则和固定步长更新。
- `src/rom.c`、`src/render.c`：ROM 读取与软件像素渲染。
- `src/main.c`：SDL3 主循环、键盘输入、命令行及截图。
- `src/sound.c`：ROM 声音驱动包装、声道优先级、PCM 生成和 WAV 输出。

后续重点是：用更长的 FCEUX 输入回放校准随机 AI 与射击时序，继续处理剩余音效和多事件混音对比，并把音频事件与波形逐采样对照。当前没有完成全 ROM 反汇编。
