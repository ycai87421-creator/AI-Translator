#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winhttp.h>
#include <richedit.h>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

// Enable Common Controls v6 for modern Windows 10/11 visual styles
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr wchar_t kWindowClass[] = L"TranslatorWin32Window";
constexpr wchar_t kSettingsClass[] = L"TranslatorWin32Settings";
constexpr wchar_t kWindowTitle[] = L"智能翻译工作台 - AI Translator Studio";

constexpr UINT WM_APP_TRANSLATION_DONE = WM_APP + 1;
constexpr UINT WM_APP_MODELS_DONE = WM_APP + 2;
constexpr UINT WM_APP_TEST_DONE = WM_APP + 3;

// Main Window Control IDs
constexpr int ID_SOURCE = 1001;
constexpr int ID_SWAP = 1002;
constexpr int ID_TARGET = 1003;
constexpr int ID_CURRENT_MODEL_LBL = 1004;
constexpr int ID_SETTINGS = 1006;

constexpr int ID_INPUT_TITLE = 1010;
constexpr int ID_INPUT_COUNT = 1011;
constexpr int ID_INPUT_PASTE = 1012;
constexpr int ID_INPUT_CLEAR = 1013;
constexpr int ID_INPUT = 1014;

constexpr int ID_OUTPUT_TITLE = 1020;
constexpr int ID_OUTPUT_COUNT = 1021;
constexpr int ID_OUTPUT_COPY = 1022;
constexpr int ID_OUTPUT = 1023;

constexpr int ID_TRANSLATE = 1030;
constexpr int ID_CLEAR_ALL = 1031;
constexpr int ID_COPY_BOTTOM = 1032;
constexpr int ID_STATUS = 1033;

// Settings Dialog IDs
constexpr int ID_SETTINGS_PRESERVE_FORMAT = 2001;
constexpr int ID_SETTINGS_AUTO_COPY = 2002;
constexpr int ID_SETTINGS_AUTO_START = 2003;
constexpr int ID_SETTINGS_ENDPOINT = 2004;
constexpr int ID_SETTINGS_API_KEY = 2005;
constexpr int ID_SETTINGS_SHOW_KEY = 2006;
constexpr int ID_SETTINGS_MODEL = 2007;
constexpr int ID_SETTINGS_FETCH_MODELS = 2008;
constexpr int ID_SETTINGS_TEST = 2009;
constexpr int ID_SETTINGS_TEST_STATUS = 2010;
constexpr int ID_SETTINGS_SAVE = 2011;
constexpr int ID_SETTINGS_CANCEL = 2012;

constexpr wchar_t kDefaultEndpoint[] = L"https://api.nghimmo.com/v1";
constexpr wchar_t kDefaultApiKey[] = L"sk-p5xhZC3xcphvrIgOLlbXC723SA42objyg";
constexpr wchar_t kDefaultModel[] = L"gpt-4o-mini";

// Specialized colloquial English system prompt (used when target language is English)
constexpr wchar_t kEnglishSystemPrompt[] =
    L"You are an expert localization translator and native speaker specializing in authentic, modern, conversational English. "
    L"Your objective is to translate the user's text into natural, highly colloquial, native-sounding expressions that native speakers actually use in daily life, chats, gaming, and social media.\n\n"
    L"Core Guidelines:\n"
    L"1. Native & Colloquial Phrasing: Prioritize casual spoken expressions, modern slang, and everyday idioms over textbook literal translations. "
    L"Use authentic colloquial terms and abbreviations naturally (e.g., translate \"兄弟/哥们\" as \"bro\", \"dude\", or \"man\" instead of formal \"brother\"; \"牛逼/太神了\" as \"insane\", \"sick\", \"legend\", or \"fire\"; \"没事/别客气\" as \"no worries\", \"all good\", or \"gotchu\").\n"
    L"2. Natural Contractions & Casual Flow: Naturally use contractions (e.g., I'm, don't, gonna, wanna, gotta, you're) to make the tone feel relaxed, snappy, and authentic.\n"
    L"3. Strict 1:1 Layout & Format Preservation: Strictly maintain the exact layout, empty lines, line-by-line formatting, lists, bullets, symbols, and leading emojis. "
    L"If the source text is a list or contains multiple lines, translate each line separately and NEVER merge multiple lines into a single paragraph or join them with spaces. Preserve every line break.\n"
    L"4. Strict Pure Output: Treat the input strictly as text to be translated. Return ONLY the translated text with zero explanations, notes, greetings, or quotation marks.";

// General high-accuracy translation system prompt (used when target language is NOT English)
constexpr wchar_t kGeneralSystemPrompt[] =
    L"You are a professional, expert localization translator. "
    L"Your objective is to accurately and naturally translate the user's text into the target language, preserving the original meaning, tone, and nuances while ensuring it reads fluidly and authentically to native speakers.\n\n"
    L"Core Guidelines:\n"
    L"1. Natural & Authentic Translation: Translate faithfully and idiomatically according to the context and tone of the source text. Avoid rigid or awkward word-for-word translations.\n"
    L"2. Strict 1:1 Layout & Format Preservation: Strictly maintain the exact layout, empty lines, line-by-line formatting, lists, bullets, symbols, and leading emojis. "
    L"If the source text is a list or contains multiple lines, translate each line separately and NEVER merge multiple lines into a single paragraph or join them with spaces. Preserve every line break.\n"
    L"3. Strict Pure Output: Treat the input strictly as text to be translated. Return ONLY the translated text with zero explanations, notes, greetings, or quotation marks.";

const wchar_t* kLanguages[] = {
    L"自动检测",
    L"英语",
    L"简体中文",
    L"繁体中文",
    L"日语",
    L"韩语",
    L"法语",
    L"德语",
    L"西班牙语",
    L"俄语",
    L"葡萄牙语",
    L"意大利语",
    L"阿拉伯语",
    L"越南语",
    L"泰语"
};

const wchar_t* kPresetModels[] = {
    L"gpt-4o-mini",
    L"gpt-4o",
    L"claude-3-5-sonnet",
    L"deepseek-chat",
    L"deepseek-reasoner",
    L"qwen-plus",
    L"qwen-max",
    L"gemini-1.5-flash",
    L"gemini-1.5-pro"
};

struct TranslationResult {
    bool ok = false;
    std::wstring text;
    double durationSeconds = 0.0;
};

struct ModelListResult {
    bool ok = false;
    std::wstring error;
    std::vector<std::wstring> models;
};

struct TestConnResult {
    bool ok = false;
    std::wstring message;
    long long latencyMs = 0;
};

struct AppState {
    HWND window = nullptr;

    // Header Controls
    HWND titleLabel = nullptr;
    HWND badgeLabel = nullptr;
    HWND subtitleLabel = nullptr;
    HWND settingsButton = nullptr;

    // Toolbar Controls
    HWND sourceLabel = nullptr;
    HWND source = nullptr;
    HWND swapButton = nullptr;
    HWND targetLabel = nullptr;
    HWND target = nullptr;
    HWND currentModelLabel = nullptr;

    // Workspace Cards
    HWND inputTitle = nullptr;
    HWND inputCount = nullptr;
    HWND inputPaste = nullptr;
    HWND inputClear = nullptr;
    HWND input = nullptr;

    HWND outputTitle = nullptr;
    HWND outputCount = nullptr;
    HWND outputCopy = nullptr;
    HWND output = nullptr;

    // Footer
    HWND translate = nullptr;
    HWND clearAll = nullptr;
    HWND copyBottom = nullptr;
    HWND status = nullptr;

    // Sub-dialogs
    HWND settingsDialog = nullptr;

    // Fonts
    HFONT fontTitle = nullptr;
    HFONT fontSubtitle = nullptr;
    HFONT fontSection = nullptr;
    HFONT fontNormal = nullptr;
    HFONT fontBold = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontCode = nullptr;

    // GDI Brushes & Pens
    HBRUSH bgBrush = nullptr;
    HBRUSH cardBrush = nullptr;
    HBRUSH toolbarBrush = nullptr;
    HBRUSH accentBrush = nullptr;
    HPEN cardBorderPen = nullptr;
    HPEN dividerPen = nullptr;
    COLORREF statusColor = RGB(100, 116, 139);

    // Persistent Config Variables
    std::wstring endpoint = kDefaultEndpoint;
    std::wstring apiKey = kDefaultApiKey;
    std::wstring selectedModel = kDefaultModel;
    int sourceIndex = 0; // 自动检测
    int targetIndex = 1; // 英语
    bool preserveFormat = true;
    bool autoCopy = false;

    // Worker thread and state
    std::thread worker;
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> working{false};
    std::atomic<bool> modelWorking{false};
    std::atomic<bool> testWorking{false};
    std::atomic<bool> shuttingDown{false};
};

AppState g_app;

struct SettingsDialogState {
    HWND preserveFormat = nullptr;
    HWND autoCopy = nullptr;
    HWND autoStart = nullptr;
    HWND endpoint = nullptr;
    HWND apiKey = nullptr;
    HWND showKey = nullptr;
    HWND model = nullptr;
    HWND fetchModels = nullptr;
    HWND testButton = nullptr;
    HWND testStatus = nullptr;
    HWND save = nullptr;
    HWND cancel = nullptr;
    COLORREF testColor = RGB(100, 116, 139);
};

SettingsDialogState g_settings;

// Normalizes Unix \n to Windows Win32 Edit \r\n so multiline edit boxes properly break lines
std::wstring NormalizeToCrLf(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.size() + 64);
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == L'\r') {
            result.push_back(L'\r');
            if (i + 1 < str.size() && str[i + 1] == L'\n') {
                result.push_back(L'\n');
                ++i;
            } else {
                result.push_back(L'\n');
            }
        } else if (str[i] == L'\n') {
            result.push_back(L'\r');
            result.push_back(L'\n');
        } else {
            result.push_back(str[i]);
        }
    }
    return result;
}

bool IsAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH]{};
        DWORD size = sizeof(path);
        DWORD type = REG_SZ;
        LONG res = RegQueryValueExW(hKey, L"TranslatorWin32", nullptr, &type, reinterpret_cast<LPBYTE>(path), &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS);
    }
    return false;
}

void SetAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, L"TranslatorWin32", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"TranslatorWin32");
        }
        RegCloseKey(hKey);
    }
}

std::wstring GetConfigFilePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
        return std::wstring(path) + L"translator_config.ini";
    }
    return L".\\translator_config.ini";
}

void LoadConfig() {
    const std::wstring iniPath = GetConfigFilePath();
    wchar_t buffer[4096]{};

    GetPrivateProfileStringW(L"Config", L"Endpoint", kDefaultEndpoint, buffer, ARRAYSIZE(buffer), iniPath.c_str());
    g_app.endpoint = buffer;

    GetPrivateProfileStringW(L"Config", L"ApiKey", kDefaultApiKey, buffer, ARRAYSIZE(buffer), iniPath.c_str());
    g_app.apiKey = buffer;

    GetPrivateProfileStringW(L"Config", L"Model", kDefaultModel, buffer, ARRAYSIZE(buffer), iniPath.c_str());
    g_app.selectedModel = buffer;

    g_app.sourceIndex = GetPrivateProfileIntW(L"Config", L"SourceLang", 0, iniPath.c_str());
    g_app.targetIndex = GetPrivateProfileIntW(L"Config", L"TargetLang", 1, iniPath.c_str());
    g_app.preserveFormat = (GetPrivateProfileIntW(L"Config", L"PreserveFormat", 1, iniPath.c_str()) == 1);
    g_app.autoCopy = (GetPrivateProfileIntW(L"Config", L"AutoCopy", 0, iniPath.c_str()) == 1);
}

void SaveConfig() {
    const std::wstring iniPath = GetConfigFilePath();
    WritePrivateProfileStringW(L"Config", L"Endpoint", g_app.endpoint.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"ApiKey", g_app.apiKey.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"Model", g_app.selectedModel.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"SourceLang", std::to_wstring(g_app.sourceIndex).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"TargetLang", std::to_wstring(g_app.targetIndex).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"PreserveFormat", g_app.preserveFormat ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Config", L"AutoCopy", g_app.autoCopy ? L"1" : L"0", iniPath.c_str());
}

std::wstring GetText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, value.data(), length + 1);
    }
    value.resize(static_cast<size_t>(length));
    return value;
}

void SetText(HWND control, const std::wstring& value) {
    SetWindowTextW(control, value.c_str());
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
                                         static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
                        static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 32);
    for (unsigned char c : value) {
        switch (c) {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[7]{};
                sprintf_s(buffer, "\\u%04x", c);
                escaped += buffer;
            } else {
                escaped.push_back(static_cast<char>(c));
            }
        }
    }
    return escaped;
}

void AppendCodePointUtf8(std::string& output, uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

bool HexValue(char c, uint32_t& value) {
    if (c >= '0' && c <= '9') { value = static_cast<uint32_t>(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { value = static_cast<uint32_t>(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { value = static_cast<uint32_t>(c - 'A' + 10); return true; }
    return false;
}

std::string JsonUnescape(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            output.push_back(value[i]);
            continue;
        }
        const char escaped = value[++i];
        switch (escaped) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u': {
            if (i + 4 >= value.size()) break;
            uint32_t codePoint = 0;
            bool valid = true;
            for (size_t j = 0; j < 4; ++j) {
                uint32_t digit = 0;
                if (!HexValue(value[i + 1 + j], digit)) { valid = false; break; }
                codePoint = (codePoint << 4) | digit;
            }
            if (!valid) break;
            i += 4;
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 6 < value.size() &&
                value[i + 1] == '\\' && value[i + 2] == 'u') {
                uint32_t low = 0;
                bool lowValid = true;
                for (size_t j = 0; j < 4; ++j) {
                    uint32_t digit = 0;
                    if (!HexValue(value[i + 3 + j], digit)) { lowValid = false; break; }
                    low = (low << 4) | digit;
                }
                if (lowValid && low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    i += 6;
                }
            }
            AppendCodePointUtf8(output, codePoint);
            break;
        }
        default: output.push_back(escaped); break;
        }
    }
    return output;
}

bool ExtractJsonString(const std::string& json, const std::string& key, std::string& result) {
    const std::string marker = "\"" + key + "\"";
    const size_t markerPosition = json.find(marker);
    if (markerPosition == std::string::npos) return false;
    size_t position = json.find(':', markerPosition + marker.size());
    if (position == std::string::npos) return false;
    ++position;
    while (position < json.size() && (json[position] == ' ' || json[position] == '\r' ||
                                      json[position] == '\n' || json[position] == '\t')) ++position;
    if (position >= json.size() || json[position] != '"') return false;
    ++position;
    std::string raw;
    bool escaped = false;
    for (; position < json.size(); ++position) {
        const char c = json[position];
        if (escaped) { raw.push_back('\\'); raw.push_back(c); escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') { result = JsonUnescape(raw); return true; }
        raw.push_back(c);
    }
    return false;
}

bool ExtractModelIds(const std::string& json, std::vector<std::string>& models) {
    const size_t dataMarker = json.find("\"data\"");
    if (dataMarker == std::string::npos) return false;
    const size_t arrayStart = json.find('[', dataMarker);
    if (arrayStart == std::string::npos) return false;
    const size_t arrayEnd = json.find(']', arrayStart);
    if (arrayEnd == std::string::npos) return false;
    size_t position = arrayStart + 1;
    while (position < arrayEnd) {
        const size_t idMarker = json.find("\"id\"", position);
        if (idMarker == std::string::npos || idMarker >= arrayEnd) break;
        size_t valueStart = json.find(':', idMarker + 4);
        if (valueStart == std::string::npos || valueStart >= arrayEnd) break;
        ++valueStart;
        while (valueStart < arrayEnd && (json[valueStart] == ' ' || json[valueStart] == '\r' ||
                                         json[valueStart] == '\n' || json[valueStart] == '\t')) ++valueStart;
        if (valueStart >= arrayEnd || json[valueStart] != '"') { position = valueStart + 1; continue; }
        ++valueStart;
        std::string raw;
        bool escaped = false;
        size_t cursor = valueStart;
        for (; cursor < arrayEnd; ++cursor) {
            const char c = json[cursor];
            if (escaped) { raw.push_back('\\'); raw.push_back(c); escaped = false; continue; }
            if (c == '\\') { escaped = true; continue; }
            if (c == '"') break;
            raw.push_back(c);
        }
        if (cursor < arrayEnd) {
            const std::string id = JsonUnescape(raw);
            if (!id.empty()) models.push_back(id);
            position = cursor + 1;
        } else {
            break;
        }
    }
    return !models.empty();
}

std::wstring LastErrorMessage(const wchar_t* action) {
    const DWORD error = GetLastError();
    wchar_t systemMessage[256]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error, 0, systemMessage, ARRAYSIZE(systemMessage), nullptr);
    std::wstring message = action;
    message += L" (code ";
    message += std::to_wstring(error);
    message += L"): ";
    message += systemMessage;
    return message;
}

bool HttpPostJson(const std::wstring& endpoint, const std::wstring& apiKey,
                  const std::string& body, std::string& response, std::wstring& error) {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(endpoint.c_str(), 0, 0, &components)) {
        error = LastErrorMessage(L"API 地址无效");
        return false;
    }
    const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path = components.dwUrlPathLength > 0
        ? std::wstring(components.lpszUrlPath, components.dwUrlPathLength) : L"/";
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path == L"/" || path.empty()) {
        path = L"/v1/chat/completions";
    } else if (path == L"/v1" || path == L"/v1/") {
        path = L"/v1/chat/completions";
    }

    HINTERNET session = WinHttpOpen(L"TranslatorStudio/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = LastErrorMessage(L"无法打开网络会话"); return false; }
    WinHttpSetTimeouts(session, 10000, 10000, 60000, 60000);
    HINTERNET connection = WinHttpConnect(session, host.c_str(), components.nPort, 0);
    if (!connection) {
        error = LastErrorMessage(L"无法连接到 API 主机");
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        error = LastErrorMessage(L"无法创建 API 请求");
        WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return false;
    }
    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    if (!apiKey.empty()) {
        headers += L"Authorization: Bearer ";
        headers += apiKey;
        headers += L"\r\n";
    }
    const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                                         const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        error = LastErrorMessage(L"API 请求失败");
        WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return false;
    }
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    std::string data;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        chunk.resize(read);
        data += chunk;
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    response = std::move(data);
    if (statusCode < 200 || statusCode >= 300) {
        std::string apiError;
        if (ExtractJsonString(response, "message", apiError)) {
            error = L"API 错误 (HTTP " + std::to_wstring(statusCode) + L"): " + Utf8ToWide(apiError);
        } else {
            error = L"API 错误 (HTTP " + std::to_wstring(statusCode) + L")";
        }
        return false;
    }
    return true;
}

bool HttpGetModels(const std::wstring& endpoint, const std::wstring& apiKey,
                   std::string& response, std::wstring& error) {
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(endpoint.c_str(), 0, 0, &components)) {
        error = LastErrorMessage(L"API 地址无效");
        return false;
    }
    const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path = components.dwUrlPathLength > 0
        ? std::wstring(components.lpszUrlPath, components.dwUrlPathLength) : L"/";
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    const size_t queryStart = path.find(L'?');
    const std::wstring query = queryStart == std::wstring::npos ? L"" : path.substr(queryStart);
    if (queryStart != std::wstring::npos) path.resize(queryStart);
    if (path == L"/" || path.empty()) path = L"/v1/models";
    else if (path == L"/v1" || path == L"/v1/") path = L"/v1/models";
    else if (path.size() >= 17 && path.compare(path.size() - 17, 17, L"/chat/completions") == 0) {
        path.resize(path.size() - 17);
        if (path.empty()) path = L"/";
        if (path.back() == L'/') path.pop_back();
        path += L"/models";
    } else {
        if (path.back() != L'/') path += L'/';
        path += L"models";
    }
    path += query;

    HINTERNET session = WinHttpOpen(L"TranslatorStudio/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = LastErrorMessage(L"无法打开网络会话"); return false; }
    WinHttpSetTimeouts(session, 10000, 10000, 20000, 20000);
    HINTERNET connection = WinHttpConnect(session, host.c_str(), components.nPort, 0);
    if (!connection) {
        error = LastErrorMessage(L"无法连接到 API 主机");
        WinHttpCloseHandle(session);
        return false;
    }
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        error = LastErrorMessage(L"无法创建模型请求");
        WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return false;
    }
    std::wstring headers = L"Accept: application/json\r\n";
    if (!apiKey.empty()) {
        headers += L"Authorization: Bearer ";
        headers += apiKey;
        headers += L"\r\n";
    }
    if (!WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        error = LastErrorMessage(L"获取模型失败");
        WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return false;
    }
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    std::string data;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
        chunk.resize(read);
        data += chunk;
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    response = std::move(data);
    if (statusCode < 200 || statusCode >= 300) {
        std::string apiError;
        if (ExtractJsonString(response, "message", apiError)) {
            error = L"获取模型失败 (HTTP " + std::to_wstring(statusCode) + L"): " + Utf8ToWide(apiError);
        } else {
            error = L"获取模型失败 (HTTP " + std::to_wstring(statusCode) + L")";
        }
        return false;
    }
    return true;
}

void ApplyFont(HWND control, HFONT font) {
    if (control && font) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void SetStatus(const std::wstring& status, COLORREF color = RGB(100, 116, 139)) {
    g_app.statusColor = color;
    SetText(g_app.status, status);
    InvalidateRect(g_app.status, nullptr, TRUE);
}

void UpdateCharCounts() {
    const int inLen = GetWindowTextLengthW(g_app.input);
    std::wstring inStr = std::to_wstring(inLen) + L" 字符";
    SetText(g_app.inputCount, inStr);

    const int outLen = GetWindowTextLengthW(g_app.output);
    std::wstring outStr = std::to_wstring(outLen) + L" 字符";
    SetText(g_app.outputCount, outStr);
}

void UpdateModelLabel() {
    if (g_app.currentModelLabel) {
        std::wstring text = L"模型: " + g_app.selectedModel;
        SetText(g_app.currentModelLabel, text);
    }
}

void LayoutControls(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    const int marginX = 16;
    const int headerY = 10;

    // Header items
    MoveWindow(g_app.titleLabel, marginX, headerY, 140, 24, TRUE);
    MoveWindow(g_app.badgeLabel, marginX + 144, headerY + 2, 96, 18, TRUE);
    MoveWindow(g_app.subtitleLabel, marginX, headerY + 24, std::max(180, width - marginX * 2 - 80), 16, TRUE);

    const int topBtnW = 68;
    const int topBtnH = 28;
    MoveWindow(g_app.settingsButton, width - marginX - topBtnW, headerY + 2, topBtnW, topBtnH, TRUE);

    // Toolbar Card (y = 48 to 94, height = 46)
    const int tbY = 48;
    const int tbH = 46;
    const int tbInnerY = tbY + 8;
    const int tbCtrlH = 28;

    int curX = marginX + 12;
    MoveWindow(g_app.sourceLabel, curX, tbInnerY + 4, 48, 18, TRUE);
    curX += 50;
    MoveWindow(g_app.source, curX, tbInnerY, 120, 240, TRUE);
    curX += 126;
    MoveWindow(g_app.swapButton, curX, tbInnerY, 62, tbCtrlH, TRUE);
    curX += 68;
    MoveWindow(g_app.targetLabel, curX, tbInnerY + 4, 60, 18, TRUE);
    curX += 62;
    MoveWindow(g_app.target, curX, tbInnerY, 120, 240, TRUE);
    curX += 130;

    const int modelLblW = std::max(100, width - marginX - 16 - curX);
    MoveWindow(g_app.currentModelLabel, curX, tbInnerY + 4, modelLblW, 20, TRUE);

    // Workspace Cards
    const int wsTop = tbY + tbH + 12;
    const int footerHeight = 46;
    const int wsHeight = std::max(140, height - wsTop - footerHeight);
    const int cardGap = 12;
    const int totalCardWidth = width - marginX * 2;
    const int cardWidth = (totalCardWidth - cardGap) / 2;

    // Left Card: Input
    const int card1X = marginX;
    const int cardHeaderH = 32;
    MoveWindow(g_app.inputTitle, card1X + 12, wsTop + 7, 95, 18, TRUE);
    MoveWindow(g_app.inputCount, card1X + 110, wsTop + 8, 80, 16, TRUE);
    MoveWindow(g_app.inputClear, card1X + cardWidth - 10 - 52, wsTop + 5, 52, 22, TRUE);
    MoveWindow(g_app.inputPaste, card1X + cardWidth - 10 - 52 - 6 - 52, wsTop + 5, 52, 22, TRUE);
    MoveWindow(g_app.input, card1X + 8, wsTop + cardHeaderH, cardWidth - 16, wsHeight - cardHeaderH - 8, TRUE);

    // Right Card: Output
    const int card2X = marginX + cardWidth + cardGap;
    MoveWindow(g_app.outputTitle, card2X + 12, wsTop + 7, 110, 18, TRUE);
    MoveWindow(g_app.outputCount, card2X + 124, wsTop + 8, 80, 16, TRUE);
    MoveWindow(g_app.outputCopy, card2X + cardWidth - 10 - 72, wsTop + 5, 72, 22, TRUE);
    MoveWindow(g_app.output, card2X + 8, wsTop + cardHeaderH, cardWidth - 16, wsHeight - cardHeaderH - 8, TRUE);

    // Footer Action & Status Bar
    const int footerY = height - 38;
    const int btnH = 30;
    const int btnTranslateW = 125;
    const int btnOtherW = 76;
    const int btnClearW = 56;

    MoveWindow(g_app.translate, width - marginX - btnTranslateW, footerY, btnTranslateW, btnH, TRUE);
    MoveWindow(g_app.copyBottom, width - marginX - btnTranslateW - 8 - btnOtherW, footerY, btnOtherW, btnH, TRUE);
    MoveWindow(g_app.clearAll, width - marginX - btnTranslateW - 8 - btnOtherW - 8 - btnClearW, footerY, btnClearW, btnH, TRUE);

    const int statusWidth = width - marginX - btnTranslateW - btnOtherW - btnClearW - 32 - marginX;
    MoveWindow(g_app.status, marginX, footerY + 5, std::max(80, statusWidth), 20, TRUE);

    InvalidateRect(window, nullptr, TRUE);
}

void CopyOutput() {
    const std::wstring text = GetText(g_app.output);
    if (text.empty()) {
        SetStatus(L"暂无可复制的翻译结果", RGB(220, 38, 38));
        return;
    }
    if (!OpenClipboard(g_app.window)) return;
    EmptyClipboard();
    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* data = GlobalLock(memory);
        memcpy(data, text.c_str(), bytes);
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
    }
    CloseClipboard();
    SetStatus(L"翻译结果已复制到剪贴板", RGB(22, 163, 74));
}

void StartTranslation() {
    if (g_app.modelWorking.load() || g_app.working.exchange(true)) return;
    const std::wstring input = GetText(g_app.input);
    if (input.empty()) {
        g_app.working = false;
        SetStatus(L"请先输入需要翻译的源文本", RGB(220, 38, 38));
        return;
    }
    if (g_app.selectedModel.empty()) {
        g_app.working = false;
        SetStatus(L"请在设置中配置模型名称", RGB(220, 38, 38));
        return;
    }

    g_app.sourceIndex = static_cast<int>(SendMessageW(g_app.source, CB_GETCURSEL, 0, 0));
    g_app.targetIndex = static_cast<int>(SendMessageW(g_app.target, CB_GETCURSEL, 0, 0));
    SaveConfig();

    const std::wstring source = kLanguages[g_app.sourceIndex >= 0 && g_app.sourceIndex < 15 ? g_app.sourceIndex : 0];
    const std::wstring target = kLanguages[g_app.targetIndex >= 0 && g_app.targetIndex < 15 ? g_app.targetIndex : 1];

    SetStatus(L"正在翻译中...", RGB(37, 99, 235));
    EnableWindow(g_app.translate, FALSE);
    SetText(g_app.output, L"");
    UpdateCharCounts();

    g_app.startTime = std::chrono::steady_clock::now();

    const bool preserveFmt = g_app.preserveFormat;

    if (g_app.worker.joinable()) g_app.worker.join();
    g_app.worker = std::thread([preserveFmt](std::wstring endpoint, std::wstring apiKey, std::wstring modelName,
                                             std::wstring srcLang, std::wstring tgtLang, std::wstring inText) {
        auto* result = new TranslationResult();
        auto start = std::chrono::steady_clock::now();

        std::wstring languageInstruction;
        if (srcLang == L"自动检测") {
            languageInstruction = L"\n\nTarget language: " + tgtLang + L"\nSource language: Auto-detect (Automatically detect source language)";
        } else {
            languageInstruction = L"\n\nSource language: " + srcLang + L"\nTarget language: " + tgtLang;
        }

        if (preserveFmt) {
            languageInstruction += L"\nFormatting Requirement: Strictly maintain the EXACT same line-by-line structure, list items, blank lines, and emojis. Do not merge lines.";
        }

        const wchar_t* basePrompt = (tgtLang == L"英语") ? kEnglishSystemPrompt : kGeneralSystemPrompt;
        std::string system = JsonEscape(WideToUtf8(std::wstring(basePrompt) + languageInstruction));
        std::string user = JsonEscape(WideToUtf8(inText));
        std::string body = "{\"model\":\"" + JsonEscape(WideToUtf8(modelName)) +
            "\",\"temperature\":0.2,\"messages\":[{\"role\":\"system\",\"content\":\"" + system +
            "\"},{\"role\":\"user\",\"content\":\"" + user + "\"}]}";

        std::string response;
        std::wstring error;
        if (HttpPostJson(endpoint, apiKey, body, response, error)) {
            std::string content;
            if (ExtractJsonString(response, "content", content)) {
                result->ok = true;
                result->text = NormalizeToCrLf(Utf8ToWide(content));
            } else {
                result->text = L"API 响应解析失败：未找到 content 字段。";
            }
        } else {
            result->text = error;
        }

        auto finish = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        result->durationSeconds = elapsed.count();

        if (!g_app.shuttingDown.load()) {
            PostMessageW(g_app.window, WM_APP_TRANSLATION_DONE, 0, reinterpret_cast<LPARAM>(result));
        } else {
            delete result;
        }
    }, g_app.endpoint, g_app.apiKey, g_app.selectedModel, source, target, input);
}

void PasteInput() {
    if (!OpenClipboard(g_app.window)) return;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        auto* pText = static_cast<wchar_t*>(GlobalLock(hData));
        if (pText) {
            SetText(g_app.input, NormalizeToCrLf(pText));
            GlobalUnlock(hData);
            UpdateCharCounts();
            SetStatus(L"已粘贴剪贴板内容", RGB(100, 116, 139));
        }
    }
    CloseClipboard();
}

void SwapLanguages() {
    int srcIdx = static_cast<int>(SendMessageW(g_app.source, CB_GETCURSEL, 0, 0));
    int tgtIdx = static_cast<int>(SendMessageW(g_app.target, CB_GETCURSEL, 0, 0));

    std::wstring outText = GetText(g_app.output);
    std::wstring inText = GetText(g_app.input);

    if (!outText.empty()) {
        SetText(g_app.input, outText);
        SetText(g_app.output, inText);
    }

    if (srcIdx == 0) { // 自动检测
        SendMessageW(g_app.source, CB_SETCURSEL, tgtIdx, 0);
        int newTgt = (tgtIdx == 2) ? 1 : 2; // Default to English or Simplified Chinese
        SendMessageW(g_app.target, CB_SETCURSEL, newTgt, 0);
    } else {
        SendMessageW(g_app.source, CB_SETCURSEL, tgtIdx, 0);
        SendMessageW(g_app.target, CB_SETCURSEL, srcIdx, 0);
    }

    g_app.sourceIndex = static_cast<int>(SendMessageW(g_app.source, CB_GETCURSEL, 0, 0));
    g_app.targetIndex = static_cast<int>(SendMessageW(g_app.target, CB_GETCURSEL, 0, 0));
    SaveConfig();

    UpdateCharCounts();
    SetStatus(L"已互换源语言和目标语言", RGB(100, 116, 139));
}

// Subclass edit control to support Ctrl + Enter for quick translation
LRESULT CALLBACK InputEditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNREFERENCED_PARAMETER(uIdSubclass);
    UNREFERENCED_PARAMETER(dwRefData);

    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
            StartTranslation();
            return 0;
        }
    } else if (uMsg == WM_CHAR) {
        if (wParam == 10 || (wParam == 13 && (GetKeyState(VK_CONTROL) & 0x8000))) {
            return 0; // Prevent bell or unwanted carriage return on Ctrl+Enter
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Settings Dialog Proc
LRESULT CALLBACK SettingsWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_settings.testColor = RGB(100, 116, 139);

        // Header Title
        HWND title = CreateWindowExW(0, L"STATIC", L"系统设置", WS_CHILD | WS_VISIBLE,
                                     20, 14, 200, 22, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(title, g_app.fontBold);

        HWND subtitle = CreateWindowExW(0, L"STATIC", L"管理软件排版偏好、API 接口及大模型配置", WS_CHILD | WS_VISIBLE,
                                        20, 36, 420, 16, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(subtitle, g_app.fontSmall);

        // Section 1: 常规与排版设置
        HWND sec1 = CreateWindowExW(0, L"STATIC", L"常规与排版设置", WS_CHILD | WS_VISIBLE,
                                    20, 60, 140, 18, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(sec1, g_app.fontSection);

        g_settings.preserveFormat = CreateWindowExW(0, L"BUTTON", L"严格保持原文排版与换行格式 (1:1 列表/段落对齐)",
                                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                    20, 82, 380, 20, window,
                                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_PRESERVE_FORMAT)),
                                                    GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.preserveFormat, g_app.fontNormal);
        if (g_app.preserveFormat) {
            SendMessageW(g_settings.preserveFormat, BM_SETCHECK, BST_CHECKED, 0);
        }

        g_settings.autoCopy = CreateWindowExW(0, L"BUTTON", L"翻译完成后自动将译文复制到剪贴板",
                                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                              20, 106, 380, 20, window,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_AUTO_COPY)),
                                              GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.autoCopy, g_app.fontNormal);
        if (g_app.autoCopy) {
            SendMessageW(g_settings.autoCopy, BM_SETCHECK, BST_CHECKED, 0);
        }

        g_settings.autoStart = CreateWindowExW(0, L"BUTTON", L"开机时自动启动软件 (Windows 自启动)",
                                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                               20, 130, 380, 20, window,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_AUTO_START)),
                                               GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.autoStart, g_app.fontNormal);
        if (IsAutoStartEnabled()) {
            SendMessageW(g_settings.autoStart, BM_SETCHECK, BST_CHECKED, 0);
        }

        // Section 2: API 与模型设置
        HWND sec2 = CreateWindowExW(0, L"STATIC", L"API 与模型", WS_CHILD | WS_VISIBLE,
                                    20, 160, 120, 18, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(sec2, g_app.fontSection);

        // Endpoint
        HWND lbl1 = CreateWindowExW(0, L"STATIC", L"API 基础地址 (Endpoint):", WS_CHILD | WS_VISIBLE,
                                    20, 184, 200, 16, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(lbl1, g_app.fontNormal);

        g_settings.endpoint = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                              20, 204, 440, 26, window,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_ENDPOINT)),
                                              GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.endpoint, g_app.fontNormal);
        SendMessageW(g_settings.endpoint, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
        SetText(g_settings.endpoint, g_app.endpoint);

        // API Key
        HWND lbl2 = CreateWindowExW(0, L"STATIC", L"API 密钥 (API Key):", WS_CHILD | WS_VISIBLE,
                                    20, 238, 160, 16, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(lbl2, g_app.fontNormal);

        g_settings.showKey = CreateWindowExW(0, L"BUTTON", L"显示明文", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                             384, 236, 76, 18, window,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_SHOW_KEY)),
                                             GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.showKey, g_app.fontSmall);

        g_settings.apiKey = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
                                            20, 258, 440, 26, window,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_API_KEY)),
                                            GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.apiKey, g_app.fontNormal);
        SendMessageW(g_settings.apiKey, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(6, 6));
        SetText(g_settings.apiKey, g_app.apiKey);

        // Model
        HWND lbl3 = CreateWindowExW(0, L"STATIC", L"翻译大模型 (Model):", WS_CHILD | WS_VISIBLE,
                                    20, 292, 160, 16, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(lbl3, g_app.fontNormal);

        g_settings.fetchModels = CreateWindowExW(0, L"BUTTON", L"获取可用模型", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                                 360, 290, 100, 22, window,
                                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_FETCH_MODELS)),
                                                 GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.fetchModels, g_app.fontSmall);

        g_settings.model = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                           CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
                                           20, 312, 440, 200, window,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_MODEL)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.model, g_app.fontNormal);
        for (const wchar_t* pm : kPresetModels) {
            SendMessageW(g_settings.model, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pm));
        }
        SetText(g_settings.model, g_app.selectedModel);

        // Test Connection Button & Status
        g_settings.testButton = CreateWindowExW(0, L"BUTTON", L"测试连接", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                                20, 350, 92, 28, window,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_TEST)),
                                                GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.testButton, g_app.fontNormal);

        g_settings.testStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                                122, 356, 338, 18, window,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_TEST_STATUS)),
                                                GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.testStatus, g_app.fontSmall);

        // Buttons
        g_settings.save = CreateWindowExW(0, L"BUTTON", L"保存设置", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                          270, 396, 92, 30, window,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_SAVE)),
                                          GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.save, g_app.fontBold);

        g_settings.cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                            370, 396, 90, 30, window,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS_CANCEL)),
                                            GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_settings.cancel, g_app.fontNormal);

        SetFocus(g_settings.endpoint);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = reinterpret_cast<HDC>(wParam);
        HWND hwndStatic = reinterpret_cast<HWND>(lParam);
        SetBkMode(hdcStatic, TRANSPARENT);
        if (hwndStatic == g_settings.testStatus) {
            SetTextColor(hdcStatic, g_settings.testColor);
        } else {
            SetTextColor(hdcStatic, RGB(30, 41, 59));
        }
        return reinterpret_cast<INT_PTR>(g_app.cardBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdcEdit, RGB(255, 255, 255));
        SetTextColor(hdcEdit, RGB(15, 23, 42));
        return reinterpret_cast<INT_PTR>(g_app.cardBrush);
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_SETTINGS_SHOW_KEY:
            if (HIWORD(wParam) == BN_CLICKED) {
                const bool isChecked = (SendMessageW(g_settings.showKey, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SendMessageW(g_settings.apiKey, EM_SETPASSWORDCHAR, isChecked ? 0 : static_cast<WPARAM>(L'●'), 0);
                InvalidateRect(g_settings.apiKey, nullptr, TRUE);
            }
            return 0;
        case ID_SETTINGS_FETCH_MODELS:
            if (HIWORD(wParam) == BN_CLICKED) {
                const std::wstring endpoint = GetText(g_settings.endpoint);
                const std::wstring apiKey = GetText(g_settings.apiKey);
                g_settings.testColor = RGB(217, 119, 6);
                SetText(g_settings.testStatus, L"正在获取模型列表...");
                EnableWindow(g_settings.fetchModels, FALSE);

                std::thread([window, endpoint, apiKey]() {
                    auto* result = new ModelListResult();
                    std::string response;
                    if (HttpGetModels(endpoint, apiKey, response, result->error)) {
                        std::vector<std::string> modelIds;
                        if (ExtractModelIds(response, modelIds)) {
                            result->ok = true;
                            for (const std::string& id : modelIds) result->models.push_back(Utf8ToWide(id));
                        } else {
                            result->error = L"未解析到模型列表";
                        }
                    }
                    if (IsWindow(window)) {
                        PostMessageW(window, WM_APP_MODELS_DONE, 0, reinterpret_cast<LPARAM>(result));
                    } else {
                        delete result;
                    }
                }).detach();
            }
            return 0;
        case ID_SETTINGS_TEST:
            if (HIWORD(wParam) == BN_CLICKED) {
                if (g_app.testWorking.exchange(true)) return 0;
                const std::wstring endpoint = GetText(g_settings.endpoint);
                const std::wstring apiKey = GetText(g_settings.apiKey);
                g_settings.testColor = RGB(217, 119, 6);
                SetText(g_settings.testStatus, L"正在测试 API 连通性...");
                EnableWindow(g_settings.testButton, FALSE);

                std::thread([window, endpoint, apiKey]() {
                    auto* result = new TestConnResult();
                    auto start = std::chrono::steady_clock::now();
                    std::string response;
                    std::wstring error;
                    if (HttpGetModels(endpoint, apiKey, response, error)) {
                        result->ok = true;
                        auto finish = std::chrono::steady_clock::now();
                        result->latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
                        result->message = L"连接成功 (延迟 " + std::to_wstring(result->latencyMs) + L" ms)";
                    } else {
                        result->ok = false;
                        result->message = L"连接失败: " + error;
                    }
                    if (IsWindow(window)) {
                        PostMessageW(window, WM_APP_TEST_DONE, 0, reinterpret_cast<LPARAM>(result));
                    } else {
                        delete result;
                    }
                }).detach();
            }
            return 0;
        case ID_SETTINGS_SAVE:
            if (HIWORD(wParam) == BN_CLICKED) {
                g_app.endpoint = GetText(g_settings.endpoint);
                g_app.apiKey = GetText(g_settings.apiKey);
                g_app.selectedModel = GetText(g_settings.model);
                g_app.preserveFormat = (SendMessageW(g_settings.preserveFormat, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_app.autoCopy = (SendMessageW(g_settings.autoCopy, BM_GETCHECK, 0, 0) == BST_CHECKED);

                const bool autoStart = (SendMessageW(g_settings.autoStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetAutoStartEnabled(autoStart);

                SaveConfig();
                UpdateModelLabel();
                SetStatus(L"设置已保存", RGB(22, 163, 74));
                DestroyWindow(window);
                return 0;
            }
            break;
        case ID_SETTINGS_CANCEL:
            if (HIWORD(wParam) == BN_CLICKED) {
                DestroyWindow(window);
                return 0;
            }
            break;
        }
        break;
    case WM_APP_MODELS_DONE: {
        auto* result = reinterpret_cast<ModelListResult*>(lParam);
        if (result) {
            if (result->ok) {
                const std::wstring current = GetText(g_settings.model);
                SendMessageW(g_settings.model, CB_RESETCONTENT, 0, 0);
                for (const std::wstring& modelName : result->models) {
                    SendMessageW(g_settings.model, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(modelName.c_str()));
                }
                int selected = static_cast<int>(SendMessageW(g_settings.model, CB_FINDSTRINGEXACT,
                                                              static_cast<WPARAM>(-1),
                                                              reinterpret_cast<LPARAM>(current.c_str())));
                if (selected == CB_ERR && !result->models.empty()) selected = 0;
                if (selected != CB_ERR) SendMessageW(g_settings.model, CB_SETCURSEL, selected, 0);
                g_settings.testColor = RGB(22, 163, 74);
                SetText(g_settings.testStatus, L"已获取 " + std::to_wstring(result->models.size()) + L" 个模型");
            } else {
                g_settings.testColor = RGB(220, 38, 38);
                SetText(g_settings.testStatus, result->error.empty() ? L"获取模型失败" : (L"错误: " + result->error));
            }
            delete result;
        }
        EnableWindow(g_settings.fetchModels, TRUE);
        return 0;
    }
    case WM_APP_TEST_DONE: {
        auto* result = reinterpret_cast<TestConnResult*>(lParam);
        if (result) {
            g_settings.testColor = result->ok ? RGB(22, 163, 74) : RGB(220, 38, 38);
            SetText(g_settings.testStatus, result->message);
            delete result;
        }
        g_app.testWorking = false;
        EnableWindow(g_settings.testButton, TRUE);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (g_app.settingsDialog == window) {
            g_app.settingsDialog = nullptr;
            EnableWindow(g_app.window, TRUE);
            SetForegroundWindow(g_app.window);
        }
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowSettings() {
    if (IsWindow(g_app.settingsDialog)) {
        SetForegroundWindow(g_app.settingsDialog);
        return;
    }
    EnableWindow(g_app.window, FALSE);
    g_app.settingsDialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
                                           kSettingsClass, L"设置", WS_POPUP | WS_CAPTION |
                                           WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
                                           496, 480, g_app.window, nullptr,
                                           GetModuleHandleW(nullptr), nullptr);
    if (!g_app.settingsDialog) {
        EnableWindow(g_app.window, TRUE);
        return;
    }
    RECT parentRect{}, dialogRect{};
    GetWindowRect(g_app.window, &parentRect);
    GetWindowRect(g_app.settingsDialog, &dialogRect);
    const int dialogWidth = dialogRect.right - dialogRect.left;
    const int dialogHeight = dialogRect.bottom - dialogRect.top;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - dialogWidth) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - dialogHeight) / 2;
    SetWindowPos(g_app.settingsDialog, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_app.window = window;
        LoadConfig();

        // Initialize Compact & Crisp Fonts
        g_app.fontTitle = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontSubtitle = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontSection = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontNormal = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        g_app.fontCode = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // GDI Resources
        g_app.bgBrush = CreateSolidBrush(RGB(246, 248, 250));
        g_app.cardBrush = CreateSolidBrush(RGB(255, 255, 255));
        g_app.toolbarBrush = CreateSolidBrush(RGB(255, 255, 255));
        g_app.accentBrush = CreateSolidBrush(RGB(37, 99, 235));
        g_app.cardBorderPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        g_app.dividerPen = CreatePen(PS_SOLID, 1, RGB(235, 238, 242));

        // Header
        g_app.titleLabel = CreateWindowExW(0, L"STATIC", L"智能翻译工作台", WS_CHILD | WS_VISIBLE,
                                           0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.titleLabel, g_app.fontTitle);

        g_app.badgeLabel = CreateWindowExW(0, L"STATIC", L"AI Studio v1.0.1", WS_CHILD | WS_VISIBLE,
                                           0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.badgeLabel, g_app.fontSmall);

        g_app.subtitleLabel = CreateWindowExW(0, L"STATIC", L"原生极速大模型翻译引擎 · 地道口语表达 · 响应迅速", WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.subtitleLabel, g_app.fontSubtitle);

        g_app.settingsButton = CreateWindowExW(0, L"BUTTON", L"设置", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                               0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SETTINGS)),
                                               GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.settingsButton, g_app.fontNormal);

        // Toolbar: Languages & Model
        g_app.sourceLabel = CreateWindowExW(0, L"STATIC", L"源语言:", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.sourceLabel, g_app.fontBold);

        g_app.source = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SOURCE)),
                                       GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.source, g_app.fontNormal);

        g_app.swapButton = CreateWindowExW(0, L"BUTTON", L"⇄ 互换", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SWAP)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.swapButton, g_app.fontNormal);

        g_app.targetLabel = CreateWindowExW(0, L"STATIC", L"目标语言:", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.targetLabel, g_app.fontBold);

        g_app.target = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TARGET)),
                                       GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.target, g_app.fontNormal);

        for (const wchar_t* lang : kLanguages) {
            SendMessageW(g_app.source, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lang));
            SendMessageW(g_app.target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lang));
        }
        SendMessageW(g_app.source, CB_SETCURSEL, g_app.sourceIndex, 0);
        SendMessageW(g_app.target, CB_SETCURSEL, g_app.targetIndex, 0);

        g_app.currentModelLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                                  0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CURRENT_MODEL_LBL)),
                                                  GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.currentModelLabel, g_app.fontSmall);
        UpdateModelLabel();

        // Workspace: Left Input Card
        g_app.inputTitle = CreateWindowExW(0, L"STATIC", L"源文本 (Input)", WS_CHILD | WS_VISIBLE,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INPUT_TITLE)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.inputTitle, g_app.fontSection);

        g_app.inputCount = CreateWindowExW(0, L"STATIC", L"0 字符", WS_CHILD | WS_VISIBLE,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INPUT_COUNT)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.inputCount, g_app.fontSmall);

        g_app.inputPaste = CreateWindowExW(0, L"BUTTON", L"粘贴", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INPUT_PASTE)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.inputPaste, g_app.fontSmall);

        g_app.inputClear = CreateWindowExW(0, L"BUTTON", L"清空", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INPUT_CLEAR)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.inputClear, g_app.fontSmall);

        g_app.input = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                                      ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INPUT)),
                                      GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.input, g_app.fontNormal);
        SendMessageW(g_app.input, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
        SendMessageW(g_app.input, EM_SETLIMITTEXT, 2000000, 0);
        SetWindowSubclass(g_app.input, InputEditSubclassProc, 1, 0);

        // Workspace: Right Output Card
        g_app.outputTitle = CreateWindowExW(0, L"STATIC", L"翻译结果 (Output)", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OUTPUT_TITLE)),
                                            GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.outputTitle, g_app.fontSection);

        g_app.outputCount = CreateWindowExW(0, L"STATIC", L"0 字符", WS_CHILD | WS_VISIBLE,
                                            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OUTPUT_COUNT)),
                                            GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.outputCount, g_app.fontSmall);

        g_app.outputCopy = CreateWindowExW(0, L"BUTTON", L"复制结果", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OUTPUT_COPY)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.outputCopy, g_app.fontSmall);

        g_app.output = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
                                       ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_READONLY | WS_TABSTOP,
                                       0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_OUTPUT)),
                                       GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.output, g_app.fontNormal);
        SendMessageW(g_app.output, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(8, 8));
        SendMessageW(g_app.output, EM_SETLIMITTEXT, 2000000, 0);

        // Footer Actions & Status
        g_app.translate = CreateWindowExW(0, L"BUTTON", L"翻译 (Ctrl+Enter)", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                         0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TRANSLATE)),
                                         GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.translate, g_app.fontBold);

        g_app.clearAll = CreateWindowExW(0, L"BUTTON", L"清空", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                         0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CLEAR_ALL)),
                                         GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.clearAll, g_app.fontNormal);

        g_app.copyBottom = CreateWindowExW(0, L"BUTTON", L"复制结果", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                           0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COPY_BOTTOM)),
                                           GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.copyBottom, g_app.fontNormal);

        g_app.status = CreateWindowExW(0, L"STATIC", L"就绪", WS_CHILD | WS_VISIBLE,
                                       0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)),
                                       GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_app.status, g_app.fontNormal);

        LayoutControls(window);
        UpdateCharCounts();
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(window, &ps);

        RECT client{};
        GetClientRect(window, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;

        // 1. Fill main background
        FillRect(hdc, &client, g_app.bgBrush);

        const int marginX = 16;

        // 2. Draw Toolbar Card
        const int tbY = 48;
        const int tbH = 46;
        RECT tbRect{marginX, tbY, width - marginX, tbY + tbH};
        HGDIOBJ oldBrush = SelectObject(hdc, g_app.toolbarBrush);
        HGDIOBJ oldPen = SelectObject(hdc, g_app.cardBorderPen);
        RoundRect(hdc, tbRect.left, tbRect.top, tbRect.right, tbRect.bottom, 10, 10);

        // 3. Draw Workspace Cards
        const int wsTop = tbY + tbH + 12;
        const int footerHeight = 46;
        const int wsHeight = std::max(140, height - wsTop - footerHeight);
        const int cardGap = 12;
        const int totalCardWidth = width - marginX * 2;
        const int cardWidth = (totalCardWidth - cardGap) / 2;
        const int cardHeaderH = 32;

        HBRUSH cardHeaderBrush = CreateSolidBrush(RGB(248, 250, 252));

        // Left Card
        RECT leftCardRect{marginX, wsTop, marginX + cardWidth, wsTop + wsHeight};
        RoundRect(hdc, leftCardRect.left, leftCardRect.top, leftCardRect.right, leftCardRect.bottom, 12, 12);
        RECT leftHeaderRect{marginX + 1, wsTop + 1, marginX + cardWidth - 1, wsTop + cardHeaderH};
        FillRect(hdc, &leftHeaderRect, cardHeaderBrush);
        MoveToEx(hdc, marginX, wsTop + cardHeaderH, nullptr);
        LineTo(hdc, marginX + cardWidth, wsTop + cardHeaderH);

        // Right Card
        const int card2X = marginX + cardWidth + cardGap;
        RECT rightCardRect{card2X, wsTop, card2X + cardWidth, wsTop + wsHeight};
        RoundRect(hdc, rightCardRect.left, rightCardRect.top, rightCardRect.right, rightCardRect.bottom, 12, 12);
        RECT rightHeaderRect{card2X + 1, wsTop + 1, card2X + cardWidth - 1, wsTop + cardHeaderH};
        FillRect(hdc, &rightHeaderRect, cardHeaderBrush);
        MoveToEx(hdc, card2X, wsTop + cardHeaderH, nullptr);
        LineTo(hdc, card2X + cardWidth, wsTop + cardHeaderH);

        DeleteObject(cardHeaderBrush);

        // Restore GDI Objects
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        EndPaint(window, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = reinterpret_cast<HDC>(wParam);
        HWND hwndStatic = reinterpret_cast<HWND>(lParam);
        SetBkMode(hdcStatic, TRANSPARENT);

        if (hwndStatic == g_app.status) {
            SetTextColor(hdcStatic, g_app.statusColor);
            return reinterpret_cast<INT_PTR>(g_app.bgBrush);
        }
        if (hwndStatic == g_app.titleLabel) {
            SetTextColor(hdcStatic, RGB(15, 23, 42));
            return reinterpret_cast<INT_PTR>(g_app.bgBrush);
        }
        if (hwndStatic == g_app.badgeLabel) {
            SetTextColor(hdcStatic, RGB(37, 99, 235));
            return reinterpret_cast<INT_PTR>(g_app.bgBrush);
        }
        if (hwndStatic == g_app.subtitleLabel) {
            SetTextColor(hdcStatic, RGB(100, 116, 139));
            return reinterpret_cast<INT_PTR>(g_app.bgBrush);
        }
        if (hwndStatic == g_app.inputCount || hwndStatic == g_app.outputCount) {
            SetTextColor(hdcStatic, RGB(148, 163, 184));
            return reinterpret_cast<INT_PTR>(g_app.cardBrush);
        }
        if (hwndStatic == g_app.inputTitle || hwndStatic == g_app.outputTitle) {
            SetTextColor(hdcStatic, RGB(30, 41, 59));
            return reinterpret_cast<INT_PTR>(g_app.cardBrush);
        }
        if (hwndStatic == g_app.currentModelLabel) {
            SetTextColor(hdcStatic, RGB(100, 116, 139));
            return reinterpret_cast<INT_PTR>(g_app.toolbarBrush);
        }
        // Toolbar labels
        SetTextColor(hdcStatic, RGB(51, 65, 85));
        return reinterpret_cast<INT_PTR>(g_app.toolbarBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = reinterpret_cast<HDC>(wParam);
        SetBkColor(hdcEdit, RGB(255, 255, 255));
        SetTextColor(hdcEdit, RGB(15, 23, 42));
        return reinterpret_cast<INT_PTR>(g_app.cardBrush);
    }
    case WM_CTLCOLORBTN: {
        return reinterpret_cast<INT_PTR>(g_app.bgBrush);
    }
    case WM_SIZE:
        LayoutControls(window);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* minMax = reinterpret_cast<MINMAXINFO*>(lParam);
        minMax->ptMinTrackSize.x = 680;
        minMax->ptMinTrackSize.y = 440;
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_INPUT:
            if (HIWORD(wParam) == EN_CHANGE) {
                UpdateCharCounts();
            }
            break;
        case ID_SOURCE:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                g_app.sourceIndex = static_cast<int>(SendMessageW(g_app.source, CB_GETCURSEL, 0, 0));
                SaveConfig();
            }
            break;
        case ID_TARGET:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                g_app.targetIndex = static_cast<int>(SendMessageW(g_app.target, CB_GETCURSEL, 0, 0));
                SaveConfig();
            }
            break;
        case ID_SWAP:
            if (HIWORD(wParam) == BN_CLICKED) SwapLanguages();
            break;
        case ID_TRANSLATE:
            if (HIWORD(wParam) == BN_CLICKED) StartTranslation();
            break;
        case ID_SETTINGS:
            if (HIWORD(wParam) == BN_CLICKED) ShowSettings();
            break;
        case ID_INPUT_PASTE:
            if (HIWORD(wParam) == BN_CLICKED) PasteInput();
            break;
        case ID_INPUT_CLEAR:
            if (HIWORD(wParam) == BN_CLICKED) {
                SetText(g_app.input, L"");
                UpdateCharCounts();
                SetStatus(L"已清空源文本", RGB(100, 116, 139));
            }
            break;
        case ID_OUTPUT_COPY:
        case ID_COPY_BOTTOM:
            if (HIWORD(wParam) == BN_CLICKED) CopyOutput();
            break;
        case ID_CLEAR_ALL:
            if (HIWORD(wParam) == BN_CLICKED) {
                SetText(g_app.input, L"");
                SetText(g_app.output, L"");
                UpdateCharCounts();
                SetStatus(L"就绪", RGB(22, 163, 74));
            }
            break;
        }
        return 0;
    case WM_APP_TRANSLATION_DONE: {
        auto* result = reinterpret_cast<TranslationResult*>(lParam);
        if (result) {
            SetText(g_app.output, result->text);
            UpdateCharCounts();
            if (result->ok) {
                std::wstringstream ss;
                ss << std::fixed << std::setprecision(2) << result->durationSeconds;
                SetStatus(L"翻译完成 (耗时 " + ss.str() + L" 秒)", RGB(22, 163, 74));
                if (g_app.autoCopy) {
                    CopyOutput();
                }
            } else {
                SetStatus(L"翻译失败: " + result->text, RGB(220, 38, 38));
            }
            delete result;
        }
        g_app.working = false;
        EnableWindow(g_app.translate, TRUE);
        return 0;
    }
    case WM_DESTROY:
        RemoveWindowSubclass(g_app.input, InputEditSubclassProc, 1);
        if (IsWindow(g_app.settingsDialog)) {
            EnableWindow(window, TRUE);
            DestroyWindow(g_app.settingsDialog);
        }
        g_app.shuttingDown = true;
        if (g_app.worker.joinable()) g_app.worker.join();

        if (g_app.fontTitle) DeleteObject(g_app.fontTitle);
        if (g_app.fontSubtitle) DeleteObject(g_app.fontSubtitle);
        if (g_app.fontSection) DeleteObject(g_app.fontSection);
        if (g_app.fontNormal) DeleteObject(g_app.fontNormal);
        if (g_app.fontBold) DeleteObject(g_app.fontBold);
        if (g_app.fontSmall) DeleteObject(g_app.fontSmall);
        if (g_app.fontCode) DeleteObject(g_app.fontCode);

        if (g_app.bgBrush) DeleteObject(g_app.bgBrush);
        if (g_app.cardBrush) DeleteObject(g_app.cardBrush);
        if (g_app.toolbarBrush) DeleteObject(g_app.toolbarBrush);
        if (g_app.accentBrush) DeleteObject(g_app.accentBrush);
        if (g_app.cardBorderPen) DeleteObject(g_app.cardBorderPen);
        if (g_app.dividerPen) DeleteObject(g_app.dividerPen);

        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(RGB(246, 248, 250));
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&windowClass)) return 0;

    WNDCLASSEXW settingsClass{sizeof(settingsClass)};
    settingsClass.hInstance = instance;
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.lpszClassName = kSettingsClass;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
    if (!RegisterClassExW(&settingsClass)) return 0;

    // Compact desktop translator window size similar to Youdao Translator (860 x 560)
    HWND window = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 860, 560,
                                  nullptr, nullptr, instance, nullptr);
    if (!window) return 0;
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
