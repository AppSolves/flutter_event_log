import 'package:event_log/event_log.dart';
import 'package:event_log/event_log_method_channel.dart';
import 'package:event_log/event_log_platform_interface.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockEventLogPlatform
    with MockPlatformInterfaceMixin
    implements EventLogPlatform {
  @override
  Future<List<ChannelInfo>> listChannels() => Future.value([
    const ChannelInfo(name: 'System', type: 'Admin', enabled: true, eventCount: 0),
    const ChannelInfo(
      name: 'Application',
      type: 'Admin',
      enabled: true,
      eventCount: 0,
    ),
  ]);

  @override
  Future<List<EventRecord>> queryEvents(EventFilter filter) => Future.value([]);

  @override
  Future<EventRecord> getEventById(int eventRecordId, {String? channel}) =>
      Future.error(UnimplementedError());

  @override
  Future<String> subscribeToEvents(EventFilter filter) =>
      Future.value('sub-123');

  @override
  Future<void> unsubscribe(String subscriptionId) => Future.value();

  @override
  Stream<EventRecord> getEventStream() => const Stream.empty();

  @override
  Future<void> clearChannel(String channelName, {String? backupPath}) =>
      Future.value();

  @override
  Future<ChannelInfo> getChannelInfo(String channelName) => Future.value(
    const ChannelInfo(name: 'System', type: 'Admin', enabled: true, eventCount: 0),
  );
}

void main() {
  final EventLogPlatform initialPlatform = EventLogPlatform.instance;

  test('$MethodChannelEventLog is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelEventLog>());
  });

  test('listChannels', () async {
    final MockEventLogPlatform fakePlatform = MockEventLogPlatform();
    EventLogPlatform.instance = fakePlatform;

    final channels = await EventLog.listChannels();
    expect(channels.length, 2);
    expect(channels[0].name, 'System');
    expect(channels[1].name, 'Application');
  });

  test('subscribeToEvents returns subscription', () async {
    final MockEventLogPlatform fakePlatform = MockEventLogPlatform();
    EventLogPlatform.instance = fakePlatform;

    final subscription = await EventLog.subscribe(
      const EventFilter(channel: 'System'),
    );
    expect(subscription, isNotNull);
  });
}
