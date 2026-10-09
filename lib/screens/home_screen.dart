import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../core/controller_channel.dart';

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

  List<dynamic> _controllers = const [];
  bool _loadingControllers = false;
  String? _controllerError;

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

  void _showCoreSetup() {
    showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        backgroundColor: _surface,
        title: const Text('Emulator setup required'),
        content: const Text(
          'This preview does not include console core libraries or BIOS files, '
          'and it does not yet provide an in-app installer. The native loader '
          'expects compatible ARM64 cores in the app-private files/cores '
          'directory. Read SETUP.md for the current requirements and expected '
          'file names. Only use files you are legally entitled to use.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Got it'),
          ),
        ],
      ),
    );
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
            _buildSectionTitle('YOUR LIBRARY', 'Games will appear here'),
            const SizedBox(height: 11),
            _buildLibraryCard(),
            const SizedBox(height: 22),
            _buildSectionTitle('DEVICE', 'Input and compatibility'),
            const SizedBox(height: 11),
            _buildControllerCard(connectedCount),
            const SizedBox(height: 12),
            _buildStatusCard(
              icon: Icons.memory_rounded,
              title: 'Native engine',
              detail: 'ARM64 bridge included · Console cores not bundled',
              color: _cyan,
            ),
            const SizedBox(height: 22),
            _buildSectionTitle('GETTING STARTED', 'Before you can play'),
            const SizedBox(height: 11),
            _buildSetupCard(),
            const SizedBox(height: 20),
            const Center(
              child: Text(
                'Independent project · No games, cores, or BIOS files included',
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
            'YOUR NEXT\\nPLAY SESSION',
            style: TextStyle(
              fontSize: 26,
              height: 1.12,
              fontWeight: FontWeight.w800,
              letterSpacing: .4,
            ),
          ),
          const SizedBox(height: 10),
          const Text(
            'A first look at the PlayStation Go Android frontend.',
            style: TextStyle(color: Colors.white70, height: 1.4),
          ),
          const SizedBox(height: 20),
          SizedBox(
            height: 46,
            child: FilledButton.icon(
              onPressed: _showCoreSetup,
              icon: const Icon(Icons.info_outline_rounded, size: 18),
              label: const Text('View setup requirements'),
              style: FilledButton.styleFrom(
                backgroundColor: _blue,
                foregroundColor: Colors.white,
                shape: RoundedRectangleBorder(
                  borderRadius: BorderRadius.circular(13),
                ),
              ),
            ),
          ),
        ],
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

  Widget _buildLibraryCard() {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 24, horizontal: 18),
      decoration: _cardDecoration(),
      child: const Column(
        children: [
          Icon(Icons.library_add_rounded, color: Colors.white30, size: 34),
          SizedBox(height: 10),
          Text(
            'Your library is taking shape',
            style: TextStyle(fontSize: 15, fontWeight: FontWeight.w700),
          ),
          SizedBox(height: 5),
          Text(
            'Game browsing and launching are not available in this preview.',
            textAlign: TextAlign.center,
            style: TextStyle(color: Colors.white54, fontSize: 12, height: 1.4),
          ),
        ],
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
            title: 'Connect a controller',
            subtitle: 'Pair a Bluetooth gamepad or connect a USB controller.',
          ),
          const Divider(height: 24, color: Colors.white12),
          _buildSetupRow(
            number: '02',
            title: 'Prepare compatible files',
            subtitle:
                'Cores and BIOS are not supplied; see the setup guide for details.',
          ),
          const Divider(height: 24, color: Colors.white12),
          _buildSetupRow(
            number: '03',
            title: 'Wait for library features',
            subtitle:
                'Import, browse, and launch workflows are still in development.',
          ),
        ],
      ),
    );
  }

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
}
