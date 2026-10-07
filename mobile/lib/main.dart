import 'package:flutter/material.dart';
import 'app/app.dart';
import 'injection.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await configureDependencies();
  runApp(const WateringApp());
}
