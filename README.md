# HTTransfer

**High-Performance Local File Copy Engine for Windows**

> **Version**: v0.1.0-alpha.7  
> **Language**: C++20 + Qt6  
> **Platform**: Windows 10/11/Server  
> **License**: Private

HTTransfer 是一款世界级的本地文件复制引擎，定位类似 FastCopy / TeraCopy / Robocopy（GUI 版）。核心能力：超大文件(TB级)稳定复制、多线程高性能、自动断点续传、数据完整性校验、定时增量备份、系统托盘后台运行。

---

## ✨ 功能特性

### 核心传输能力
- **多线程并行传输** — ReaderPool(N个并行读取线程) → ConcurrentQueue → WriterThread(单线程写入) 流水线架构
- **超大文件支持** — 16MB 分块 I/O + 循环读写(每次≤2GB)，稳定复制 TB 级文件
- **断点续传** — `.htresume` 续传文件，批量刷盘优化(每4个chunk写一次)，崩溃后可恢复
- **完整性校验** — BLAKE3 / SHA-256 / CRC32 多算法校验，确保数据一致性
- **限速控制** — 令牌桶(Token Bucket)算法集成到 WriterThread，精确限速
- **SMB 检测** — 自动检测 SMB/UNC 源路径，降级为单线程读取保证稳定性
- **中文路径** — 完整 UTF-8/CJK 路径支持，`utf8ToPath()` / `pathToUtf8()` 转换层

### 定时增量备份 (Alpha-6)
- **文件变动监控** — 定时轮询 + 修改时间/大小对比检测文件变动
- **自动增量备份** — 检测到新增/修改文件后自动备份到目标路径，保持相对路径结构
- **扫描间隔配置** — 5秒 / 10秒 / 自定义(1~3600秒)
- **防抖机制** — 备份进行中跳过扫描周期，避免任务堆积
- **单一按钮入口** — 绿色"增量备份" ↔ 红色"停止备份"

### 系统托盘与开机自启动 (Alpha-7)
- **后台运行** — 关闭窗口隐藏到系统托盘，不退出程序
- **托盘菜单** — 右键"显示主窗口" / "退出"，双击显示窗口
- **气泡通知** — 后台运行时任务完成/失败通知
- **开机自启动** — "随系统启动"复选框，通过注册表实现
- **配置持久化** — 自动保存/恢复源路径、目标路径、传输选项等配置

### GUI 功能
- **Qt6 原生界面** — 简洁响应式 Windows GUI
- **实时进度** — 传输百分比、速度(MB/s)、剩余时间
- **语言切换** — English / 中文，Help > Language 菜单
- **文件/目录选择** — 独立的文件和目录浏览按钮
- **应用图标** — 桌面/任务栏/exe 图标

---

## 🏗️ 架构设计

### 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│                        GUI (Qt6)                              │
│            MainWindow / WatchConfigDialog /                   │
│            SystemTrayManager / AboutDialog                    │
├──────────────────────────────────────────────────────────────┤
│                       TaskManager                             │
│         State Machine · Progress · Scheduling                 │
├──────────┬──────────┬───────────┬──────────┬─────────────────┤
│FileEngine│VerifyEng │ResumeEng  │Transfer  │  WatchSession   │
│  Scan    │ BLAKE3   │ .htresume │ Engine   │  FileWatcher    │
│  Prealloc│ SHA-256  │  Batched  │ ┌──────┐ │  BackupScheduler│
│  Resolve │ CRC32    │  Crash    │ │Reader│ │                 │
│          │  Report  │  Recovery │ │Pool N│ │                 │
│          │          │           │ │  ↓   │ │                 │
│          │          │           │ │Queue │ │                 │
│          │          │           │ │  ↓   │ │                 │
│          │          │           │ │Writer│ │                 │
│          │          │           │ │  1   │ │                 │
│          │          │           │ └──────┘ │                 │
├──────────┴──────────┴───────────┴──────────┴─────────────────┤
│              IDataSource / IDataSink                          │
│         LocalFileSource / LocalFileSink                       │
│    (CreateFileW / ReadFile / WriteFile with loop I/O)         │
├──────────────────────────────────────────────────────────────┤
│                   Core Infrastructure                         │
│  BufferPool · SpeedController(Token Bucket)                   │
│  Logger(SQLite Prepared Statements + Daily Rotation)          │
│  AppConfigManager(QSettings) · AutoStartManager(Registry)     │
└──────────────────────────────────────────────────────────────┘
```

### 传输流水线

```
startTransferReaderWriter()
├── isSMB(source_path) → reader_count = 1 (SMB单线程)
├── ConcurrentQueue (bounded, capacity = reader_count × 4)
├── ReaderPool (N threads)
│   ├── 独立 LocalFileSource 句柄
│   ├── 原子 next_chunk_index 分发工作
│   ├── 循环 Read (每次≤2GB)
│   ├── 失败时 signalWriterError() 传播错误
│   └── RAII join 析构
├── WriterThread (1 thread)
│   ├── 单一 LocalFileSink 句柄
│   ├── 循环 Write (每次≤2GB)
│   ├── SpeedController waitForTokens (分批消费)
│   ├── markChunkCompleted → ResumeEngine (批量每4个chunk)
│   └── RAII join 析构
└── Progress callback → TaskManager (mutex保护)
```

### 关键设计决策

| 决策 | 理由 |
|------|------|
| ReaderPool → Queue → WriterThread 流水线 | 消除多写入者SMB不稳定问题；目标文件单句柄 |
| `IDataSource` / `IDataSink` 抽象 | 解耦传输逻辑与数据源/目标 specifics |
| `utf8ToPath()` / `pathToUtf8()` | 避免 Windows ANSI codepage 陷阱 |
| 循环读写(每次≤2GB) | 解决 DWORD 截断(>4GB)和部分写入问题 |
| SQLite Prepared Statements | 防止审计日志 SQL 注入 |
| RAII join 析构 | 防止异常时 std::terminate |
| 令牌桶分批消费 | 处理 chunk 超过桶容量时的无限循环 |
| 定时轮询 vs QFileSystemWatcher | 复用 FileEngine.scanDirectory，间隔可控，实现简洁 |
| QSettings vs SQLite 配置 | 配置简单键值对，QSettings 更轻量 |
| 注册表 vs 启动文件夹 | 注册表更可靠，支持命令行参数 |

---

## 🧮 先进算法

### 1. 令牌桶限速 (Token Bucket)

```
waitForTokens(bytes):
  while remaining > 0:
    lock:
      refillTokens()  // 按时间差补充令牌
      consume = min(remaining, bucket_capacity)
      if tokens >= consume:
        tokens -= consume
        remaining -= consume
    sleep(10ms)
```

**特点**：分批消费解决 chunk 超过桶容量时的无限循环问题。

### 2. 增量变动检测 (FileWatcher)

```
scanAndDetect():
  current = buildSnapshot(scanDirectory(source))
  if first_scan:
    baseline = current  // 首次建立基线，不触发全量备份
    return {}
  events = detectChanges(current, baseline)
    for each file in current:
      if not in baseline → Created event
      if modify_time or size changed → Modified event
  baseline = current
  return events
```

**特点**：修改时间 + 文件大小双维度对比，首次扫描建立基线不触发全量备份。

### 3. 批量续传刷盘 (Batched Resume)

```
markChunkCompleted(chunk_id):
  cache_[task_id].completed_chunks.push_back(chunk_id)
  pending_writes++
  if pending_writes >= 4:
    flushPendingWrites()  // 每4个chunk写一次磁盘
    pending_writes = 0
```

**特点**：减少磁盘 I/O 次数，提升续传性能。

### 4. 防抖备份调度 (Debounce)

```
onScanTimeout():
  if backup_in_progress:
    skip  // 备份进行中跳过本次扫描
  else:
    backup_in_progress = true
    events = scanAndDetect()
    if events not empty:
      executeBackup(events)  // 完成后复位
    else:
      backup_in_progress = false
```

**特点**：避免备份任务堆积，确保前一次备份完成后再启动新一轮。

### 5. BLAKE3 哈希校验

采用 BLAKE3 算法，比 SHA-256 快 10-20 倍，支持 SIMD 加速，适合大文件校验。

---

## 📁 项目结构

```
HTTransfer/
├── App/                        # 应用入口
│   ├── main.cpp                # WinMain + --version/--minimized
│   ├── Application.h           # 应用组装
│   └── CMakeLists.txt
├── Core/                       # 核心业务逻辑
│   ├── Common/                 # Types.h, Result.h, Constants.h, ErrorCodes.h
│   ├── Domain/                 # TransferTask, ChunkManifest, TransferPreset, ResumeFile
│   ├── TaskManager.h/cpp       # 任务管理器
│   ├── FileEngine.h/cpp        # 文件扫描/预分配
│   ├── SpeedController.h/cpp   # 令牌桶限速
│   ├── LocalFileSource.h/cpp   # IDataSource 实现
│   ├── LocalFileSink.h/cpp     # IDataSink 实现
│   └── IDataSource.h/IDataSink.h
├── Transfer/                   # 传输引擎
│   ├── TransferEngine.h/cpp    # Reader→Queue→Writer 编排
│   ├── DataChunk.h             # 数据块结构
│   ├── ConcurrentQueue.h/cpp   # 线程安全有界阻塞队列
│   ├── ReaderPool.h/cpp        # N并行读取线程
│   └── WriterThread.h/cpp      # 单写入线程
├── Watch/                      # 定时增量备份
│   ├── FileWatcher.h/cpp       # 文件变动检测器
│   ├── WatchSession.h/cpp      # 监控会话状态机
│   ├── BackupScheduler.h/cpp   # 备份调度器(QTimer)
│   └── WatchTypes.h            # 类型定义
├── Verify/                     # 完整性校验
│   ├── VerifyEngine.h/cpp      # 多算法校验
│   ├── Blake3Calculator.h/cpp  # BLAKE3
│   ├── SHA256Calculator.h      # SHA-256
│   └── CRC32Calculator.h       # CRC32
├── Resume/                     # 断点续传
│   └── ResumeEngine.h/cpp      # .htresume 批量刷盘
├── Logger/                     # 审计日志
│   └── Logger.h/cpp            # SQLite Prepared Statements
├── Config/                     # 配置管理
│   ├── AppConfigManager.h      # QSettings 持久化
│   └── AutoStartManager.h      # 注册表自启动
├── GUI/                        # Qt6 界面
│   ├── MainWindow.h            # 主窗口
│   ├── SystemTrayManager.h     # 系统托盘
│   ├── WatchConfigDialog.h     # 增量备份配置
│   └── AboutDialog.h           # 关于对话框
├── Resources/                  # 应用资源
│   ├── app.ico / app.png       # 图标
│   └── resources.qrc           # Qt 资源
├── Tests/                      # 单元测试 (58项)
│   ├── CoreTests/              # 12 tests
│   ├── TransferTests/          # 26 tests
│   ├── VerifyTests/            # 13 tests
│   └── ResumeTests/            # 7 tests
└── CMakeLists.txt              # 根构建配置
```

---

## 🚀 构建指南

### 前置条件

| 工具 | 版本 | 说明 |
|------|------|------|
| Visual Studio 2022 | BuildTools | C++20 支持 |
| CMake | ≥ 3.25 | 构建系统 |
| Ninja | ≥ 1.11 | 构建执行器 |
| Qt6 | 6.8.2 | MSVC 2022 x64 |
| vcpkg | 最新 | 包管理器 |

### 1. 安装 Qt6

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.8.2 msvc2022_64 -O C:\Qt6
```

### 2. 安装 vcpkg 依赖

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
vcpkg install openssl:x64-windows sqlite3:x64-windows blake3:x64-windows curl:x64-windows libssh2:x64-windows zstd:x64-windows gtest:x64-windows
```

### 3. 编译

```powershell
# 设置 MSVC 环境
$env:INCLUDE = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include;..."
$env:LIB = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64;..."
$env:PATH = "C:\vcpkg\downloads\tools\cmake-4.3.3-windows\cmake-4.3.3-windows-x86_64\bin;..." + $env:PATH

# 配置
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# 编译
ninja -C build-release
```

### 4. 运行

```powershell
.\build-release\App\HTTransfer.exe            # GUI 模式
.\build-release\App\HTTransfer.exe --version  # 打印版本
.\build-release\App\HTTransfer.exe --minimized # 最小化启动
```

---

## 📖 使用指南

### 基本文件复制

1. **选择源路径** — 点击"文件"选择单文件，或"目录"选择文件夹
2. **选择目标路径** — 点击"..."浏览目标目录
3. **配置选项**：
   - 多线程 | 覆盖 | 断点续传 | 校验 | 限速 | 线程数
4. **点击开始** — 实时监控进度
5. **暂停/继续/停止** — 随时控制传输

### 定时增量备份

1. **点击绿色"增量备份"按钮** — 弹出配置对话框
2. **选择源目录和目标目录**
3. **选择扫描间隔** — 5秒 / 10秒 / 自定义
4. **点击"开始备份"** — 按钮变红色"停止备份"
5. **状态栏显示** — 监控运行中 | 间隔 | 已检测 | 已备份
6. **点击"停止备份"** — 停止监控

### 系统托盘

- **关闭窗口** → 隐藏到系统托盘后台运行
- **双击托盘图标** → 显示主窗口
- **右键托盘图标** → "显示主窗口" / "退出"
- **退出** → 保存配置并安全关闭

### 开机自启动

- 勾选"随系统启动"复选框 → 写入注册表
- 系统启动时自动运行，最小化到系统托盘
- 取消勾选 → 删除注册表项

### 传输预设

| 预设 | 多线程 | 校验 | 限速 |
|------|--------|------|------|
| Fast | 关 | 关 | 关 |
| Balanced | 开 | 开 | 关 |
| Secure | 开 | 开 | 开 |

---

## 🧪 测试

```powershell
.\build-release\Tests\TransferTests\ht_transfer_tests.exe  # 26 tests
.\build-release\Tests\ResumeTests\ht_resume_tests.exe      # 7 tests
.\build-release\Tests\VerifyTests\ht_verify_tests.exe      # 13 tests
.\build-release\Tests\CoreTests\ht_core_tests.exe          # 12 tests
```

**总计 58 项单元测试全部通过**

---

## 📊 Alpha 路线图

| 阶段 | 功能 | 状态 |
|------|------|------|
| Alpha-1 | GUI + 基础本地复制 | ✅ 完成 |
| Alpha-2 | 多线程 + 异步 I/O | ✅ 完成 |
| Alpha-3R | CopyEngine 重构 (Reader→Queue→Writer) | ✅ 完成 |
| Alpha-3 | 断点续传 (批量刷盘) | ✅ 完成 |
| Alpha-4 | BLAKE3 + SHA-256 + CRC32 校验 | ✅ 完成 |
| Alpha-5 | 限速 + 任务管理 + 日志 | ✅ 完成 |
| Alpha-3.1 | Bug Fix Sprint (P0×6 + P1×8) | ✅ 完成 |
| Alpha-6 | 定时增量备份 | ✅ 完成 |
| Alpha-7 | 系统托盘 + 开机自启动 | ✅ 完成 |
| **Beta** | **稳定性测试 + 性能基准** | 📋 计划中 |

---

## 🔧 代码质量规范

| 规则 | 说明 |
|------|------|
| 禁止裸 `new`/`delete` | 统一 `unique_ptr`/`shared_ptr` |
| 禁止裸 `std::thread` | 统一 RAII join 析构 |
| 禁止 SQL 字符串拼接 | 统一 Prepared Statements |
| 禁止 `DWORD` 保存文件大小 | 统一 `uint64_t`/`size_t` |
| 所有 I/O 检查返回值 | 循环 Read/Write 直到完成 |
| 所有 chunk: Read→Hash→Write→Verify | 完整数据完整性流水线 |

---

## 🛠️ 技术栈

| 库 | 版本 | 用途 |
|----|------|------|
| C++20 | MSVC 14.44 | 编程语言 |
| Qt6 | 6.8.2 | GUI 框架 (Core, Widgets) |
| OpenSSL | 3.x | SHA-256 哈希 |
| BLAKE3 | 1.8.x | BLAKE3 哈希 |
| SQLite3 | 3.x | 审计日志 (WAL 模式) |
| zstd | 1.5.x | 压缩 (未来) |
| Google Test | 1.17.x | 单元测试 |

---

## 📝 错误码

| 代码 | 类别 | 说明 |
|------|------|------|
| HT-E001 | Config | 配置错误 |
| HT-E002 | Source | 源文件不可访问 |
| HT-E003 | Target | 目标文件创建失败 |
| HT-E004 | Storage | 磁盘空间不足 |
| HT-E005 | Verify | 完整性校验失败 |
| HT-E006 | Verify | 哈希不匹配 |
| HT-E007 | Transfer | 目录传输中文件失败 |
| HT-E999 | System | 未处理异常 |

---

## 🙏 致谢

- [Qt6](https://www.qt.io/) — 跨平台 UI 框架
- [OpenSSL](https://www.openssl.org/) — 密码学库
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3) — 快速密码学哈希
- [SQLite](https://www.sqlite.org/) — 嵌入式数据库
- [vcpkg](https://vcpkg.io/) — C++ 包管理器
- [Google Test](https://github.com/google/googletest) — 测试框架
