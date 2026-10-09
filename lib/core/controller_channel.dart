import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

class ControllerChannel {
  ControllerChannel._();

  static const String channelName = 'com.psg.emulator/controller';
  static const MethodChannel _channel = MethodChannel(channelName);

  static Future<List<dynamic>?> getControllers() async {
    try {
      return await _channel.invokeListMethod<dynamic>('getControllers');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('getControllers failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> sendVirtualButton(int buttonId, bool pressed) async {
    try {
      return await _channel.invokeMethod<bool>(
        'virtualButton',
        <String, Object>{
          'buttonId': buttonId,
          'pressed': pressed,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('sendVirtualButton failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> sendVirtualStick(
    int stickId,
    double x,
    double y,
  ) async {
    try {
      return await _channel.invokeMethod<bool>(
        'virtualStick',
        <String, Object>{
          'stickId': stickId,
          'x': x,
          'y': y,
        },
      );
    } on PlatformException catch (error, stackTrace) {
      debugPrint('sendVirtualStick failed: ${error.message}\n$stackTrace');
      return null;
    }
  }
}
