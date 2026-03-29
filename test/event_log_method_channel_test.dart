import 'package:event_log/event_log_method_channel.dart';
import 'package:event_log/src/exceptions/event_log_exception.dart';
import 'package:event_log/src/models/channel_info.dart';
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
              return [
                {'name': 'System', 'enabled': true, 'type': 'Admin'},
                {'name': 'Application', 'enabled': true, 'type': 'Admin'},
                {'name': 'Security', 'enabled': false, 'type': 'Admin'},
              ];
            case 'queryEvents':
              return [];
            case 'getChannelInfo':
              final arguments = methodCall.arguments as Map<Object?, Object?>?;
              if (arguments?['channelName'] == 'Missing') {
                throw PlatformException(
                  code: 'CHANNEL_NOT_FOUND',
                  message: 'Channel not found',
                  details: {'channel': 'Missing'},
                );
              }
              if (arguments?['channelName'] == 'Unsupported') {
                throw PlatformException(
                  code: 'UNSUPPORTED_CHANNEL',
                  message: 'This channel does not support event queries.',
                  details: {
                    'channel': 'Unsupported',
                    'operation': 'query',
                    'errorCode': 50,
                  },
                );
              }
              return {'name': 'System', 'enabled': true, 'type': 'Admin'};
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
    expect(channels, const [
      ChannelInfo(name: 'System', enabled: true, type: 'Admin'),
      ChannelInfo(name: 'Application', enabled: true, type: 'Admin'),
      ChannelInfo(name: 'Security', enabled: false, type: 'Admin'),
    ]);
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

  test('maps platform exceptions to typed exceptions', () async {
    expect(
      () => platform.getChannelInfo('Missing'),
      throwsA(isA<ChannelNotFoundException>()),
    );
    expect(
      () => platform.getChannelInfo('Unsupported'),
      throwsA(isA<UnsupportedChannelException>()),
    );
  });
}
