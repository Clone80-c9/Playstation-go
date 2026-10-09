import 'dart:convert';
import 'dart:async';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../core/controller_channel.dart';
import '../core/emulator_channel.dart';

class _GameEntry {
  const _GameEntry({
    required this.name,
    required this.path,
    required this.console,
  });

  final String name;
  final String path;
  final String console;

  Map<String, String> toJson() => {
        'name': name,
        'path': path,
        'console': console,
      };

  factory _GameEntry.fromJson(Map<String, dynamic> json) => _GameEntry(
        name: json['name'] as String,
        path: json['path'] as String,
        console: json['console'] as String,
      );
}

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  static const Color _background = Color(0xFF070712);
  static const Color _surface = Color(0xFF111321);
  static const Color _blue = Color(0xFF0070D1);
  static const Color _cyan = Color(0xFF00C8FF);
  static const List<String> _consoles = ['ps1', 'ps2', 'ps3', 'ps4'];
  static const List<String> _romExtensions = [
    'bin',
    'cue',
    'chd',
    'pbp',
    'iso',
    'img',
    'cso',
    'rom',
    'nrg',
    'mdf',
  ];

  List<_GameEntry> _games = const [];
  List<dynamic> _controllers = const [];
  bool _loadingControllers = false;
  bool _importing = false;
  bool _skipBios = true;
  String? _controllerError;

  @override
  void initState() {
    super.initState();
    _loadLibrary();
    _loadSkipBiosSetting();
    _refreshControllers();
  }

  Future<void> _loadSkipBiosSetting() async {
    final skipBios = await EmulatorChannel.getSkipBios();
    if (mounted && skipBios != null) {
      setState(() => _skipBios = skipBios);
    }
  }

  Future<void> _setSkipBios(bool skipBios) async {
    final saved = await EmulatorChannel.setSkipBios(skipBios);
    if (!mounted) return;
    if (saved == true) {
      setState(() => _skipBios = skipBios);
    } else {
      _showMessage('Could not save the BIOS setting.');
    }
  }

  Future<void> _loadLibrary() async {
    final preferences = await SharedPreferences.getInstance();
    final encoded = preferences.getString(_libraryPreferenceKey);
    if (encoded == null || !mounted) return;
    try {
      final games = (jsonDecode(encoded) as List<dynamic>)
          .map((entry) => _GameEntry.fromJson(entry as Map<String, dynamic>))
          .toList();
      setState(() => _games = games);
    } on FormatException {
      _showMessage('Saved game library could not be read.');
    } on TypeError {
      _showMessage('Saved game library has an invalid entry.');
    }
  }

  Future<void> _saveLibrary() async {
    final preferences = await SharedPreferences.getInstance();
    await preferences.setString(
      _libraryPreferenceKey,
      jsonEncode(_games.map((game) => game.toJson()).toList()),
    );
  }

  Future<void> _refreshControllers() async {
    setState(() {
      _loadingControllers = true;
      _controllerError = null;
    });

    try {
      final controllers = await ControllerChannel.getControllers();
      if (!mounted) return;
      setState(() {
        _controllers = controllers ?? const [];
        _controllerError = controllers == null
            ? 'Controller discovery is unavailable on this device.'
            : null;
        _loadingControllers = false;
      });
    } on MissingPluginException {
      if (!mounted) return;
      setState(() {
        _controllerError = 'Controller discovery is only available on Android.';
        _loadingControllers = false;
      });
    } on PlatformException catch (error) {
      if (!mounted) return;
      setState(() {
        _controllerError =
            error.message ?? 'Could not check for connected controllers.';
        _loadingControllers = false;
      });
    }
  }

  Future<String?> _chooseConsole() => showModalBottomSheet<String>(
        context: context,
        backgroundColor: _surface,
        builder: (context) => SafeArea(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Padding(
                padding: EdgeInsets.all(18),
                child: Text(
                  'Choose the game system',
                  style: TextStyle(fontSize: 17, fontWeight: FontWeight.w700),
                ),
              ),
              for (final console in _consoles)
                ListTile(
                  leading:
                      const Icon(Icons.videogame_asset_rounded, color: _cyan),
                  title: Text(console.toUpperCase()),
                  subtitle: console == 'ps3' || console == 'ps4'
                      ? const Text('Experimental core support')
                      : null,
                  onTap: () => Navigator.of(context).pop(console),
                ),
            ],
          ),
        ),
      );

  Future<void> _importGame() async {
    final console = await _chooseConsole();
    if (console == null || !mounted) return;

    try {
      final selection = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: _romExtensions,
        allowMultiple: false,
      );
      final sourcePath = selection?.files.single.path;
      if (sourcePath == null) {
        _showMessage('The selected game file could not be accessed.');
        return;
      }

      setState(() => _importing = true);
      final importedPath =
          await EmulatorChannel.importGame(sourcePath, console);
      if (!mounted) return;
      if (importedPath == null || importedPath.isEmpty) {
        _showMessage(
          'Game import failed. Check that the file is readable and try again.',
        );
        return;
      }

      final imported = _GameEntry(
        name: sourcePath.split('/').last,
        path: importedPath,
        console: console,
      );
      setState(() => _games = [..._games, imported]);
      await _saveLibrary();
      _showMessage('${imported.name} added to your library.');
    } on PlatformException catch (error) {
      _showMessage(error.message ?? 'Game import failed.');
    } finally {
      if (mounted) setState(() => _importing = false);
    }
  }

  Future<void> _installCore() async {
    final console = await _chooseConsole();
    if (console == null || !mounted) return;
    try {
      final selection = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: const ['so'],
        allowMultiple: false,
      );
      final sourcePath = selection?.files.single.path;
      if (sourcePath == null) {
        _showMessage('The selected core file could not be accessed.');
        return;
      }
      setState(() => _importing = true);
      final installedPath =
          await EmulatorChannel.installCore(sourcePath, console);
      if (!mounted) return;
      _showMessage(
        installedPath == null
            ? 'Core installation failed. The file may not match this system.'
            : '${console.toUpperCase()} core installed. Its native ABI is checked when loaded.',
      );
    } on PlatformException catch (error) {
      _showMessage(error.message ?? 'Core installation failed.');
    } finally {
      if (mounted) setState(() => _importing = false);
    }
  }

  Future<void> _installBios() async {
    final console = await _chooseConsole();
    if (console == null || !mounted) return;
    try {
      final selection = await FilePicker.platform.pickFiles(
        type: FileType.custom,
        allowedExtensions: const ['bin', 'rom'],
        allowMultiple: false,
      );
      final sourcePath = selection?.files.single.path;
      if (sourcePath == null) {
        _showMessage('The selected BIOS file could not be accessed.');
        return;
      }
      setState(() => _importing = true);
      final installedPath =
          await EmulatorChannel.installBios(sourcePath, console);
      if (!mounted) return;
      _showMessage(
        installedPath == null
            ? 'BIOS installation failed.'
            : '${console.toUpperCase()} BIOS imported to app storage.',
      );
    } on PlatformException catch (error) {
      _showMessage(error.message ?? 'BIOS installation failed.');
    } finally {
      if (mounted) setState(() => _importing = false);
    }
  }

  Future<void> _playGame(_GameEntry game) async {
    await Navigator.of(context).push<void>(
      MaterialPageRoute<void>(
        builder: (context) => _GamePlayerScreen(game: game),
      ),
    );
  }

  void _showMessage(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    final connectedCount = _controllers.length;

    return Scaffold(
      backgroundColor: _background,
      appBar: AppBar(
        backgroundColor: _background,
        titleSpacing: 20,
        title: const Row(
          children: [
            Icon(Icons.sports_esports_rounded, color: _cyan, size: 25),
            SizedBox(width: 10),
            Text(
              'PlayStation Go',
              style: TextStyle(fontWeight: FontWeight.w700, letterSpacing: .2),
            ),
          ],
        ),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 18),
            child: Center(
              child: Container(
                padding:
                    const EdgeInsets.symmetric(horizontal: 11, vertical: 6),
                decoration: BoxDecoration(
                  color: _blue.withOpacity(.16),
                  borderRadius: BorderRadius.circular(30),
                  border: Border.all(color: _blue.withOpacity(.45)),
                ),
                child: const Text(
                  'PREVIEW',
                  style: TextStyle(
                    color: _cyan,
                    fontSize: 10,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 1.2,
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
      body: SafeArea(
        top: false,
        child: ListView(
          padding: const EdgeInsets.fromLTRB(20, 14, 20, 32),
          children: [
            _buildWelcomeCard(),
            const SizedBox(height: 22),
            _buildSectionTitle('YOUR LIBRARY', '${_games.length} games'),
            const SizedBox(height: 11),
            if (_games.isEmpty)
              _buildLibraryEmpty()
            else
              ..._games.map(_buildGameTile),
            const SizedBox(height: 22),
            _buildSectionTitle('DEVICE', 'Input and compatibility'),
            const SizedBox(height: 11),
            _buildControllerCard(connectedCount),
            const SizedBox(height: 12),
            _buildStatusCard(
              icon: Icons.memory_rounded,
              title: 'Native engine',
              detail:
                  'ARM64 bridge · Requires a compatible core for each system',
              color: _cyan,
            ),
            const SizedBox(height: 22),
            _buildSectionTitle('GETTING STARTED', 'Use your own files'),
            const SizedBox(height: 11),
            _buildSetupCard(),
            const SizedBox(height: 12),
            _buildBiosSetting(),
            const SizedBox(height: 20),
            const Center(
              child: Text(
                'No games, console cores, or BIOS files are distributed with this app',
                textAlign: TextAlign.center,
                style: TextStyle(color: Colors.white38, fontSize: 11),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildWelcomeCard() {
    return Container(
      padding: const EdgeInsets.all(22),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(22),
        gradient: const LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0xFF14284A), Color(0xFF10111F), Color(0xFF1A1020)],
        ),
        border: Border.all(color: Colors.white.withOpacity(.08)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Text(
            'YOUR NEXT\nPLAY SESSION',
            style: TextStyle(
              fontSize: 26,
              height: 1.12,
              fontWeight: FontWeight.w800,
              letterSpacing: .4,
            ),
          ),
          const SizedBox(height: 10),
          const Text(
            'Import your own game files and a compatible ARM64 core to try the preview.',
            style: TextStyle(color: Colors.white70, height: 1.4),
          ),
          const SizedBox(height: 18),
          Wrap(
            spacing: 9,
            runSpacing: 9,
            children: [
              _actionButton(
                icon: Icons.add_rounded,
                label: 'Import game',
                onPressed: _importing ? null : _importGame,
                primary: true,
              ),
              _actionButton(
                icon: Icons.memory_rounded,
                label: 'Install core',
                onPressed: _importing ? null : _installCore,
              ),
              _actionButton(
                icon: Icons.verified_user_outlined,
                label: 'Import BIOS',
                onPressed: _importing ? null : _installBios,
              ),
            ],
          ),
          if (_importing) ...[
            const SizedBox(height: 15),
            const LinearProgressIndicator(minHeight: 2),
            const SizedBox(height: 5),
            const Text(
              'Copying file into private app storage…',
              style: TextStyle(color: Colors.white60, fontSize: 11),
            ),
          ],
        ],
      ),
    );
  }

  Widget _actionButton({
    required IconData icon,
    required String label,
    required VoidCallback? onPressed,
    bool primary = false,
  }) {
    return FilledButton.tonalIcon(
      onPressed: onPressed,
      icon: Icon(icon, size: 17),
      label: Text(label),
      style: FilledButton.styleFrom(
        backgroundColor: primary ? _blue : Colors.white.withOpacity(.09),
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(horizontal: 13, vertical: 12),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      ),
    );
  }

  Widget _buildSectionTitle(String title, String subtitle) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.end,
      children: [
        Text(
          title,
          style: const TextStyle(
            fontSize: 12,
            fontWeight: FontWeight.w800,
            letterSpacing: 1.3,
          ),
        ),
        const Spacer(),
        Text(
          subtitle,
          style: const TextStyle(color: Colors.white38, fontSize: 11),
        ),
      ],
    );
  }

  Widget _buildLibraryEmpty() {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 24, horizontal: 18),
      decoration: _cardDecoration(),
      child: const Column(
        children: [
          Icon(Icons.library_add_rounded, color: Colors.white30, size: 34),
          SizedBox(height: 10),
          Text(
            'Your library is empty',
            style: TextStyle(fontSize: 15, fontWeight: FontWeight.w700),
          ),
          SizedBox(height: 5),
          Text(
            'Import a game file you are legally entitled to use.',
            textAlign: TextAlign.center,
            style: TextStyle(color: Colors.white54, fontSize: 12, height: 1.4),
          ),
        ],
      ),
    );
  }

  Widget _buildGameTile(_GameEntry game) {
    return Container(
      margin: const EdgeInsets.only(bottom: 10),
      decoration: _cardDecoration(),
      child: ListTile(
        leading: const CircleAvatar(
          backgroundColor: Color(0xFF17294B),
          child: Icon(Icons.sports_esports_rounded, color: _cyan),
        ),
        title: Text(
          game.name,
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(fontWeight: FontWeight.w700),
        ),
        subtitle: Text(
          game.console.toUpperCase(),
          style: const TextStyle(color: Colors.white54, fontSize: 11),
        ),
        trailing: IconButton(
          tooltip: 'Try to launch ${game.name}',
          onPressed: () => _playGame(game),
          icon: const Icon(Icons.play_circle_fill_rounded, color: _cyan),
          iconSize: 29,
        ),
      ),
    );
  }

  Widget _buildControllerCard(int connectedCount) {
    final status = _controllerError ??
        (connectedCount == 0
            ? 'No controllers detected'
            : '$connectedCount controller${connectedCount == 1 ? '' : 's'} connected');
    final statusColor = _controllerError == null && connectedCount > 0
        ? const Color(0xFF65D99A)
        : Colors.white60;

    return Container(
      padding: const EdgeInsets.all(16),
      decoration: _cardDecoration(),
      child: Row(
        children: [
          Container(
            width: 42,
            height: 42,
            decoration: BoxDecoration(
              color: _blue.withOpacity(.16),
              borderRadius: BorderRadius.circular(13),
            ),
            child: const Icon(Icons.gamepad_rounded, color: _cyan),
          ),
          const SizedBox(width: 13),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Controller support',
                  style: TextStyle(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 4),
                Text(
                  status,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: TextStyle(color: statusColor, fontSize: 12),
                ),
              ],
            ),
          ),
          const SizedBox(width: 8),
          IconButton(
            tooltip: 'Refresh connected controllers',
            onPressed: _loadingControllers ? null : _refreshControllers,
            icon: _loadingControllers
                ? const SizedBox(
                    width: 19,
                    height: 19,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.refresh_rounded),
          ),
        ],
      ),
    );
  }

  Widget _buildStatusCard({
    required IconData icon,
    required String title,
    required String detail,
    required Color color,
  }) {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: _cardDecoration(),
      child: Row(
        children: [
          Icon(icon, color: color, size: 23),
          const SizedBox(width: 13),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title,
                    style: const TextStyle(fontWeight: FontWeight.w700)),
                const SizedBox(height: 4),
                Text(
                  detail,
                  style: const TextStyle(color: Colors.white54, fontSize: 12),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSetupCard() {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: _cardDecoration(),
      child: Column(
        children: [
          _buildSetupRow(
            number: '01',
            title: 'Import a game file',
            subtitle:
                'Choose a system and select a local ROM, image, or supported archive file.',
          ),
          const Divider(height: 24, color: Colors.white12),
          _buildSetupRow(
            number: '02',
            title: 'Install a matching ARM64 core',
            subtitle:
                'The core must export the PSG native ABI; a regular ROM file is not an emulator.',
          ),
          const Divider(height: 24, color: Colors.white12),
          _buildSetupRow(
            number: '03',
            title: 'Add a BIOS if your core requires one',
            subtitle:
                'Use only files you are legally entitled to use. Core availability and compatibility vary.',
          ),
        ],
      ),
    );
  }

  Widget _buildBiosSetting() => Container(
        decoration: _cardDecoration(),
        child: SwitchListTile(
          value: _skipBios,
          onChanged: _setSkipBios,
          title: const Text(
            'Skip BIOS',
            style: TextStyle(fontWeight: FontWeight.w700),
          ),
          subtitle: const Text(
            'Turn off only when you have installed a compatible BIOS file.',
            style: TextStyle(color: Colors.white54, fontSize: 12),
          ),
          activeColor: _cyan,
        ),
      );

  Widget _buildSetupRow({
    required String number,
    required String title,
    required String subtitle,
  }) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          number,
          style: const TextStyle(
            color: _cyan,
            fontWeight: FontWeight.w800,
            fontSize: 12,
          ),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: const TextStyle(fontWeight: FontWeight.w700)),
              const SizedBox(height: 4),
              Text(
                subtitle,
                style: const TextStyle(
                  color: Colors.white54,
                  fontSize: 12,
                  height: 1.35,
                ),
              ),
            ],
          ),
        ),
      ],
    );
  }

  BoxDecoration _cardDecoration() => BoxDecoration(
        color: _surface,
        borderRadius: BorderRadius.circular(17),
        border: Border.all(color: Colors.white.withOpacity(.07)),
      );

  static const String _libraryPreferenceKey = 'imported_games_v1';
}

class _GamePlayerScreen extends StatefulWidget {
  const _GamePlayerScreen({required this.game});

  final _GameEntry game;

  @override
  State<_GamePlayerScreen> createState() => _GamePlayerScreenState();
}

class _GamePlayerScreenState extends State<_GamePlayerScreen> {
  static const Color _cyan = Color(0xFF00C8FF);
  static const MethodChannel _surfaceChannel =
      MethodChannel('com.psg.emulator/surface');
  bool _surfaceReady = false;
  bool _starting = false;
  bool _running = false;
  String? _surfaceError;
  Timer? _statusTimer;

  @override
  void initState() {
    super.initState();
    _surfaceChannel.setMethodCallHandler(_handleSurfaceCall);
  }

  Future<Object?> _handleSurfaceCall(MethodCall call) async {
    if (call.method != 'surfaceChanged') return null;
    final arguments = call.arguments;
    if (arguments is! Map) return null;
    if (!mounted) return null;
    setState(() {
      _surfaceReady = arguments['ready'] == true;
      _surfaceError = arguments['message'] as String?;
      if (!_surfaceReady) _running = false;
    });
    return null;
  }

  Future<void> _startGame() async {
    if (!_surfaceReady || _starting) return;
    setState(() {
      _starting = true;
      _surfaceError = null;
    });
    try {
      final loaded = await EmulatorChannel.loadCore(widget.game.console);
      if (loaded != 'OK') {
        _showError(
          'Could not load a compatible ${widget.game.console.toUpperCase()} core. '
          'Install an ARM64 core that exports the PSG core ABI.',
        );
        return;
      }
      final launched = await EmulatorChannel.launchGame(
        widget.game.path,
        widget.game.console,
      );
      if (launched != 'OK') {
        _showError(
          'Game launch failed. Check core compatibility, BIOS requirements, and Logcat.',
        );
        return;
      }
      if (mounted) {
        setState(() => _running = true);
        _statusTimer?.cancel();
        _statusTimer = Timer.periodic(
          const Duration(seconds: 1),
          (_) => _checkGameStatus(),
        );
      }
    } on PlatformException catch (error) {
      _showError(error.message ?? 'Unable to start the selected game.');
      await EmulatorChannel.stopGame();
    } finally {
      if (mounted) setState(() => _starting = false);
    }
  }

  Future<void> _stopGame() async {
    _statusTimer?.cancel();
    await EmulatorChannel.stopGame();
    if (mounted) setState(() => _running = false);
  }

  Future<void> _checkGameStatus() async {
    final isRunning = await EmulatorChannel.isGameRunning();
    if (mounted && _running && isRunning == false) {
      _statusTimer?.cancel();
      setState(() {
        _running = false;
        _surfaceError =
            'The core stopped. Review its compatibility and device logs.';
      });
    }
  }

  void _showError(String message) {
    if (!mounted) return;
    setState(() => _surfaceError = message);
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  void dispose() {
    _statusTimer?.cancel();
    _surfaceChannel.setMethodCallHandler(null);
    EmulatorChannel.stopGame();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
        backgroundColor: const Color(0xFF070712),
        appBar: AppBar(
          backgroundColor: const Color(0xFF070712),
          title: Text(widget.game.name,
              maxLines: 1, overflow: TextOverflow.ellipsis),
        ),
        body: SafeArea(
          child: Column(
            children: [
              Expanded(
                child: Container(
                  color: Colors.black,
                  width: double.infinity,
                  child: AndroidView(
                    viewType: 'com.psg.app/game_surface',
                  ),
                ),
              ),
              _buildTouchControls(),
              if (_surfaceError != null)
                Padding(
                  padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
                  child: Text(
                    _surfaceError!,
                    textAlign: TextAlign.center,
                    style: const TextStyle(color: Colors.orangeAccent),
                  ),
                ),
              Padding(
                padding: const EdgeInsets.all(16),
                child: Row(
                  children: [
                    Expanded(
                      child: Text(
                        _running
                            ? 'Running · ${widget.game.console.toUpperCase()}'
                            : _surfaceReady
                                ? 'Surface ready · ${widget.game.console.toUpperCase()}'
                                : 'Preparing game surface…',
                        style: const TextStyle(color: Colors.white70),
                      ),
                    ),
                    if (_running)
                      OutlinedButton.icon(
                        onPressed: _stopGame,
                        icon: const Icon(Icons.stop_rounded),
                        label: const Text('Stop'),
                      )
                    else
                      FilledButton.icon(
                        onPressed:
                            _surfaceReady && !_starting ? _startGame : null,
                        icon: _starting
                            ? const SizedBox(
                                width: 16,
                                height: 16,
                                child:
                                    CircularProgressIndicator(strokeWidth: 2),
                              )
                            : const Icon(Icons.play_arrow_rounded),
                        label: Text(_starting ? 'Starting' : 'Start'),
                      ),
                  ],
                ),
              ),
            ],
          ),
        ),
      );

  Widget _buildTouchControls() => Padding(
        padding: const EdgeInsets.fromLTRB(14, 10, 14, 0),
        child: Column(
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Row(
                      children: [
                        _touchButton(4, 'L1', compact: true),
                        const SizedBox(width: 6),
                        _touchButton(6, 'L2', compact: true),
                        const SizedBox(width: 6),
                        _touchButton(7, 'R2', compact: true),
                        const SizedBox(width: 6),
                        _touchButton(5, 'R1', compact: true),
                      ],
                    ),
                    const SizedBox(height: 6),
                    _buildDPad(),
                    const SizedBox(height: 5),
                    Row(
                      children: [
                        _touchButton(13, 'SELECT', compact: true),
                        const SizedBox(width: 6),
                        _touchButton(12, 'START', compact: true),
                      ],
                    ),
                  ],
                ),
                _buildFaceButtons(),
              ],
            ),
            const SizedBox(height: 4),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceEvenly,
              children: [
                _buildVirtualStick(16, 'L STICK'),
                _buildVirtualStick(17, 'R STICK'),
              ],
            ),
          ],
        ),
      );

  Widget _buildDPad() => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _touchButton(8, '▲'),
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              _touchButton(10, '◀'),
              const SizedBox(width: 4),
              _touchButton(11, '▶'),
            ],
          ),
          _touchButton(9, '▼'),
        ],
      );

  Widget _buildFaceButtons() => Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          _touchButton(3, '△'),
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              _touchButton(2, '□'),
              const SizedBox(width: 8),
              _touchButton(1, '○'),
            ],
          ),
          _touchButton(0, '×'),
        ],
      );

  Widget _touchButton(int id, String label, {bool compact = false}) =>
      GestureDetector(
        onTapDown: (_) => ControllerChannel.sendVirtualButton(id, true),
        onTapUp: (_) => ControllerChannel.sendVirtualButton(id, false),
        onTapCancel: () => ControllerChannel.sendVirtualButton(id, false),
        child: Container(
          width: compact ? 47 : 43,
          height: compact ? 34 : 39,
          alignment: Alignment.center,
          decoration: BoxDecoration(
            color: const Color(0xFF19223A),
            borderRadius: BorderRadius.circular(compact ? 10 : 20),
            border: Border.all(color: const Color(0xFF344361)),
          ),
          child: Text(
            label,
            style: TextStyle(
              color: _cyan,
              fontSize: compact ? 9 : 16,
              fontWeight: FontWeight.w800,
            ),
          ),
        ),
      );

  Widget _buildVirtualStick(int stickId, String label) {
    const diameter = 64.0;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        GestureDetector(
          onPanStart: (details) =>
              _sendStickPosition(stickId, details.localPosition, diameter),
          onPanUpdate: (details) =>
              _sendStickPosition(stickId, details.localPosition, diameter),
          onPanEnd: (_) => ControllerChannel.sendVirtualStick(stickId, 0, 0),
          onPanCancel: () => ControllerChannel.sendVirtualStick(stickId, 0, 0),
          child: Container(
            width: diameter,
            height: diameter,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: const Color(0xFF19223A),
              border: Border.all(color: const Color(0xFF344361)),
            ),
            child: const Icon(
              Icons.control_camera_rounded,
              color: Colors.white54,
              size: 29,
            ),
          ),
        ),
        const SizedBox(height: 2),
        Text(
          label,
          style: const TextStyle(
            color: Colors.white54,
            fontSize: 8,
            letterSpacing: .8,
          ),
        ),
      ],
    );
  }

  void _sendStickPosition(int stickId, Offset position, double diameter) {
    final radius = diameter / 2;
    final x = ((position.dx - radius) / radius).clamp(-1.0, 1.0).toDouble();
    final y = ((position.dy - radius) / radius).clamp(-1.0, 1.0).toDouble();
    ControllerChannel.sendVirtualStick(stickId, x, y);
  }
}
