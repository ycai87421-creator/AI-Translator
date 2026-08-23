# TranslatorWin32 (智能翻译工作台)

基于纯原生 C++ Win32 API 与 WinHTTP 开发的现代化 AI 大模型翻译桌面应用，无任何 Qt/MFC 臃肿依赖，极速启动、内存占用极低。

## ✨ 核心特性

- **现代卡片式视觉风格**：启用 Common Controls v6 视觉样式，搭配 Microsoft YaHei UI（微软雅黑）排版与卡片圆角容器，消除传统 Win32 灰色块瑕疵。
- **自适应响应式布局**：输入与翻译结果双栏弹性伸缩，支持窗口任意拉伸、全屏与高 DPI 适配。
- **语言一键互换 (⇄)**：快速调换源语言与目标语言，自动同步输入输出文本。
- **高效快捷键支持**：在输入框内按下 `Ctrl + Enter` 即可快速触发翻译。
- **实时字符统计**：源文本与翻译结果实时显示字符计数。
- **独立设置与连通性测试**：支持设置 API 基础地址、API Key（支持明文切换）、默认模型，并可一键测试 API 网络连通性与响应延迟。
- **内置高精度英文翻译 Prompt**：内部集成经过深度调优的系统级翻译提示词，严格保障专业度、格式保留度与无额外冗余废话，对用户完全透明隐藏。
- **本地配置持久化**：自动将 API 地址、密钥、模型与语言选择保存至 `translator_config.ini`，下次打开即用。
- **耗时与状态实时反馈**：每次翻译均精确统计并展示 API 请求耗时（如 `耗时 0.42 秒`）。

## 🔨 编译指南

使用 Visual Studio 2022/2026 打开 `TranslatorWin32.sln`，选择 `Release | x64` 直接生成解决方案。

或者在命令行（Developer PowerShell）中执行：

```powershell
msbuild .\TranslatorWin32.sln /p:Configuration=Release /p:Platform=x64
```

编译输出目录为：`x64\Release\TranslatorWin32.exe`。

## 🚀 使用说明

1. 启动 `TranslatorWin32.exe`。
2. 点击右上角 **`⚙️ API 与模型`**，输入你的 API 基础地址与 API Key（支持点击“⚡ 测试连接”测试连通性与网络延迟），点击保存。
3. 点击工具栏 **`🔄 获取模型`** 自动从 API 拉取可用模型列表，亦可手动输入或选择预设模型（如 `gpt-4o-mini`, `claude-3-5-sonnet`, `deepseek-chat` 等）。
4. 在左侧输入或粘贴源文本，按下 **`Ctrl + Enter`** 或点击 **`🚀 翻译`** 按钮开始翻译。
5. 翻译完成后可点击 **`📋 复制结果`** 一键复制到剪贴板。
