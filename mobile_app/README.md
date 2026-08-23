# AI 智能翻译手机版 (Flutter 跨平台 App)

本工程是专为手机端打造的 AI 智能翻译 App，一套代码同时支持 **Android (安卓 APK)** 和 **iOS (苹果)**。

---

## 📱 手机端功能亮点
1. **地道口语化 Prompt 引擎**：内置与桌面版一致的翻译提示词（支持 Bro/Dude/俚语转换，1:1 严格保持清单与换行）。
2. **移动端卡片式双栏布局**：
   - 顶部语言快速对调（自动检测 $\leftrightarrow$ 英语）。
   - 上方输入卡片（字符统计、一键粘贴、一键清空）。
   - 下方结果卡片（字符统计、一键复制、自动复制到剪贴板）。
3. **多模型与 API 配置中心**：
   - 支持自定义 Endpoint 与 API Key。
   - 一键测试 API 连通性与网络延迟。
   - 动态拉取支持的模型列表（gpt-4o-mini, claude-3-5-sonnet, deepseek-chat, qwen-plus 等）。

---

## 🛠️ 如何在电脑上打包生成 APK

### 步骤 1：安装 Flutter 环境
1. 下载 Flutter SDK：[https://flutter.dev](https://flutter.dev) 并解压（例如到 `C:\flutter`）。
2. 将 `C:\flutter\bin` 添加到系统的环境变量 `Path` 中。
3. 打开 PowerShell 运行：
   ```powershell
   flutter doctor
   ```

### 步骤 2：生成项目骨架并运行
进入本工程目录：
```powershell
cd e:\代码\翻译软件\mobile_app
flutter create .
flutter pub get
```

### 步骤 3：打包生成安卓安装包 (APK)
```powershell
flutter build apk --release
```
打包完成后，生成的 APK 安装包位于：
`build/app/outputs/flutter-apk/app-release.apk`
直接发送到手机上即可安装使用！
