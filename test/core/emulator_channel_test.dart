import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:psg/core/emulator_channel.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  const channel = MethodChannel(EmulatorChannel.channelName);
  final calls = <MethodCall>[];

  setUp(() {
    calls.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (call) async {
      calls.add(call);
      switch (call.method) {
        case 'getSkipBios':
        case 'setSkipBios':
        case 'isGameRunning':
        case 'stopGame':
          return true;
        case 'loadCore':
        case 'launchGame':
          return 'OK';
        case 'importGame':
          return '/app/files/games/ps1/game.bin';
        case 'installCore':
          return '/app/files/cores/libpcsx_rearmed.so';
        case 'installBios':
          return '/app/files/bios/scph1001.bin';
        default:
          return null;
      }
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  test('forwards game, core, BIOS, launch, and runtime operations', () async {
    expect(
      await EmulatorChannel.importGame('/picker/game.bin', 'ps1'),
      '/app/files/games/ps1/game.bin',
    );
    expect(
      await EmulatorChannel.installCore('/picker/core.so', 'ps1'),
      '/app/files/cores/libpcsx_rearmed.so',
    );
    expect(
      await EmulatorChannel.installBios('/picker/bios.bin', 'ps1'),
      '/app/files/bios/scph1001.bin',
    );
    expect(await EmulatorChannel.setSkipBios(false), isTrue);
    expect(await EmulatorChannel.loadCore('ps1'), 'OK');
    expect(
      await EmulatorChannel.launchGame('/app/files/games/ps1/game.bin', 'ps1'),
      'OK',
    );
    expect(await EmulatorChannel.isGameRunning(), isTrue);
    expect(await EmulatorChannel.stopGame(), isTrue);

    expect(calls.map((call) => call.method), [
      'importGame',
      'installCore',
      'installBios',
      'setSkipBios',
      'loadCore',
      'launchGame',
      'isGameRunning',
      'stopGame',
    ]);
    expect(calls.first.arguments, {
      'sourcePath': '/picker/game.bin',
      'consoleType': 'ps1',
    });
    expect(calls[5].arguments, {
      'romPath': '/app/files/games/ps1/game.bin',
      'consoleType': 'ps1',
    });
  });
}
