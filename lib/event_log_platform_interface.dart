import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'event_log_method_channel.dart';
import 'src/models/channel_info.dart';
import 'src/models/event_filter.dart';
import 'src/models/event_record.dart';

/// The interface that platform-specific implementations of event_log must extend.
///
/// Platform implementations should extend this class rather than implement it as `event_log`
/// does not consider newly added methods to be breaking changes. Extending this class
/// (using `extends`) ensures that the subclass will get the default implementation, while
/// platform implementations that `implements` this interface will be broken by newly added
/// [EventLogPlatform] methods.
abstract class EventLogPlatform extends PlatformInterface {
  /// Constructs a EventLogPlatform.
  EventLogPlatform() : super(token: _token);

  static final Object _token = Object();

  static EventLogPlatform _instance = MethodChannelEventLog();

  /// The default instance of [EventLogPlatform] to use.
  ///
  /// Defaults to [MethodChannelEventLog].
  static EventLogPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [EventLogPlatform] when
  /// they register themselves.
  static set instance(EventLogPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  /// Lists all available event log channels on the system.
  ///
  /// Returns a list of [ChannelInfo] objects containing information about
  /// each channel (e.g., "System", "Application", "Security").
  ///
  /// Throws [EventLogException] if the operation fails.
  Future<List<ChannelInfo>> listChannels() {
    throw UnimplementedError('listChannels() has not been implemented.');
  }

  /// Queries historical events from the event log.
  ///
  /// [filter] specifies the criteria for selecting events (channel, time range,
  /// event IDs, levels, etc.).
  ///
  /// Returns a list of [EventRecord] objects matching the filter criteria.
  ///
  /// Throws [EventLogException] if the query fails.
  Future<List<EventRecord>> queryEvents(EventFilter filter) {
    throw UnimplementedError('queryEvents() has not been implemented.');
  }

  /// Retrieves a single event by its record ID.
  ///
  /// [eventRecordId] is the unique identifier for the event.
  /// [channel] is the optional channel name where the event is located.
  /// If [channel] is not specified, searches all channels.
  ///
  /// Returns the [EventRecord] if found, null otherwise.
  ///
  /// Throws [EventLogException] if the operation fails.
  Future<EventRecord?> getEventById(int eventRecordId, {String? channel}) {
    throw UnimplementedError('getEventById() has not been implemented.');
  }

  /// Subscribes to real-time events from the event log.
  ///
  /// [filter] specifies the criteria for events to receive.
  ///
  /// Returns a unique subscription ID that can be used to cancel the subscription.
  ///
  /// Use [getEventStream] to receive the events.
  ///
  /// Throws [EventLogException] if the subscription fails.
  Future<String> subscribeToEvents(EventFilter filter) {
    throw UnimplementedError('subscribeToEvents() has not been implemented.');
  }

  /// Unsubscribes from a real-time event subscription.
  ///
  /// [subscriptionId] is the ID returned by [subscribeToEvents].
  ///
  /// Throws [EventLogException] if the operation fails.
  Future<void> unsubscribe(String subscriptionId) {
    throw UnimplementedError('unsubscribe() has not been implemented.');
  }

  /// Gets the stream of events for real-time subscriptions.
  ///
  /// Returns a broadcast stream that emits [EventRecord] objects as they occur.
  ///
  /// Events are only sent to this stream after calling [subscribeToEvents].
  Stream<EventRecord> getEventStream() {
    throw UnimplementedError('getEventStream() has not been implemented.');
  }

  /// Gets the stream of events for a specific subscription.
  ///
  /// Platform implementations can override this to route events per
  /// subscription. The default implementation falls back to the shared stream.
  Stream<EventRecord> getEventStreamForSubscription(String subscriptionId) {
    return getEventStream();
  }

  /// Clears all events from the specified channel.
  ///
  /// [channel] is the name of the channel to clear.
  /// [backupPath] is an optional path to save the events before clearing.
  ///
  /// Throws [EventLogException] if the operation fails.
  Future<void> clearChannel(String channel, {String? backupPath}) {
    throw UnimplementedError('clearChannel() has not been implemented.');
  }

  /// Gets information about a specific channel.
  ///
  /// [channelName] is the name of the channel.
  ///
  /// Returns [ChannelInfo] if the channel exists, null otherwise.
  ///
  /// Throws [EventLogException] if the operation fails.
  Future<ChannelInfo?> getChannelInfo(String channelName) {
    throw UnimplementedError('getChannelInfo() has not been implemented.');
  }
}
