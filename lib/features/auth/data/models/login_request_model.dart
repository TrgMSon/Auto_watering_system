class LoginRequestModel {
  final String username;
  final String password;

  const LoginRequestModel({required this.username, required this.password});

  Map<String, dynamic> toJson() => {
    'username': username,
    'password': password,
  };
}
