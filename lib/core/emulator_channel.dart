import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

class EmulatorChannel {
  EmulatorChannel._();

  static const String channelName = 'com.psg.emulator/core';
  static const MethodChannel _channel = MethodChannel(channelName);

  static Future<String?> loadCore(String consoleType) async {
    try {
      return await _channel.invokeMethod<String>(
        'loadCore',
        <String, Object>{'consoleType': consoleType},
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('loadCore failed: ${error.message}\n$stackTrace');
      rethrow;
    }
  }

  static Future<String?> importGame(
      String sourcePath, String consoleType) async {
    try {
      return await _channel.invokeMethod<String>(
        'importGame',
        <String, Object>{
          'sourcePath': sourcePath,
          'consoleType': consoleType,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('importGame failed: ${error.message}\n$stackTrace');
      rethrow;
    }
  }

  static Future<String?> installCore(
      String sourcePath, String consoleType) async {
    try {
      return await _channel.invokeMethod<String>(
        'installCore',
        <String, Object>{
          'sourcePath': sourcePath,
          'consoleType': consoleType,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('installCore failed: ${error.message}\n$stackTrace');
      rethrow;
    }
  }

  static Future<String?> installBios(
      String sourcePath, String consoleType) async {
    try {
      return await _channel.invokeMethod<String>(
        'installBios',
        <String, Object>{
          'sourcePath': sourcePath,
          'consoleType': consoleType,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('installBios failed: ${error.message}\n$stackTrace');
      rethrow;
    }
  }

  static Future<bool?> getSkipBios() async {
    try {
      return await _channel.invokeMethod<bool>('getSkipBios');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('getSkipBios failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> setSkipBios(bool skipBios) async {
    try {
      return await _channel.invokeMethod<bool>(
        'setSkipBios',
        <String, Object>{'skipBios': skipBios},
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('setSkipBios failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<String?> launchGame(
    String romPath,
    String consoleType,
  ) async {
    try {
      return await _channel.invokeMethod<String>(
        'launchGame',
        <String, Object>{
          'romPath': romPath,
          'consoleType': consoleType,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('launchGame failed: ${error.message}\n$stackTrace');
      rethrow;
    }
  }

  static Future<bool?> stopGame() async {
    try {
      return await _channel.invokeMethod<bool>('stopGame');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('stopGame failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> isGameRunning() async {
    try {
      return await _channel.invokeMethod<bool>('isGameRunning');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('isGameRunning failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> saveState(int slot) async {
    try {
      return await _channel.invokeMethod<bool>(
        'saveState',
        <String, Object>{'slot': slot},
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('saveState failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> loadState(int slot) async {
    try {
      return await _channel.invokeMethod<bool>(
        'loadState',
        <String, Object>{'slot': slot},
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('loadState failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<Map<dynamic, dynamic>?> getStats() async {
    try {
      return await _channel.invokeMapMethod<dynamic, dynamic>('getStats');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('getStats failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> applySettings(Map<dynamic, dynamic> settings) async {
    try {
      return await _channel.invokeMethod<bool>('applySettings', settings);
    } on PlatformException catch (error, stackTrace) {
      debugPrint('applySettings failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<String?> getDeviceTier() async {
    try {
      return await _channel.invokeMethod<String>('getDeviceTier');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('getDeviceTier failed: ${error.message}\n$stackTrace');
      return null;
    }
  }
}
