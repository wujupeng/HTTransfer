# HTTransfer

**High-Performance Local File Copy Engine for Windows**

A world-class local file replication engine designed for stability, speed, and reliability — positioned as a modern C++ alternative to FastCopy / TeraCopy / Robocopy (GUI edition).

> **Current Version**: v0.1.0-alpha.3.1  
> **Status**: Alpha (Bug Fix Sprint)  
> **Branch**: `master` — Pure C++20 implementation, no managed runtime dependencies

---

## Features

### Core Capabilities
- **Multi-threaded Parallel Transfer** — ReaderPool (N parallel readers) → ConcurrentQueue → WriterThread (single writer) pipeline architecture
- **Large File Support** — Stable copying of TB-scale files using 16MB chunked I/O with loop-based Read/Write (handles >4GB per call)
- **Data Integrity Verification** — BLAKE3 / SHA-256 / CRC32 hash verification with configurable algorithm selection
- **Crash Recovery** — Resume files (`.htresume`) with batched disk writes enable task continuation after unexpected termination
- **Speed Control** — Token-bucket rate limiting integrated into WriterThread
- **SMB Detection** — Automatic single-thread fallback for SMB/UNC source paths
- **UTF-8 Path Support** — Full Chinese/CJK path handling via `utf8ToPath()` / `pathToUtf8()` conversion layer

### GUI Features
- **Qt6 Native Interface** — Clean, responsive Windows GUI
- **Real-time Progress** — Transfer percentage, speed (MB/s), estimated remaining time
- **Language Switching** — English / 中文 (Chinese) via Help > Language menu
- **About Dialog** — Version info accessible from Help menu
- **File & Directory Selection** — Separate browse buttons for files and folders
- **Application Icon** — Desktop/taskbar/exe icon

### CLI Features
- **Version Flag** — `HunterTransfer.exe --version` or `-v` prints version and exits

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                       GUI (Qt6)                           │
│               MainWindow / AboutDialog                    │
├──────────────────────────────────────────────────────────┤
│                     TaskManager                           │
│      State Machine · Progress · Scheduling · Presets      │
├──────────┬──────────┬───────────┬────────────────────────┤
│FileEngine│VerifyEng │ResumeEng  │    TransferEngine       │
│  Scan    │ BLAKE3   │ .htresume │  ┌──────────────────┐  │
│  Prealloc│ SHA-256  │  Batched  │  │  ReaderPool (N)  │  │
│  Stability│ CRC32   │  Crash    │  │       ↓          │  │
│  Resolve │  Report  │  Recovery │  │ ConcurrentQueue  │  │
│          │          │           │  │       ↓          │  │
│          │          │           │  │ WriterThread (1) │  │
│          │          │           │  └──────────────────┘  │
├──────────┴──────────┴───────────┴────────────────────────┤
│           IDataSource / IDataSink                         │
│         LocalFileSource / LocalFileSink                    │
│    (CreateFileW / ReadFile / WriteFile with loop I/O)     │
├──────────────────────────────────────────────────────────┤
│               Core Infrastructure                         │
│   BufferPool · SpeedController (Token Bucket)             │
│   Logger (SQLite Prepared Statements + Daily Rotation)    │
│   ConfigManager · PresetRepository                        │
└──────────────────────────────────────────────────────────┘
```

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| ReaderPool → Queue → WriterThread pipeline | Eliminates multi-writer SMB instability; single file handle for target |
| `IDataSource` / `IDataSink` abstraction | Decouples transfer logic from data source/sink specifics |
| `utf8ToPath()` / `pathToUtf8()` | Avoids `std::filesystem::path(string)` ANSI codepage traps on Windows |
| Loop-based Read/Write (max 2GB per Win32 call) | Handles DWORD truncation and partial writes for >4GB chunks |
| SQLite Prepared Statements | Prevents SQL injection in audit logging |
| RAII join in ReaderPool/WriterThread destructors | Prevents `std::terminate` on exception during thread lifecycle |
| Token-bucket speed control with batched consumption | Handles chunk sizes exceeding bucket capacity without infinite loop |

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
│   │   ├── Types.h         # offset_t, utf8ToPath(), pathToUtf8()
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
│   ├── SpeedController.h/cpp # Token-bucket rate limiter
│   ├── LocalFileSource.h/cpp  # IDataSource → CreateFileW (loop Read)
│   ├── LocalFileSink.h/cpp    # IDataSink → CreateFileW (loop Write)
│   ├── IDataSource.h       # Read(offset, buffer, size) interface
│   ├── IDataSink.h         # Write(offset, buffer, size) interface
│   └── IOCDispatcher.h     # IOCP dispatcher (future)
├── Transfer/               # Transfer engine
│   ├── TransferEngine.h/cpp# Reader→Queue→Writer orchestration
│   ├── DataChunk.h         # Chunk data structure with move semantics
│   ├── ConcurrentQueue.h/cpp # Thread-safe bounded blocking queue
│   ├── ReaderPool.h/cpp    # N parallel reader threads
│   ├── WriterThread.h/cpp  # Single writer thread with RAII join
│   ├── RetryController.h/cpp
│   ├── WorkerPool.h/cpp    # Generic thread pool
│   └── Adapters/           # SMB, HTTP, FTP (stubs)
├── Verify/                 # Integrity verification
│   ├── VerifyEngine.h/cpp  # Multi-algorithm file hashing
│   ├── Blake3Calculator.h/cpp # BLAKE3 hash
│   ├── CRC32Calculator.h   # CRC32 (header-only)
│   └── SHA256Calculator.h  # SHA-256 (header-only)
├── Resume/                 # Crash recovery
│   ├── ResumeEngine.h/cpp  # .htresume create/load/invalidate (batched)
│   ├── ResumeFileParser.h/cpp
│   └── ResumeFileWriter.h/cpp
├── Logger/                 # Audit logging
│   ├── ILogger.h           # Log interface
│   ├── Logger.h/cpp        # SQLite (prepared stmts) + daily file rotation
│   └── SQLiteDB.h/cpp      # SQLite wrapper with parameterized queries
├── Config/                 # Configuration
│   ├── ConfigManager.h/cpp
│   ├── PresetRepository.h/cpp
│   └── ScheduleRepository.h/cpp
├── GUI/                    # Qt6 user interface
│   ├── MainWindow.h        # Main window (inline implementation)
│   ├── AboutDialog.h       # About dialog with version info
│   └── CMakeLists.txt      # moc generation
├── Resources/              # Application resources
│   ├── app.ico             # Windows exe icon (multi-size)
│   ├── app.png             # Qt window icon
│   ├── app.rc              # Windows resource file
│   └── resources.qrc       # Qt resource file
├── Tests/                  # Unit tests (Google Test)
│   ├── CoreTests/          # 12 tests
│   ├── TransferTests/      # 26 tests
│   ├── VerifyTests/        # 13 tests
│   └── ResumeTests/        # 7 tests
├── cmake/
│   └── version.h.in        # CMake template → generated/version.h
├── CMakeLists.txt          # Root build configuration
└── vcpkg.json              # Dependency manifest (blake3, openssl, sqlite3, etc.)
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
pip install aqtinstall
aqt install-qt windows desktop 6.8.2 msvc2022_64 -O C:\Qt6
```

### 2. Install vcpkg Dependencies

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
bootstrap-vcpkg.bat
vcpkg install openssl:x64-windows sqlite3:x64-windows blake3:x64-windows curl:x64-windows libssh2:x64-windows zstd:x64-windows gtest:x64-windows
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
   - **Verify** — BLAKE3/SHA-256 integrity check after transfer
   - **Speed Limit** — Throttle transfer speed (token-bucket)
   - **Threads** — Number of parallel readers (1-8)
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
- Pre-release identifier: `set(HT_VERSION_PRERELEASE "alpha.3.1")`
- Auto-generated `version.h` via `configure_file()`
- Accessed in code via `VersionInfo::version_string` / `VersionInfo::version_full`

### Alpha Roadmap

| Phase | Focus | Status |
|-------|-------|--------|
| Alpha-1 | GUI + Basic local copy | ✅ Complete |
| Alpha-2 | Multi-thread + Async I/O | ✅ Complete |
| Alpha-3R | CopyEngine refactor (Reader→Queue→Writer) | ✅ Complete |
| Alpha-3 | Resume (batched disk writes) | ✅ Complete |
| Alpha-4 | BLAKE3 + SHA-256 + CRC32 verification | ✅ Complete |
| Alpha-5 | Speed control + Task management + Logging | ✅ Complete |
| Alpha-3.1 | Bug Fix Sprint (P0×6 + P1×8) | ✅ Complete |
| **Beta** | **Stability testing + Performance benchmarks** | 📋 Planned |

---

## Technical Details

### Transfer Pipeline

```
startTransferReaderWriter()
├── isSMB(source_path) → reader_count = 1 (single-thread for SMB)
├── ConcurrentQueue (bounded, capacity = reader_count × 4)
├── ReaderPool (N threads)
│   ├── Each opens independent LocalFileSource handle
│   ├── Atomic next_chunk_index distributes work
│   ├── Loop Read (max 2GB per ReadFile call)
│   ├── On failure: signalWriterError() → stops Writer
│   └── RAII join on destruction
├── WriterThread (1 thread)
│   ├── Single LocalFileSink handle (single target file handle)
│   ├── Loop Write (max 2GB per WriteFile call)
│   ├── SpeedController waitForTokens (batched consumption)
│   ├── markChunkCompleted → ResumeEngine (batched every 4 chunks)
│   └── RAII join on destruction
└── Progress callback → TaskManager (mutex-protected)
```

### UTF-8 Path Handling (Windows)

Windows `std::filesystem::path(string)` uses the system ANSI code page (GBK on Chinese Windows), which corrupts CJK characters. HTTransfer uses a dedicated conversion layer:

```cpp
std::filesystem::path utf8ToPath(const std::string& utf8_str);
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
| HT-E006 | Verify | SHA-256 hash mismatch |
| HT-E007 | Transfer | File failed in directory transfer |
| HT-E999 | System | Unhandled exception |

### Retry Strategy

- Each chunk gets up to **3 retry attempts** (configurable via `kMaxChunkRetries`)
- Exponential backoff: 100ms × (retry + 1)
- Failed chunks propagate error via `signalWriterError()` → Writer stops

### Code Quality Rules (PM-enforced)

| Rule | Description |
|------|-------------|
| No bare `new`/`delete` | Use `unique_ptr`/`shared_ptr` |
| No bare `std::thread` | Use RAII join wrappers |
| No SQL string concatenation | Use Prepared Statements |
| No `DWORD` for file sizes | Use `uint64_t`/`size_t` |
| All I/O must check return values | Loop Read/Write until complete |
| All chunks: Read→Hash→Write→Verify | Full data integrity pipeline |

---

## Testing

```powershell
# Run all tests
.\build-release\Tests\TransferTests\ht_transfer_tests.exe
.\build-release\Tests\ResumeTests\ht_resume_tests.exe
.\build-release\Tests\VerifyTests\ht_verify_tests.exe
.\build-release\Tests\CoreTests\ht_core_tests.exe
```

**58 tests total** — Transfer 26 + Resume 7 + Verify 13 + Core 12

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| **Qt6** | 6.8.2 | GUI framework (Core, Widgets) |
| **OpenSSL** | 3.x | SHA-256 hash computation |
| **BLAKE3** | 1.8.x | BLAKE3 hash computation |
| **SQLite3** | 3.x | Audit log storage (WAL mode) |
| **zstd** | 1.5.x | Compression (future) |
| **Google Test** | 1.17.x | Unit testing |
| **libcurl** | 8.x | HTTP transfers (future) |
| **libssh2** | 1.11.x | SSH/SFTP transfers (future) |

---

## License

Private project. All rights reserved.

---

## Acknowledgments

Built with:
- [Qt6](https://www.qt.io/) — Cross-platform UI framework
- [OpenSSL](https://www.openssl.org/) — Cryptographic library
- [BLAKE3](https://github.com/BLAKE3-team/BLAKE3) — Fast cryptographic hash
- [SQLite](https://www.sqlite.org/) — Embedded database engine
- [vcpkg](https://vcpkg.io/) — C++ package manager
- [Google Test](https://github.com/google/googletest) — Testing framework
