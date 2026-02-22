/// A Flutter plugin for accessing the Windows Event Log.
///
/// This plugin provides a comprehensive API for interacting with the Windows Event Viewer,
/// allowing you to:
/// - Subscribe to real-time events
/// - Query historical events
/// - Retrieve events by ID
/// - List available channels
/// - Filter events by various criteria
///
/// Example:
/// ```dart
/// // List all channels
/// final channels = await EventLog.listChannels();
///
/// // Subscribe to System events
/// final subscription = await EventLog.subscribe(
///   const EventFilter(channel: 'System'),
/// );
/// subscription.listen((event) {
///   print('Event: ${event.eventId} - ${event.message}');
/// });
///
/// // Query historical events
/// final events = await EventLog.query(
///   EventFilter(
///     channel: 'Application',
///     levels: [EventLevel.error, EventLevel.critical],
///     maxEvents: 100,
///   ),
/// );
/// ```
library;

import 'dart:async';

import 'event_log_platform_interface.dart';
import 'src/models/channel_info.dart';
import 'src/models/event_filter.dart';
import 'src/models/event_record.dart';

export 'src/exceptions/event_log_exception.dart';
// Export public API
export 'src/models/channel_info.dart';
export 'src/models/event_filter.dart';
export 'src/models/event_level.dart';
export 'src/models/event_record.dart';

/// Main class for accessing Windows Event Log functionality.
///
/// Provides static methods for all event log operations.
class EventLog {
  EventLog._(); // Private constructor to prevent instantiation

  static final Map<String, StreamSubscription<EventRecord>> _subscriptions = {};

  /// Lists all available event log channels on the system.
  ///
  /// Returns a list of [ChannelInfo] objects containing information about
  /// each channel (e.g., "System", "Application", "Security").
  ///
  /// Example:
  /// ```dart
  /// final channels = await EventLog.listChannels();
  /// for (final channel in channels) {
  ///   print('${channel.name}: ${channel.enabled}');
  /// }
  /// ```
  ///
  /// Throws [EventLogException] if the operation fails.
  static Future<List<ChannelInfo>> listChannels() {
    return EventLogPlatform.instance.listChannels();
  }

  /// Queries historical events from the event log.
  ///
  /// [filter] specifies the criteria for selecting events. You can filter by:
  /// - Channel name
  /// - Event IDs
  /// - Severity levels
  /// - Time range
  /// - Provider names
  /// - Custom XPath queries
  ///
  /// Example:
  /// ```dart
  /// final events = await EventLog.query(
  ///   EventFilter(
  ///     channel: 'System',
  ///     levels: [EventLevel.error, EventLevel.critical],
  ///     startTime: DateTime.now().subtract(const Duration(hours: 24)),
  ///     maxEvents: 50,
  ///   ),
  /// );
  /// ```
  ///
  /// Returns a list of [EventRecord] objects matching the filter criteria.
  ///
  /// Throws [EventLogException] if the query fails.
  static Future<List<EventRecord>> query(EventFilter filter) {
    return EventLogPlatform.instance.queryEvents(filter);
  }

  /// Retrieves a single event by its record ID.
  ///
  /// [eventRecordId] is the unique identifier for the event.
  /// [channel] is the optional channel name where the event is located.
  /// If [channel] is not specified, searches all channels (slower).
  ///
  /// Example:
  /// ```dart
  /// final event = await EventLog.getById(12345, channel: 'System');
  /// if (event != null) {
  ///   print('Found event: ${event.message}');
  /// }
  /// ```
  ///
  /// Returns the [EventRecord] if found, null otherwise.
  ///
  /// Throws [EventLogException] if the operation fails.
  static Future<EventRecord?> getById(int eventRecordId, {String? channel}) {
    return EventLogPlatform.instance.getEventById(
      eventRecordId,
      channel: channel,
    );
  }

  /// Subscribes to real-time events from the event log.
  ///
  /// [filter] specifies the criteria for events to receive. Events matching
  /// the filter will be streamed as they occur.
  ///
  /// Returns a [Stream] of [EventRecord] objects that can be listened to.
  ///
  /// Example:
  /// ```dart
  /// final subscription = await EventLog.subscribe(
  ///   const EventFilter(
  ///     channel: 'Application',
  ///     levels: [EventLevel.warning, EventLevel.error],
  ///   ),
  /// );
  ///
  /// subscription.listen(
  ///   (event) => print('Event: ${event.eventId}'),
  ///   onError: (error) => print('Error: $error'),
  /// );
  ///
  /// // Later: cancel the subscription
  /// await EventLog.unsubscribe(subscriptionId);
  /// ```
  ///
  /// Throws [SubscriptionException] if the subscription fails.
  static Future<EventLogSubscription> subscribe(EventFilter filter) async {
    final subscriptionId = await EventLogPlatform.instance.subscribeToEvents(
      filter,
    );

    final stream = EventLogPlatform.instance.getEventStream();

    return EventLogSubscription._(subscriptionId, stream, filter);
  }

  /// Unsubscribes from a real-time event subscription.
  ///
  /// [subscriptionId] is the ID returned by [subscribe].
  ///
  /// Example:
  /// ```dart
  /// await EventLog.unsubscribe('subscription-id-123');
  /// ```
  ///
  /// Throws [EventLogException] if the operation fails.
  static Future<void> unsubscribe(String subscriptionId) async {
    await EventLogPlatform.instance.unsubscribe(subscriptionId);
    _subscriptions.remove(subscriptionId)?.cancel();
  }

  /// Clears all events from the specified channel.
  ///
  /// [channel] is the name of the channel to clear.
  /// [backupPath] is an optional path to save the events before clearing.
  ///
  /// **Warning:** This operation requires administrator privileges and
  /// will permanently delete events unless a backup path is specified.
  ///
  /// Example:
  /// ```dart
  /// await EventLog.clear(
  ///   'Application',
  ///   backupPath: r'C:\Logs\app_backup.evtx',
  /// );
  /// ```
  ///
  /// Throws [AccessDeniedException] if insufficient permissions.
  /// Throws [ChannelNotFoundException] if the channel doesn't exist.
  static Future<void> clear(String channel, {String? backupPath}) {
    return EventLogPlatform.instance.clearChannel(
      channel,
      backupPath: backupPath,
    );
  }

  /// Gets information about a specific channel.
  ///
  /// [channelName] is the name of the channel.
  ///
  /// Example:
  /// ```dart
  /// final info = await EventLog.getChannelInfo('System');
  /// if (info != null) {
  ///   print('Enabled: ${info.enabled}');
  ///   print('Events: ${info.eventCount}');
  /// }
  /// ```
  ///
  /// Returns [ChannelInfo] if the channel exists, null otherwise.
  ///
  /// Throws [EventLogException] if the operation fails.
  static Future<ChannelInfo?> getChannelInfo(String channelName) {
    return EventLogPlatform.instance.getChannelInfo(channelName);
  }
}

/// Represents an active event log subscription.
///
/// This class wraps a subscription ID and provides access to the event stream.
class EventLogSubscription {
  EventLogSubscription._(this.id, this._stream, this.filter);

  /// The unique identifier for this subscription.
  final String id;

  /// The filter used for this subscription.
  final EventFilter filter;

  final Stream<EventRecord> _stream;
  StreamSubscription<EventRecord>? _subscription;

  /// Listens to events from this subscription.
  ///
  /// [onData] is called for each event received.
  /// [onError] is called if an error occurs.
  /// [onDone] is called when the subscription ends.
  /// [cancelOnError] determines whether to cancel on first error.
  StreamSubscription<EventRecord> listen(
    void Function(EventRecord event)? onData, {
    Function? onError,
    void Function()? onDone,
    bool? cancelOnError,
  }) {
    _subscription = _stream.listen(
      onData,
      onError: onError,
      onDone: onDone,
      cancelOnError: cancelOnError,
    );
    return _subscription!;
  }

  /// Cancels this subscription.
  ///
  /// After cancellation, no more events will be received.
  Future<void> cancel() async {
    await _subscription?.cancel();
    await EventLog.unsubscribe(id);
  }
}
