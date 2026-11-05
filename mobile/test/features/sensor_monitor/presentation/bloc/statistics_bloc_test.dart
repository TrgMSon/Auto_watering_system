import 'dart:async';

import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/statistics_point.dart';
import 'package:watering_app/features/sensor_monitor/domain/repositories/sensor_repository.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/statistics_bloc.dart';

class MockSensorRepository extends Mock implements SensorRepository {}

void main() {
  late MockSensorRepository repository;
  final points = [StatisticsPoint(time: DateTime(2026, 11, 5, 10), soilMoisture: 40)];

  setUp(() {
    repository = MockSensorRepository();
  });

  blocTest<StatisticsBloc, StatisticsState>(
    'loads statistics for the chosen range',
    build: () {
      when(() => repository.getStatistics(deviceId: 'A', range: StatisticsRange.week))
          .thenAnswer((_) async => points);
      return StatisticsBloc(repository: repository);
    },
    act: (bloc) =>
        bloc.add(const StatisticsRequested(deviceId: 'A', range: StatisticsRange.week)),
    expect: () => [
      const StatisticsState(range: StatisticsRange.week, isLoading: true),
      StatisticsState(range: StatisticsRange.week, points: points),
    ],
  );

  blocTest<StatisticsBloc, StatisticsState>(
    'keeps the range and shows the error when loading fails',
    build: () {
      when(() => repository.getStatistics(deviceId: 'A', range: StatisticsRange.month))
          .thenThrow(Exception('lỗi'));
      return StatisticsBloc(repository: repository);
    },
    act: (bloc) =>
        bloc.add(const StatisticsRequested(deviceId: 'A', range: StatisticsRange.month)),
    expect: () => [
      const StatisticsState(range: StatisticsRange.month, isLoading: true),
      const StatisticsState(range: StatisticsRange.month, errorMessage: 'Exception: lỗi'),
    ],
  );

  test('ignores a slow response after reset', () async {
    final pending = Completer<List<StatisticsPoint>>();
    when(() => repository.getStatistics(deviceId: 'A', range: StatisticsRange.day))
        .thenAnswer((_) => pending.future);
    final bloc = StatisticsBloc(repository: repository);

    bloc.add(const StatisticsRequested(deviceId: 'A', range: StatisticsRange.day));
    await pumpEventQueue();
    bloc.add(const StatisticsResetRequested());
    await pumpEventQueue();
    pending.complete(points);
    await pumpEventQueue();

    expect(bloc.state, const StatisticsState());
    await bloc.close();
  });
}
