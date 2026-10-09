import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

import 'screens/splash_screen.dart';

class PSGApp extends StatelessWidget {
  const PSGApp({super.key});

  @override
  Widget build(BuildContext context) {
    final baseTheme = ThemeData.dark();

    return MaterialApp(
      title: 'PlayStation Go',
      debugShowCheckedModeBanner: false,
      theme: baseTheme.copyWith(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF070712),
        primaryColor: const Color(0xFF0070D1),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF0070D1),
          secondary: Color(0xFF00C8FF),
          surface: Color(0xFF0D1020),
        ),
        textTheme: GoogleFonts.interTextTheme(baseTheme.textTheme),
      ),
      home: const SplashScreen(),
    );
  }
}
