import os
import subprocess
import sys

# 设置环境变量
env = os.environ.copy()
env['QT_DEBUG_PLUGINS'] = '1'
env['QT_QPA_PLATFORM_PLUGIN_PATH'] = r'C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\dist\HunterTransfer-1.0.0-windows-x86_64\plugins\platforms'

# 运行程序
exe_path = r'C:\Users\DELL\Documents\dev\autofile\autofile\HunterTransfer\dist\HunterTransfer-1.0.0-windows-x86_64\bin\HunterTransfer.exe'

print(f"运行: {exe_path}")
print(f"插件路径: {env['QT_QPA_PLATFORM_PLUGIN_PATH']}")

try:
    result = subprocess.run([exe_path], env=env, capture_output=True, text=True, timeout=5)
    print("标准输出:", result.stdout)
    print("标准错误:", result.stderr)
    print("返回码:", result.returncode)
except subprocess.TimeoutExpired:
    print("程序启动成功（超时退出）")
except Exception as e:
    print(f"错误: {e}")