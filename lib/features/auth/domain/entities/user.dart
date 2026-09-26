import 'package:equatable/equatable.dart';

enum UserRole { admin, user }

class User extends Equatable {
  final int id;
  final String username;
  final String email;
  final String? fullName;
  final UserRole role;

  const User({
    required this.id,
    required this.username,
    required this.email,
    this.fullName,
    required this.role,
  });

  bool get isAdmin => role == UserRole.admin;

  @override
  List<Object?> get props => [id, username, email, fullName, role];
}
