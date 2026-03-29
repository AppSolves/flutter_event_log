import 'dart:async';

import 'package:event_log/event_log.dart';
import 'package:event_log/event_log_method_channel.dart';
import 'package:event_log/event_log_platform_interface.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockEventLogPlatform
    with MockPlatformInterfaceMixin
    implements EventLogPlatform {
  final Map<String, StreamController<EventRecord>> _subscriptionControllers =
      {};
  int _nextSubscriptionId = 1;

  @override
  Future<List<ChannelInfo>> listChannels() => Future.value([
    const ChannelInfo(
      name: 'System',
      type: 'Admin',
      enabled: true,
      eventCount: 0,
    ),
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
  Future<EventRecord?> getEventById(int eventRecordId, {String? channel}) =>
      Future.value(null);

  @override
  Future<String> subscribeToEvents(EventFilter filter) {
    final subscriptionId = 'sub-${_nextSubscriptionId++}';
    _subscriptionControllers[subscriptionId] =
        StreamController<EventRecord>.broadcast();
    return Future.value(subscriptionId);
  }

  @override
  Future<void> unsubscribe(String subscriptionId) async {
    await _subscriptionControllers.remove(subscriptionId)?.close();
  }

  @override
  Stream<EventRecord> getEventStream() => const Stream.empty();

  @override
  Stream<EventRecord> getEventStreamForSubscription(String subscriptionId) {
    return _subscriptionControllers
        .putIfAbsent(
          subscriptionId,
          () => StreamController<EventRecord>.broadcast(),
        )
        .stream;
  }

  @override
  Future<void> clearChannel(String channel, {String? backupPath}) =>
      Future.value();

  @override
  Future<ChannelInfo?> getChannelInfo(String channelName) => Future.value(
    const ChannelInfo(
      name: 'System',
      type: 'Admin',
      enabled: true,
      eventCount: 0,
    ),
  );

  void emitEvent(String subscriptionId, EventRecord event) {
    _subscriptionControllers[subscriptionId]?.add(event);
  }
}

void main() {
  final EventLogPlatform initialPlatform = EventLogPlatform.instance;

  EventRecord buildEvent({required int recordId, required String channel}) {
    return EventRecord(
      eventRecordId: recordId,
      eventId: recordId,
      level: EventLevel.information,
      timeCreated: DateTime.utc(2026, 3, 30),
      channel: channel,
      computer: 'localhost',
      providerName: 'EventLogTest',
    );
  }

  tearDown(() {
    EventLogPlatform.instance = initialPlatform;
  });

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

  test('subscriptions stay isolated by subscription id', () async {
    final fakePlatform = MockEventLogPlatform();
    EventLogPlatform.instance = fakePlatform;

    final systemSubscription = await EventLog.subscribe(
      const EventFilter(channel: 'System'),
    );
    final applicationSubscription = await EventLog.subscribe(
      const EventFilter(channel: 'Application'),
    );

    final systemEvents = <EventRecord>[];
    final applicationEvents = <EventRecord>[];

    final systemListener = systemSubscription.listen(systemEvents.add);
    final applicationListener = applicationSubscription.listen(
      applicationEvents.add,
    );

    fakePlatform.emitEvent(
      systemSubscription.id,
      buildEvent(recordId: 1, channel: 'System'),
    );
    fakePlatform.emitEvent(
      applicationSubscription.id,
      buildEvent(recordId: 2, channel: 'Application'),
    );

    await Future<void>.delayed(Duration.zero);

    expect(systemEvents.map((event) => event.channel), ['System']);
    expect(applicationEvents.map((event) => event.channel), ['Application']);

    await systemListener.cancel();
    await applicationListener.cancel();
    await systemSubscription.cancel();
    await applicationSubscription.cancel();
  });
}
