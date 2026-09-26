import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/network/api_client.dart';
import '../models/device_model.dart';

abstract class DeviceRemoteDataSource {
  Future<List<DeviceModel>> getDevices();
  Future<DeviceModel> getDeviceById(int id);
  Future<void> addDevice({required String name, required String type, String? location});
  Future<void> updateDevice({required int id, String? name, String? location});
  Future<void> deleteDevice(int id);
  Future<bool> toggleDevice({required int id, required bool activate});
}

class DeviceRemoteDataSourceImpl implements DeviceRemoteDataSource {
  final ApiClient apiClient;
  const DeviceRemoteDataSourceImpl({required this.apiClient});

  @override
  Future<List<DeviceModel>> getDevices() async {
    try {
      final response = await apiClient.dio.get(ApiEndpoints.devices);
      final List<dynamic> data = response.data as List<dynamic>;
      return data.map((j) => DeviceModel.fromJson(j as Map<String, dynamic>)).toList();
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 t\u1ea3i danh s\u00e1ch thi\u1ebft b\u1ecb: \$e');
    }
  }

  @override
  Future<DeviceModel> getDeviceById(int id) async {
    try {
      final response = await apiClient.dio.get('\${ApiEndpoints.devices}/\$id');
      return DeviceModel.fromJson(response.data);
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 t\u1ea3i th\u00f4ng tin thi\u1ebft b\u1ecb: \$e');
    }
  }

  @override
  Future<void> addDevice({required String name, required String type, String? location}) async {
    try {
      await apiClient.dio.post(ApiEndpoints.devices, data: {
        'name': name,
        'type': type,
        'location': location,
      });
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 th\u00eam thi\u1ebft b\u1ecb: \$e');
    }
  }

  @override
  Future<void> updateDevice({required int id, String? name, String? location}) async {
    try {
      await apiClient.dio.put('\${ApiEndpoints.devices}/\$id', data: {
        if (name != null) 'name': name,
        if (location != null) 'location': location,
      });
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 c\u1eadp nh\u1eadt thi\u1ebft b\u1ecb: \$e');
    }
  }

  @override
  Future<void> deleteDevice(int id) async {
    try {
      await apiClient.dio.delete('\${ApiEndpoints.devices}/\$id');
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 x\u00f3a thi\u1ebft b\u1ecb: \$e');
    }
  }

  @override
  Future<bool> toggleDevice({required int id, required bool activate}) async {
    try {
      final response = await apiClient.dio.post(
        '\${ApiEndpoints.devices}/\$id/toggle',
        data: {'active': activate},
      );
      return response.statusCode == 200;
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 \u0111i\u1ec1u khi\u1ec3n thi\u1ebft b\u1ecb: \$e');
    }
  }
}
