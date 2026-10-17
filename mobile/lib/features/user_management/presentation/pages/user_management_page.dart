import 'package:flutter/material.dart';
import '../../../../core/theme/app_colors.dart';

class UserManagementPage extends StatelessWidget {
  const UserManagementPage({super.key});

  @override
  Widget build(BuildContext context) {
    // TODO: Integrate with UserManagementBloc when ready
    return Scaffold(
      appBar: AppBar(title: const Text('Qu\u1ea3n l\u00fd ng\u01b0\u1eddi d\u00f9ng')),
      body: const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.group, size: 64, color: AppColors.textSecondary),
            SizedBox(height: 16),
            Text('T\u00ednh n\u0103ng qu\u1ea3n l\u00fd ng\u01b0\u1eddi d\u00f9ng'),
            Text('S\u1ebd \u0111\u01b0\u1ee3c k\u1ebft n\u1ed1i v\u1edbi BLoC'),
          ],
        ),
      ),
    );
  }
}
