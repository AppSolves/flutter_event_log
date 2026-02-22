import 'package:event_log/event_log_method_channel.dart';
import 'package:event_log/src/models/event_filter.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final MethodChannelEventLog platform = MethodChannelEventLog();
  const MethodChannel channel = MethodChannel('event_log');

  setUp(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
          switch (methodCall.method) {
            case 'listChannels':
              return ['System', 'Application', 'Security'];
            case 'queryEvents':
              return [];
            case 'subscribeToEvents':
              return 'subscription-123';
            case 'unsubscribe':
              return null;
            default:
              return null;
          }
        });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  test('listChannels', () async {
    final channels = await platform.listChannels();
    expect(channels, ['System', 'Application', 'Security']);
  });

  test('subscribeToEvents', () async {
    final subscriptionId = await platform.subscribeToEvents(
      const EventFilter(channel: 'System'),
    );
    expect(subscriptionId, 'subscription-123');
  });

  test('unsubscribe', () async {
    await platform.unsubscribe('subscription-123');
    // No exception means success
  });
}
