import '../../../auth/domain/entities/user.dart';
import '../../domain/entities/managed_user.dart';

class UserAdminModel extends ManagedUser {
  const UserAdminModel({
    required super.id,
    required super.username,
    required super.email,
    super.fullName,
    required super.role,
    required super.isActive,
    required super.createdAt,
  });

  factory UserAdminModel.fromJson(Map<String, dynamic> json) {
    return UserAdminModel(
      id: json['id'] as int,
      username: json['username'] as String,
      email: json['email'] as String,
      fullName: json['fullName'] as String?,
      role: json['role'] == 'ADMIN' ? UserRole.admin : UserRole.user,
      isActive: json['active'] as bool? ?? true,
      createdAt: DateTime.parse(json['createdAt'] as String),
    );
  }
}
