import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'translation_service.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final _endpointController = TextEditingController();
  final _apiKeyController = TextEditingController();
  final _modelController = TextEditingController();

  bool _obscureKey = true;
  bool _preserveFormat = true;
  bool _autoCopy = false;

  bool _testing = false;
  String _testStatus = '';
  Color _testStatusColor = Colors.grey;

  bool _fetchingModels = false;
  List<String> _models = [
    'gpt-4o-mini',
    'gpt-4o',
    'claude-3-5-sonnet',
    'deepseek-chat',
    'deepseek-reasoner',
    'qwen-plus',
    'qwen-max',
    'gemini-1.5-flash',
    'gemini-1.5-pro',
  ];

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _endpointController.text =
          prefs.getString('endpoint') ?? TranslationService.defaultEndpoint;
      _apiKeyController.text =
          prefs.getString('apiKey') ?? TranslationService.defaultApiKey;
      _modelController.text =
          prefs.getString('model') ?? TranslationService.defaultModel;
      _preserveFormat = prefs.getBool('preserveFormat') ?? true;
      _autoCopy = prefs.getBool('autoCopy') ?? false;
    });
  }

  Future<void> _saveSettings() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('endpoint', _endpointController.text.trim());
    await prefs.setString('apiKey', _apiKeyController.text.trim());
    await prefs.setString('model', _modelController.text.trim());
    await prefs.setBool('preserveFormat', _preserveFormat);
    await prefs.setBool('autoCopy', _autoCopy);

    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('设置已保存'),
          backgroundColor: Color(0xFF16A34A),
          duration: Duration(seconds: 2),
        ),
      );
      Navigator.pop(context, true);
    }
  }

  Future<void> _testConnection() async {
    setState(() {
      _testing = true;
      _testStatus = '正在测试连接...';
      _testStatusColor = const Color(0xFFD97706);
    });

    final res = await TranslationService.testConnection(
      endpoint: _endpointController.text.trim(),
      apiKey: _apiKeyController.text.trim(),
    );

    if (mounted) {
      setState(() {
        _testing = false;
        _testStatus = res.message;
        _testStatusColor = res.ok ? const Color(0xFF16A34A) : const Color(0xFFDC2626);
      });
    }
  }

  Future<void> _fetchModels() async {
    setState(() {
      _fetchingModels = true;
      _testStatus = '正在获取模型列表...';
      _testStatusColor = const Color(0xFFD97706);
    });

    final list = await TranslationService.fetchModels(
      endpoint: _endpointController.text.trim(),
      apiKey: _apiKeyController.text.trim(),
    );

    if (mounted) {
      setState(() {
        _fetchingModels = false;
        if (list.isNotEmpty) {
          _models = list;
          _testStatus = '已获取 ${list.length} 个模型';
          _testStatusColor = const Color(0xFF16A34A);
          if (!list.contains(_modelController.text)) {
            _modelController.text = list.first;
          }
        } else {
          _testStatus = '获取模型失败，请检查 API 配置';
          _testStatusColor = const Color(0xFFDC2626);
        }
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFFF8FAFC),
      appBar: AppBar(
        title: const Text('系统设置', style: TextStyle(fontWeight: FontWeight.bold)),
        backgroundColor: Colors.white,
        elevation: 0.5,
        foregroundColor: const Color(0xFF0F172A),
        actions: [
          TextButton(
            onPressed: _saveSettings,
            child: const Text('保存', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Color(0xFF2563EB))),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          // Section 1: 常规与排版
          _buildSectionHeader('常规与排版设置'),
          Container(
            decoration: BoxDecoration(
              color: Colors.white,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: const Color(0xFFE2E8F0)),
            ),
            child: Column(
              children: [
                SwitchListTile(
                  title: const Text('严格保持原文排版与换行', style: TextStyle(fontSize: 15, fontWeight: FontWeight.w500)),
                  subtitle: const Text('1:1 保持清单、空行及 Emoji 结构，不合并行', style: TextStyle(fontSize: 12, color: Color(0xFF64748B))),
                  value: _preserveFormat,
                  activeColor: const Color(0xFF2563EB),
                  onChanged: (val) => setState(() => _preserveFormat = val),
                ),
                const Divider(height: 1, indent: 16, endIndent: 16),
                SwitchListTile(
                  title: const Text('自动复制翻译结果', style: TextStyle(fontSize: 15, fontWeight: FontWeight.w500)),
                  subtitle: const Text('翻译完成后自动将译文写入系统剪贴板', style: TextStyle(fontSize: 12, color: Color(0xFF64748B))),
                  value: _autoCopy,
                  activeColor: const Color(0xFF2563EB),
                  onChanged: (val) => setState(() => _autoCopy = val),
                ),
              ],
            ),
          ),
          const SizedBox(height: 20),

          // Section 2: API 与模型配置
          _buildSectionHeader('API 与模型配置'),
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: Colors.white,
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: const Color(0xFFE2E8F0)),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('API 基础地址 (Endpoint)', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: Color(0xFF334155))),
                const SizedBox(height: 6),
                TextField(
                  controller: _endpointController,
                  decoration: InputDecoration(
                    hintText: 'https://api.openai.com/v1',
                    contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
                    border: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: const BorderSide(color: Color(0xFFCBD5E1))),
                  ),
                ),
                const SizedBox(height: 14),

                const Text('API 密钥 (API Key)', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: Color(0xFF334155))),
                const SizedBox(height: 6),
                TextField(
                  controller: _apiKeyController,
                  obscureText: _obscureKey,
                  decoration: InputDecoration(
                    hintText: 'sk-...',
                    contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
                    border: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: const BorderSide(color: Color(0xFFCBD5E1))),
                    suffixIcon: IconButton(
                      icon: Icon(_obscureKey ? Icons.visibility_off : Icons.visibility, color: Colors.grey),
                      onPressed: () => setState(() => _obscureKey = !_obscureKey),
                    ),
                  ),
                ),
                const SizedBox(height: 14),

                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    const Text('翻译大模型 (Model)', style: TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: Color(0xFF334155))),
                    TextButton.icon(
                      onPressed: _fetchingModels ? null : _fetchModels,
                      icon: _fetchingModels
                          ? const SizedBox(width: 12, height: 12, child: CircularProgressIndicator(strokeWidth: 2))
                          : const Icon(Icons.refresh, size: 14),
                      label: const Text('获取可用模型', style: TextStyle(fontSize: 12)),
                    ),
                  ],
                ),
                const SizedBox(height: 4),
                Autocomplete<String>(
                  initialValue: TextEditingValue(text: _modelController.text),
                  optionsBuilder: (TextEditingValue textEditingValue) {
                    if (textEditingValue.text.isEmpty) return _models;
                    return _models.where((String option) {
                      return option.toLowerCase().contains(textEditingValue.text.toLowerCase());
                    });
                  },
                  onSelected: (String selection) {
                    _modelController.text = selection;
                  },
                  fieldViewBuilder: (context, controller, focusNode, onFieldSubmitted) {
                    if (controller.text.isEmpty && _modelController.text.isNotEmpty) {
                      controller.text = _modelController.text;
                    }
                    controller.addListener(() {
                      _modelController.text = controller.text;
                    });
                    return TextField(
                      controller: controller,
                      focusNode: focusNode,
                      decoration: InputDecoration(
                        hintText: 'gpt-4o-mini',
                        contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
                        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8), borderSide: const BorderSide(color: Color(0xFFCBD5E1))),
                      ),
                    );
                  },
                ),
                const SizedBox(height: 16),

                // Test Connection Bar
                Row(
                  children: [
                    OutlinedButton(
                      onPressed: _testing ? null : _testConnection,
                      style: OutlinedButton.styleFrom(
                        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                      ),
                      child: _testing
                          ? const SizedBox(width: 14, height: 14, child: CircularProgressIndicator(strokeWidth: 2))
                          : const Text('测试连接'),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: Text(
                        _testStatus,
                        style: TextStyle(fontSize: 12, color: _testStatusColor, fontWeight: FontWeight.w500),
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ],
                ),
              ],
            ),
          ),
          const SizedBox(height: 30),

          ElevatedButton(
            onPressed: _saveSettings,
            style: ElevatedButton.styleFrom(
              backgroundColor: const Color(0xFF2563EB),
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(vertical: 14),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
              elevation: 0,
            ),
            child: const Text('保存并应用', style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
          ),
        ],
      ),
    );
  }

  Widget _buildSectionHeader(String title) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, bottom: 8),
      child: Text(
        title,
        style: const TextStyle(fontSize: 13, fontWeight: FontWeight.bold, color: Color(0xFF64748B)),
      ),
    );
  }
}
