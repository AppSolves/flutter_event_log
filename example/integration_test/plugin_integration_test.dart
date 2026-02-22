// This is a basic Flutter integration test.
//
// Since integration tests run in a full Flutter application, they can interact
// with the host side of a plugin implementation, unlike Dart unit tests.
//
// For more information about Flutter integration tests, please see
// https://flutter.dev/to/integration-testing

import 'package:event_log/event_log.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('listChannels test', (WidgetTester tester) async {
    // On Windows, there should always be some channels available
    final channels = await EventLog.listChannels();
    expect(channels.isNotEmpty, true);
    // System channel should always exist on Windows
    expect(channels.any((channel) => channel.name == 'System'), true);
  });

  testWidgets('queryEvents test', (WidgetTester tester) async {
    // Query the System channel with a limit
    final events = await EventLog.query(
      EventFilter(channel: 'System', maxEvents: 10),
    );
    // Should return a list (may be empty)
    expect(events, isA<List>());
  });

  testWidgets('getChannelInfo test', (WidgetTester tester) async {
    // Get info about the System channel
    final info = await EventLog.getChannelInfo('System');
    expect(info?.name, 'System');
    expect(info?.enabled, isA<bool>());
  });
}
