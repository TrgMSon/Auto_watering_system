import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class AppTypography {
  AppTypography._();

  static TextTheme get textTheme => TextTheme(
    headlineLarge: GoogleFonts.inter(
      fontSize: 32,
      fontWeight: FontWeight.w700,
      letterSpacing: -0.5,
    ),
    headlineMedium: GoogleFonts.inter(
      fontSize: 24,
      fontWeight: FontWeight.w600,
    ),
    headlineSmall: GoogleFonts.inter(
      fontSize: 20,
      fontWeight: FontWeight.w600,
    ),
    titleLarge: GoogleFonts.inter(
      fontSize: 18,
      fontWeight: FontWeight.w600,
    ),
    titleMedium: GoogleFonts.inter(
      fontSize: 16,
      fontWeight: FontWeight.w500,
    ),
    titleSmall: GoogleFonts.inter(
      fontSize: 14,
      fontWeight: FontWeight.w500,
    ),
    bodyLarge: GoogleFonts.inter(
      fontSize: 16,
      fontWeight: FontWeight.w400,
    ),
    bodyMedium: GoogleFonts.inter(
      fontSize: 14,
      fontWeight: FontWeight.w400,
    ),
    bodySmall: GoogleFonts.inter(
      fontSize: 12,
      fontWeight: FontWeight.w400,
    ),
    labelLarge: GoogleFonts.inter(
      fontSize: 14,
      fontWeight: FontWeight.w600,
      letterSpacing: 0.5,
    ),
    labelMedium: GoogleFonts.inter(
      fontSize: 12,
      fontWeight: FontWeight.w500,
    ),
    labelSmall: GoogleFonts.inter(
      fontSize: 10,
      fontWeight: FontWeight.w500,
      letterSpacing: 0.5,
    ),
  );
}
