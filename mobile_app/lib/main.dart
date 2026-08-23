import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'translation_service.dart';
import 'settings_page.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const TranslatorApp());
}

class TranslatorApp extends StatelessWidget {
  const TranslatorApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AI 智能翻译',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF2563EB),
          primary: const Color(0xFF2563EB),
        ),
        useMaterial3: true,
        fontFamily: 'Roboto',
      ),
      home: const TranslationHomePage(),
    );
  }
}

class TranslationHomePage extends StatefulWidget {
  const TranslationHomePage({super.key});

  @override
  State<TranslationHomePage> createState() => _TranslationHomePageState();
}

class _TranslationHomePageState extends State<TranslationHomePage> {
  final _inputController = TextEditingController();
  final _outputController = TextEditingController();

  final List<String> _languages = [
    '自动检测',
    '英语',
    '简体中文',
    '繁体中文',
    '日语',
    '韩语',
    '法语',
    '德语',
    '西班牙语',
    '俄语',
    '葡萄牙语',
    '意大利语',
    '阿拉伯语',
    '越南语',
    '泰语',
  ];

  String _sourceLang = '自动检测';
  String _targetLang = '英语';

  String _endpoint = TranslationService.defaultEndpoint;
  String _apiKey = TranslationService.defaultApiKey;
  String _model = TranslationService.defaultModel;
  bool _preserveFormat = true;
  bool _autoCopy = false;

  bool _isTranslating = false;
  String _statusText = '就绪';
  Color _statusColor = const Color(0xFF64748B);

  @override
  void initState() {
    super.initState();
    _loadConfig();
  }

  Future<void> _loadConfig() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _endpoint = prefs.getString('endpoint') ?? TranslationService.defaultEndpoint;
      _apiKey = prefs.getString('apiKey') ?? TranslationService.defaultApiKey;
      _model = prefs.getString('model') ?? TranslationService.defaultModel;
      _sourceLang = prefs.getString('sourceLang') ?? '自动检测';
      _targetLang = prefs.getString('targetLang') ?? '英语';
      _preserveFormat = prefs.getBool('preserveFormat') ?? true;
      _autoCopy = prefs.getBool('autoCopy') ?? false;
    });
  }

  Future<void> _saveLanguageConfig() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('sourceLang', _sourceLang);
    await prefs.setString('targetLang', _targetLang);
  }

  void _swapLanguages() {
    setState(() {
      final oldInput = _inputController.text;
      final oldOutput = _outputController.text;
      if (oldOutput.isNotEmpty) {
        _inputController.text = oldOutput;
        _outputController.text = oldInput;
      }

      if (_sourceLang == '自动检测') {
        _sourceLang = _targetLang;
        _targetLang = (_targetLang == '简体中文') ? '英语' : '简体中文';
      } else {
        final temp = _sourceLang;
        _sourceLang = _targetLang;
        _targetLang = temp;
      }
    });
    _saveLanguageConfig();
  }

  Future<void> _doTranslate() async {
    final text = _inputController.text.trim();
    if (text.isEmpty) {
      _showToast('请先输入需要翻译的文本');
      return;
    }

    FocusScope.of(context).unfocus();

    setState(() {
      _isTranslating = true;
      _statusText = '正在翻译中...';
      _statusColor = const Color(0xFF2563EB);
      _outputController.clear();
    });

    final res = await TranslationService.translate(
      endpoint: _endpoint,
      apiKey: _apiKey,
      model: _model,
      sourceLang: _sourceLang,
      targetLang: _targetLang,
      text: text,
      preserveFormat: _preserveFormat,
    );

    if (!mounted) return;

    setState(() {
      _isTranslating = false;
      if (res.ok) {
        _outputController.text = res.text;
        _statusText = '翻译完成 (耗时 ${res.durationSeconds.toStringAsFixed(2)} 秒)';
        _statusColor = const Color(0xFF16A34A);

        if (_autoCopy && res.text.isNotEmpty) {
          Clipboard.setData(ClipboardData(text: res.text));
          _showToast('译文已自动复制到剪贴板');
        }
      } else {
        _outputController.text = res.errorMessage ?? '翻译失败';
        _statusText = '翻译失败: ${res.errorMessage}';
        _statusColor = const Color(0xFFDC2626);
      }
    });
  }

  void _copyOutput() {
    if (_outputController.text.isNotEmpty) {
      Clipboard.setData(ClipboardData(text: _outputController.text));
      _showToast('翻译结果已复制到剪贴板');
    }
  }

  Future<void> _pasteInput() async {
    final data = await Clipboard.getData(Clipboard.kTextPlain);
    if (data?.text != null && data!.text!.isNotEmpty) {
      setState(() {
        _inputController.text = data.text!;
      });
      _showToast('已粘贴剪贴板内容');
    }
  }

  void _clearInput() {
    setState(() {
      _inputController.clear();
    });
  }

  void _showToast(String msg) {
    ScaffoldMessenger.of(context).hideCurrentSnackBar();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(msg),
        behavior: SnackBarBehavior.floating,
        duration: const Duration(seconds: 2),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF1F5F9),
      appBar: AppBar(
        backgroundColor: Colors.white,
        elevation: 0.5,
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              '智能翻译工作台',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Color(0xFF0F172A)),
            ),
            Text(
              '当前模型: $_model',
              style: const TextStyle(fontSize: 11, color: Color(0xFF64748B)),
            ),
          ],
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings_outlined, color: Color(0xFF334155)),
            tooltip: '设置',
            onPressed: () async {
              final updated = await Navigator.push(
                context,
                MaterialPageRoute(builder: (_) => const SettingsPage()),
              );
              if (updated == true) {
                _loadConfig();
              }
            },
          ),
        ],
      ),
      body: SafeArea(
        child: Column(
          children: [
            // Top Language Bar Card
            Container(
              margin: const EdgeInsets.fromLTRB(14, 12, 14, 8),
              padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
              decoration: BoxDecoration(
                color: Colors.white,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: const Color(0xFFE2E8F0)),
              ),
              child: Row(
                children: [
                  Expanded(
                    child: DropdownButtonHideUnderline(
                      child: DropdownButton<String>(
                        value: _sourceLang,
                        isExpanded: true,
                        icon: const Icon(Icons.keyboard_arrow_down, size: 20, color: Color(0xFF64748B)),
                        style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: Color(0xFF1E293B)),
                        items: _languages.map((l) => DropdownMenuItem(value: l, child: Text(l))).toList(),
                        onChanged: (val) {
                          if (val != null) {
                            setState(() => _sourceLang = val);
                            _saveLanguageConfig();
                          }
                        },
                      ),
                    ),
                  ),
                  IconButton(
                    icon: const Icon(Icons.swap_horiz_rounded, color: Color(0xFF2563EB), size: 26),
                    onPressed: _swapLanguages,
                    tooltip: '互换语言',
                  ),
                  Expanded(
                    child: DropdownButtonHideUnderline(
                      child: DropdownButton<String>(
                        value: _targetLang,
                        isExpanded: true,
                        icon: const Icon(Icons.keyboard_arrow_down, size: 20, color: Color(0xFF64748B)),
                        style: const TextStyle(fontSize: 15, fontWeight: FontWeight.w600, color: Color(0xFF1E293B)),
                        items: _languages
                            .where((l) => l != '自动检测')
                            .map((l) => DropdownMenuItem(value: l, child: Text(l)))
                            .toList(),
                        onChanged: (val) {
                          if (val != null) {
                            setState(() => _targetLang = val);
                            _saveLanguageConfig();
                          }
                        },
                      ),
                    ),
                  ),
                ],
              ),
            ),

            // Scrollable Dual Cards
            Expanded(
              child: ListView(
                padding: const EdgeInsets.symmetric(horizontal: 14),
                children: [
                  // Upper Card: Input Card
                  Container(
                    margin: const EdgeInsets.only(bottom: 12),
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: const Color(0xFFE2E8F0)),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        // Card Header
                        Padding(
                          padding: const EdgeInsets.fromLTRB(14, 10, 10, 0),
                          child: Row(
                            children: [
                              const Text('源文本', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 14, color: Color(0xFF1E293B))),
                              const SizedBox(width: 8),
                              Text('${_inputController.text.length} 字符', style: const TextStyle(fontSize: 11, color: Color(0xFF94A3B8))),
                              const Spacer(),
                              TextButton(
                                onPressed: _pasteInput,
                                style: TextButton.styleFrom(padding: const EdgeInsets.symmetric(horizontal: 8), minimumSize: Size.zero),
                                child: const Text('粘贴', style: TextStyle(fontSize: 12)),
                              ),
                              TextButton(
                                onPressed: _clearInput,
                                style: TextButton.styleFrom(padding: const EdgeInsets.symmetric(horizontal: 8), minimumSize: Size.zero),
                                child: const Text('清空', style: TextStyle(fontSize: 12, color: Color(0xFF64748B))),
                              ),
                            ],
                          ),
                        ),
                        // Input Field
                        Padding(
                          padding: const EdgeInsets.fromLTRB(14, 4, 14, 14),
                          child: TextField(
                            controller: _inputController,
                            maxLines: 5,
                            minLines: 4,
                            onChanged: (_) => setState(() {}),
                            style: const TextStyle(fontSize: 15, height: 1.5, color: Color(0xFF0F172A)),
                            decoration: const InputDecoration(
                              hintText: '输入或粘贴需要翻译的文本...',
                              hintStyle: TextStyle(color: Color(0xFF94A3B8), fontSize: 14),
                              border: InputBorder.none,
                              contentPadding: EdgeInsets.zero,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),

                  // Lower Card: Output Card
                  Container(
                    margin: const EdgeInsets.only(bottom: 12),
                    decoration: BoxDecoration(
                      color: Colors.white,
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: const Color(0xFFE2E8F0)),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        // Output Card Header
                        Padding(
                          padding: const EdgeInsets.fromLTRB(14, 10, 10, 0),
                          child: Row(
                            children: [
                              const Text('翻译结果', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 14, color: Color(0xFF1E293B))),
                              const SizedBox(width: 8),
                              Text('${_outputController.text.length} 字符', style: const TextStyle(fontSize: 11, color: Color(0xFF94A3B8))),
                              const Spacer(),
                              TextButton(
                                onPressed: _outputController.text.isEmpty ? null : _copyOutput,
                                style: TextButton.styleFrom(padding: const EdgeInsets.symmetric(horizontal: 8), minimumSize: Size.zero),
                                child: const Text('复制结果', style: TextStyle(fontSize: 12)),
                              ),
                            ],
                          ),
                        ),
                        // Output Field
                        Padding(
                          padding: const EdgeInsets.fromLTRB(14, 4, 14, 14),
                          child: TextField(
                            controller: _outputController,
                            readOnly: true,
                            maxLines: null,
                            minLines: 5,
                            style: const TextStyle(fontSize: 15, height: 1.5, color: Color(0xFF0F172A)),
                            decoration: const InputDecoration(
                              hintText: '翻译结果将在此展示...',
                              hintStyle: TextStyle(color: Color(0xFF94A3B8), fontSize: 14),
                              border: InputBorder.none,
                              contentPadding: EdgeInsets.zero,
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),

            // Bottom Action Bar
            Container(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 12),
              color: Colors.white,
              child: Column(
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          _statusText,
                          style: TextStyle(fontSize: 12, color: _statusColor, fontWeight: FontWeight.w500),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                      ),
                      const SizedBox(width: 8),
                      ElevatedButton(
                        onPressed: _isTranslating ? null : _doTranslate,
                        style: ElevatedButton.styleFrom(
                          backgroundColor: const Color(0xFF2563EB),
                          foregroundColor: Colors.white,
                          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
                          elevation: 0,
                        ),
                        child: _isTranslating
                            ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                            : const Text('立即翻译', style: TextStyle(fontSize: 15, fontWeight: FontWeight.bold)),
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
