import 'package:flutter/material.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF070712),
      appBar: AppBar(
        title: const Text('PlayStation Go'),
      ),
      body: const Center(
        child: Text('PSG Core Engine Active — UI coming soon'),
      ),
    );
  }
}
