# HunterTransfer

**High-Performance Local File Copy Engine for Windows**

A world-class local file replication engine designed for stability, speed, and reliability — positioned as a modern C++ alternative to FastCopy / TeraCopy / Robocopy (GUI edition).

> **Current Version**: v0.1.0-alpha.2  
> **Status**: Alpha (Active Development)  
> **Branch**: `pure-cpp` — Pure C++ implementation, no managed runtime dependencies

---

## Features

### Core Capabilities
- **Multi-threaded Parallel Transfer** — Atomic chunk distribution across N worker threads with independent file handles
- **Large File Support** — Stable copying of TB-scale files using 16MB chunked I/O
- **Data Integrity Verification** — SHA-256 hash verification (source vs target) with configurable presets
- **Crash Recovery** — Resume files (`.htresume`) enable task continuation after unexpected termination
- **UTF-8 Path Support** — Full Chinese/CJK path handling via `utf8ToPath()` / `pathToUtf8()` conversion layer

### GUI Features
- **Qt6 Native Interface** — Clean, responsive Windows GUI
- **Real-time Progress** — Transfer percentage, speed (MB/s), estimated remaining time
- **Language Switching** — English / 中文 (Chinese) on-the-fly via combo box
- **About Dialog** — Version info accessible from Help menu
- **File & Directory Selection** — Separate browse buttons for files and folders

### CLI Features
- **Version Flag** — `HunterTransfer.exe --version` or `-v` prints version and exits

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│                    GUI (Qt6)                     │
│              MainWindow / AboutDialog            │
├─────────────────────────────────────────────────┤
│                  TaskManager                     │
│    State Machine · Progress · Scheduling         │
├──────────┬──────────┬───────────┬───────────────┤
│FileEngine│VerifyEng │ResumeEng  │TransferEngine │
│  Scan    │ SHA-256  │ .htresume │  Multi-thread │
│  Prealloc│  Report  │  Crash    │  Chunk I/O    │
│  Stability│         │  Recovery │  Retry Ctrl   │
├──────────┴──────────┴───────────┴───────────────┤
│           IDataSource / IDataSink               │
│         LocalFileSource / LocalFileSink          │
│         (CreateFileW / ReadFile / WriteFile)     │
├─────────────────────────────────────────────────┤
│              Core Infrastructure                 │
│   BufferPool · SpeedController · IOCDispatcher   │
│   Logger (SQLite + Daily File Rotation)          │
│   ConfigManager · PresetRepository               │
└─────────────────────────────────────────────────┘
```

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| `IDataSource` / `IDataSink` abstraction | Decouples transfer logic from data source/sink specifics |
| `utf8ToPath()` / `pathToUtf8()` | Avoids `std::filesystem::path(string)` ANSI codepage traps on Windows |
| `CreateFileW` with `FILE_SHARE_READ \| FILE_SHARE_WRITE` | Enables multi-worker concurrent access to same file |
| CMake `configure_file()` for version | Single source of truth — version defined once in `CMakeLists.txt` |
| Per-worker independent file handles | Eliminates mutex contention on `SetFilePointerEx` + `ReadFile`/`WriteFile` |

---

## Project Structure

```
HunterTransfer/
├── App/                    # Application entry point
│   ├── main.cpp            # WinMain / main with --version support
│   ├── Application.h       # App initialization, logger, version log
│   └── CMakeLists.txt
├── Core/                   # Core business logic
│   ├── Common/
│   │   ├── Types.h         # offset_t, utf8ToPath()
│   │   ├── Constants.h     # Chunk size, parallelism, timeouts
│   │   ├── Result.h        # Result<T> error handling
│   │   ├── ErrorCodes.h    # HT-E001 ~ HT-E999
│   │   └── VersionInfo.h   # Compile-time version from version.h
│   ├── Domain/
│   │   ├── TransferTask.h  # Task state machine, TaskStatus enum
│   │   ├── ChunkManifest.h # Chunk splitting logic
│   │   ├── TransferPreset.h# Fast / Balanced / Secure presets
│   │   ├── ResumeFile.h    # .htresume format definition
│   │   └── IntegrityReport.h
│   ├── TaskManager.h/cpp   # Central orchestrator
│   ├── FileEngine.h/cpp    # File operations, scanning, preallocation
│   ├── BufferPool.h/cpp    # Pre-allocated memory pool
│   ├── SpeedController.h/cpp
│   ├── LocalFileSource.h/cpp  # IDataSource → CreateFileW
│   ├── LocalFileSink.h/cpp    # IDataSink → CreateFileW
│   ├── IDataSource.h       # Read(offset, buffer, size) interface
│   ├── IDataSink.h         # Write(offset, buffer, size) interface
│   └── IOCDispatcher.h     # IOCP dispatcher (future)
├── Transfer/               # Transfer engine
│   ├── TransferEngine.h/cpp# Multi-thread transfer orchestration
│   ├── RetryController.h/cpp
│   ├── WorkerPool.h/cpp    # Generic thread pool
│   └── Adapters/           # SMB, HTTP, FTP (stubs)
├── Verify/                 # Integrity verification
│   ├── VerifyEngine.h/cpp  # SHA-256 file hashing
│   ├── CRC32Calculator.h/cpp
│   └── SHA256Calculator.h/cpp
├── Resume/                 # Crash recovery
│   ├── ResumeEngine.h/cpp  # .htresume create/load/invalidate
│   ├── ResumeFileParser.h/cpp
│   └── ResumeFileWriter.h/cpp
├── Logger/                 # Audit logging
│   ├── ILogger.h           # Log interface
│   ├── Logger.h/cpp        # SQLite + daily file rotation
│   └── SQLiteDB.h/cpp      # SQLite wrapper
├── Config/                 # Configuration
│   ├── ConfigManager.h/cpp
│   ├── PresetRepository.h/cpp
│   └── ScheduleRepository.h/cpp
├── GUI/                    # Qt6 user interface
│   ├── MainWindow.h        # Main window (inline implementation)
│   ├── AboutDialog.h       # About dialog with version info
│   └── CMakeLists.txt      # moc generation
├── Tests/                  # Unit tests (Google Test)
│   ├── CoreTests/
│   ├── TransferTests/
│   ├── VerifyTests/
│   └── ResumeTests/
├── cmake/
│   └── version.h.in        # CMake template → generated/version.h
├── CMakeLists.txt          # Root build configuration
└── vcpkg.json              # Dependency manifest
```

---

## Build Instructions

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| **Visual Studio 2022** | BuildTools | With C++20 support |
| **CMake** | ≥ 3.25 | Via vcpkg or standalone |
| **Ninja** | ≥ 1.11 | Build system |
| **Qt6** | 6.8.2 | MSVC 2022 x64 |
| **vcpkg** | Latest | Package manager |

### 1. Install Qt6

```powershell
# Using aqtinstall (recommended)
pip install aqtinstall
aqt install-qt windows desktop 6.8.2 msvc2022_64 -O C:\Qt6
```

### 2. Install vcpkg Dependencies

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
vcpkg install openssl:x64-windows sqlite3:x64-windows curl:x64-windows libssh2:x64-windows zstd:x64-windows gtest:x64-windows
```

### 3. Configure & Build

```powershell
# Set up MSVC environment (PowerShell)
$env:INCLUDE = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared"
$env:LIB = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
$env:PATH = "C:\vcpkg\downloads\tools\cmake-4.3.3-windows\cmake-4.3.3-windows-x86_64\bin;C:\vcpkg\downloads\tools\ninja-1.13.2-windows;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;" + $env:PATH

# Configure
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
ninja -C build-release
```

### 4. Run

```powershell
# GUI mode
.\build-release\App\HunterTransfer.exe

# Print version
.\build-release\App\HunterTransfer.exe --version
# Output: HunterTransfer v0.1.0-alpha.2
```

---

## Usage

### GUI Operation

1. **Select Source** — Click "File" to pick a single file, or "Directory" to pick a folder
2. **Select Target** — Click "..." to browse for the destination directory
3. **Configure Options**:
   - **Multi-thread** — Enable parallel transfer (default: ON, 4 threads)
   - **Overwrite** — Replace existing target files
   - **Resume** — Enable crash recovery via .htresume files
   - **Verify** — SHA-256 integrity check after transfer
   - **Speed Limit** — Throttle transfer speed
   - **Threads** — Number of parallel workers (1-8)
4. **Click Start** — Monitor progress in real-time
5. **Pause/Resume/Stop** — Control transfer at any time

### Transfer Presets

| Preset | Multi-thread | Verify | Speed Limit |
|--------|-------------|--------|-------------|
| **Fast** | Off | Off | Off |
| **Balanced** | On | On | Off |
| **Secure** | On | On | On |

---

## Version Management

Version numbers follow [Semantic Versioning 2.0](https://semver.org/):

```
MAJOR.MINOR.PATCH[-PRERELEASE]
```

- Defined in `CMakeLists.txt`: `project(HunterTransfer VERSION 0.1.0)`
- Pre-release identifier: `set(HT_VERSION_PRERELEASE "alpha.2")`
- Auto-generated `version.h` via `configure_file()`
- Accessed in code via `VersionInfo::version_string` / `VersionInfo::version_full`

### Version Visibility

| Location | Format | Example |
|----------|--------|---------|
| Window Title | `Hunter Transfer {version}` | `Hunter Transfer 0.1.0-alpha.2` |
| First Log Line | `HunterTransfer started - HunterTransfer v{version}` | `HunterTransfer started - HunterTransfer v0.1.0-alpha.2` |
| About Dialog | Full version + copyright | `Version: 0.1.0-alpha.2` |
| CLI `--version` | Full identifier | `HunterTransfer v0.1.0-alpha.2` |

### Alpha Roadmap

| Phase | Focus | Status |
|-------|-------|--------|
| Alpha-1 | GUI + Basic local copy | ✅ Complete |
| Alpha-2 | Multi-thread + Async I/O | 🔄 In Progress |
| Alpha-3 | Resume (SQLite persistence) | 📋 Planned |
| Alpha-4 | BLAKE3 + SHA-256 verification | 📋 Planned |
| Alpha-5 | Task management & logging | 📋 Planned |

---

## Technical Details

### Multi-threaded Transfer Flow

```
startTransfer()
├── parallelism_ > 1 → startTransferMultiThread()
│   ├── preallocateFile() creates target with CREATE_ALWAYS
│   ├── N worker threads, each opens independent file handles
│   ├── Atomic counter next_chunk_index distributes work
│   ├── Per-worker 4MB buffer → sub-chunk I/O loop
│   └── progress_callback_ updates TaskManager (mutex-protected)
└── parallelism_ == 1 → startTransferSingleThread()
    └── Sequential chunk Read → Write
```

### UTF-8 Path Handling (Windows)

Windows `std::filesystem::path(string)` uses the system ANSI code page (GBK on Chinese Windows), which corrupts CJK characters. HunterTransfer uses a dedicated conversion layer:

```cpp
// UTF-8 std::string → std::filesystem::path (via wchar_t)
std::filesystem::path utf8ToPath(const std::string& utf8_str);

// std::filesystem::path → UTF-8 std::string (via wchar_t)
std::string pathToUtf8(const std::filesystem::path& p);
```

All file operations use `CreateFileW` / `GetFileAttributesExW` (wide-char Win32 APIs) internally.

### Error Handling

All operations return `Result<T>` with structured error codes:

| Code | Category | Description |
|------|----------|-------------|
| HT-E001 | Config | Invalid configuration |
| HT-E002 | Source | Source file inaccessible |
| HT-E003 | Target | Target file creation failed |
| HT-E004 | Storage | Insufficient disk space |
| HT-E005 | Verify | Integrity check failed |
| HT-E999 | System | Unhandled exception |

### Retry Strategy

- Each chunk gets up to **3 retry attempts**
- Exponential backoff: 50ms × (retry + 1)
- Failed chunks are counted but don't abort the entire transfer

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| **Qt6** | 6.8.2 | GUI framework (Core, Widgets) |
| **OpenSSL** | 3.x | SHA-256 hash computation |
| **SQLite3** | 3.x | Audit log storage |
| **zstd** | 1.5.x | Compression (future) |
| **Google Test** | 1.17.x | Unit testing |
| **libcurl** | 8.x | HTTP transfers (future) |
| **libssh2** | 1.11.x | SSH/SFTP transfers (future) |

---

## Configuration

### CMake Compile Definitions

| Definition | Default | Description |
|------------|---------|-------------|
| `HT_CHUNK_SIZE` | 16777216 (16MB) | Size of each transfer chunk |
| `HT_BUFFER_POOL_SIZE` | 67108864 (64MB) | Pre-allocated buffer pool |
| `HT_MAX_PARALLELISM` | 8 | Maximum worker threads |
| `HT_DEFAULT_PARALLELISM` | 4 | Default worker threads |

### Runtime Configuration

- **Language**: Stored in `QSettings("HunterTransfer", "HunterTransfer")` under key `language`
- **Log Files**: Daily rotation to `{YYYY-MM-DD}.log` in working directory
- **Audit Database**: `ht_audit.db` (SQLite WAL mode)

---

## Testing

```powershell
# Build tests
cmake --build build-release --target ht_tests

# Run tests
cd build-release
ctest --output-on-failure
```

Test modules:
- `CoreTests` — BufferPool, Domain types, Result, Types
- `TransferTests` — TransferEngine, RetryController
- `VerifyTests` — VerifyEngine, CRC32, SHA-256
- `ResumeTests` — ResumeEngine, file format

---

## License

Private project. All rights reserved.

---

## Acknowledgments

Built with:
- [Qt6](https://www.qt.io/) — Cross-platform UI framework
- [OpenSSL](https://www.openssl.org/) — Cryptographic library
- [SQLite](https://www.sqlite.org/) — Embedded database engine
- [vcpkg](https://vcpkg.io/) — C++ package manager
- [Google Test](https://github.com/google/googletest) — Testing framework