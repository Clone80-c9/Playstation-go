import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

class ThermalChannel {
  ThermalChannel._() {
    _channel.setMethodCallHandler(_handleMethodCall);
  }

  static const String channelName = 'com.psg.emulator/thermal';
  static final ThermalChannel instance = ThermalChannel._();
  final MethodChannel _channel = const MethodChannel(channelName);
  final StreamController<dynamic> _thermalController =
      StreamController<dynamic>.broadcast();

  static Stream<dynamic> get thermalStream =>
      instance._thermalController.stream;

  static Future<bool?> startMonitoring() async {
    try {
      return await instance._channel.invokeMethod<bool>('startMonitoring');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('startMonitoring failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<bool?> stopMonitoring() async {
    try {
      return await instance._channel.invokeMethod<bool>('stopMonitoring');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('stopMonitoring failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  static Future<double?> getTemperature() async {
    try {
      return await instance._channel.invokeMethod<double>('getTemperature');
    } on PlatformException catch (error, stackTrace) {
      debugPrint('getTemperature failed: ${error.message}\n$stackTrace');
      return null;
    }
  }

  Future<Object?> _handleMethodCall(MethodCall call) async {
    if (call.method == 'onThermalChange') {
      _thermalController.add(call.arguments);
      return null;
    }
    debugPrint('Ignoring unknown thermal callback: ${call.method}');
    return null;
  }
}
