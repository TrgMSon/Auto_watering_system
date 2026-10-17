import 'package:equatable/equatable.dart';
import '../../../auth/domain/entities/user.dart';

class ManagedUser extends Equatable {
  final int id;
  final String username;
  final String email;
  final String? fullName;
  final UserRole role;
  final bool isActive;
  final DateTime createdAt;

  const ManagedUser({
    required this.id,
    required this.username,
    required this.email,
    this.fullName,
    required this.role,
    required this.isActive,
    required this.createdAt,
  });

  @override
  List<Object?> get props => [id, username, email, fullName, role, isActive, createdAt];
}
