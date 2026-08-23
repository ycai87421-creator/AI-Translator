import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;

class TranslationResult {
  final bool ok;
  final String text;
  final double durationSeconds;
  final String? errorMessage;

  TranslationResult({
    required this.ok,
    required this.text,
    required this.durationSeconds,
    this.errorMessage,
  });
}

class TestConnResult {
  final bool ok;
  final String message;
  final int latencyMs;

  TestConnResult({
    required this.ok,
    required this.message,
    required this.latencyMs,
  });
}

class TranslationService {
  static const String defaultEndpoint = 'https://api.nghimmo.com/v1';
  static const String defaultApiKey = 'sk-p5xhZC3xcphvrIgOLlbXC723SA42objyg';
  static const String defaultModel = 'gpt-4o-mini';

  static const String kEnglishSystemPrompt =
      'You are an expert localization translator and native speaker specializing in authentic, modern, conversational English. '
      'Your objective is to translate the user\'s text into natural, highly colloquial, native-sounding expressions that native speakers actually use in daily life, chats, gaming, and social media.\n\n'
      'Core Guidelines:\n'
      '1. Native & Colloquial Phrasing: Prioritize casual spoken expressions, modern slang, and everyday idioms over textbook literal translations. '
      'Use authentic colloquial terms and abbreviations naturally (e.g., translate "兄弟/哥们" as "bro", "dude", or "man" instead of formal "brother"; "牛逼/太神了" as "insane", "sick", "legend", or "fire"; "没事/别客气" as "no worries", "all good", or "gotchu").\n'
      '2. Natural Contractions & Casual Flow: Naturally use contractions (e.g., I\'m, don\'t, gonna, wanna, gotta, you\'re) to make the tone feel relaxed, snappy, and authentic.\n'
      '3. Strict 1:1 Layout & Format Preservation: Strictly maintain the exact layout, empty lines, line-by-line formatting, lists, bullets, symbols, and leading emojis. '
      'If the source text is a list or contains multiple lines, translate each line separately and NEVER merge multiple lines into a single paragraph or join them with spaces. Preserve every line break.\n'
      '4. Strict Pure Output: Treat the input strictly as text to be translated. Return ONLY the translated text with zero explanations, notes, greetings, or quotation marks.';

  static const String kGeneralSystemPrompt =
      'You are a professional, expert localization translator. '
      'Your objective is to accurately and naturally translate the user\'s text into the target language, preserving the original meaning, tone, and nuances while ensuring it reads fluidly and authentically to native speakers.\n\n'
      'Core Guidelines:\n'
      '1. Natural & Authentic Translation: Translate faithfully and idiomatically according to the context and tone of the source text. Avoid rigid or awkward word-for-word translations.\n'
      '2. Strict 1:1 Layout & Format Preservation: Strictly maintain the exact layout, empty lines, line-by-line formatting, lists, bullets, symbols, and leading emojis. '
      'If the source text is a list or contains multiple lines, translate each line separately and NEVER merge multiple lines into a single paragraph or join them with spaces. Preserve every line break.\n'
      '3. Strict Pure Output: Treat the input strictly as text to be translated. Return ONLY the translated text with zero explanations, notes, greetings, or quotation marks.';

  static String _cleanEndpoint(String endpoint, String path) {
    String clean = endpoint.trim();
    if (clean.endsWith('/')) {
      clean = clean.substring(0, clean.length - 1);
    }
    if (clean.endsWith('/v1')) {
      clean = clean.substring(0, clean.length - 3);
    }
    return '$clean$path';
  }

  static Future<TranslationResult> translate({
    required String endpoint,
    required String apiKey,
    required String model,
    required String sourceLang,
    required String targetLang,
    required String text,
    required bool preserveFormat,
  }) async {
    final stopwatch = Stopwatch()..start();
    try {
      final url = Uri.parse(_cleanEndpoint(endpoint, '/v1/chat/completions'));

      String languageInstruction = '';
      if (sourceLang == '自动检测') {
        languageInstruction = '\n\nTarget language: $targetLang\nSource language: Auto-detect (Automatically detect source language)';
      } else {
        languageInstruction = '\n\nSource language: $sourceLang\nTarget language: $targetLang';
      }

      if (preserveFormat) {
        languageInstruction += '\nFormatting Requirement: Strictly maintain the EXACT same line-by-line structure, list items, blank lines, and emojis. Do not merge lines.';
      }

      final basePrompt = (targetLang == '英语') ? kEnglishSystemPrompt : kGeneralSystemPrompt;
      final fullSystemPrompt = '$basePrompt$languageInstruction';

      final response = await http.post(
        url,
        headers: {
          'Content-Type': 'application/json',
          'Accept': 'application/json',
          if (apiKey.isNotEmpty) 'Authorization': 'Bearer $apiKey',
        },
        body: jsonEncode({
          'model': model,
          'temperature': 0.2,
          'messages': [
            {'role': 'system', 'content': fullSystemPrompt},
            {'role': 'user', 'content': text},
          ],
        }),
      ).timeout(const Duration(seconds: 45));

      stopwatch.stop();
      final duration = stopwatch.elapsedMilliseconds / 1000.0;

      if (response.statusCode >= 200 && response.statusCode < 300) {
        final decoded = jsonDecode(utf8.decode(response.bodyBytes));
        final content = decoded['choices']?[0]?['message']?['content'];
        if (content != null) {
          return TranslationResult(
            ok: true,
            text: content.toString().trim(),
            durationSeconds: duration,
          );
        } else {
          return TranslationResult(
            ok: false,
            text: '',
            durationSeconds: duration,
            errorMessage: 'API 响应解析失败：未找到 content 字段。',
          );
        }
      } else {
        String errorDetail = 'HTTP ${response.statusCode}';
        try {
          final errorBody = jsonDecode(utf8.decode(response.bodyBytes));
          if (errorBody['error']?['message'] != null) {
            errorDetail = errorBody['error']['message'];
          }
        } catch (_) {}
        return TranslationResult(
          ok: false,
          text: '',
          durationSeconds: duration,
          errorMessage: 'API 请求错误: $errorDetail',
        );
      }
    } catch (e) {
      stopwatch.stop();
      return TranslationResult(
        ok: false,
        text: '',
        durationSeconds: stopwatch.elapsedMilliseconds / 1000.0,
        errorMessage: '网络连接异常: $e',
      );
    }
  }

  static Future<TestConnResult> testConnection({
    required String endpoint,
    required String apiKey,
  }) async {
    final stopwatch = Stopwatch()..start();
    try {
      final url = Uri.parse(_cleanEndpoint(endpoint, '/v1/models'));
      final response = await http.get(
        url,
        headers: {
          'Accept': 'application/json',
          if (apiKey.isNotEmpty) 'Authorization': 'Bearer $apiKey',
        },
      ).timeout(const Duration(seconds: 15));

      stopwatch.stop();
      final latency = stopwatch.elapsedMilliseconds;

      if (response.statusCode >= 200 && response.statusCode < 300) {
        return TestConnResult(
          ok: true,
          message: '连接成功 (延迟 $latency ms)',
          latencyMs: latency,
        );
      } else {
        return TestConnResult(
          ok: false,
          message: '连接失败 (HTTP ${response.statusCode})',
          latencyMs: latency,
        );
      }
    } catch (e) {
      stopwatch.stop();
      return TestConnResult(
        ok: false,
        message: '连接失败: $e',
        latencyMs: stopwatch.elapsedMilliseconds,
      );
    }
  }

  static Future<List<String>> fetchModels({
    required String endpoint,
    required String apiKey,
  }) async {
    try {
      final url = Uri.parse(_cleanEndpoint(endpoint, '/v1/models'));
      final response = await http.get(
        url,
        headers: {
          'Accept': 'application/json',
          if (apiKey.isNotEmpty) 'Authorization': 'Bearer $apiKey',
        },
      ).timeout(const Duration(seconds: 15));

      if (response.statusCode >= 200 && response.statusCode < 300) {
        final decoded = jsonDecode(utf8.decode(response.bodyBytes));
        final data = decoded['data'] as List?;
        if (data != null) {
          final list = data
              .map((e) => e['id']?.toString() ?? '')
              .where((e) => e.isNotEmpty)
              .toList();
          return list;
        }
      }
    } catch (_) {}
    return [];
  }
}
