import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'event_log_platform_interface.dart';
import 'src/exceptions/event_log_exception.dart';
import 'src/models/channel_info.dart';
import 'src/models/event_filter.dart';
import 'src/models/event_record.dart';

/// An implementation of [EventLogPlatform] that uses method channels.
class MethodChannelEventLog extends EventLogPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('event_log');

  /// The event channel for receiving real-time events.
  @visibleForTesting
  final eventChannel = const EventChannel('event_log/events');

  /// Broadcast stream controller for events
  StreamController<EventRecord>? _eventStreamController;

  /// Subscription to the event channel
  StreamSubscription<dynamic>? _eventChannelSubscription;

  @override
  Stream<EventRecord> getEventStream() {
    _eventStreamController ??= StreamController<EventRecord>.broadcast(
      onListen: _startListening,
      onCancel: _stopListening,
    );
    return _eventStreamController!.stream;
  }

  void _startListening() {
    _eventChannelSubscription ??= eventChannel.receiveBroadcastStream().listen(
      _handleEvent,
      onError: _handleError,
    );
  }

  void _stopListening() {
    _eventChannelSubscription?.cancel();
    _eventChannelSubscription = null;
  }

  void _handleEvent(dynamic event) {
    if (event is Map) {
      try {
        final eventRecord = EventRecord.fromMap(
          Map<String, dynamic>.from(event),
        );
        _eventStreamController?.add(eventRecord);
      } catch (e) {
        debugPrint('Error parsing event: $e');
      }
    }
  }

  void _handleError(dynamic error) {
    debugPrint('Event stream error: $error');
    _eventStreamController?.addError(EventLogException(error.toString()));
  }

  @override
  Future<List<ChannelInfo>> listChannels() async {
    try {
      final result = await methodChannel.invokeMethod<List>('listChannels');
      if (result == null) return [];

      return result
          .cast<Map>()
          .map((map) => ChannelInfo.fromMap(Map<String, dynamic>.from(map)))
          .toList();
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<List<EventRecord>> queryEvents(EventFilter filter) async {
    try {
      final result = await methodChannel.invokeMethod<List>(
        'queryEvents',
        filter.toMap(),
      );
      if (result == null) return [];

      return result
          .cast<Map>()
          .map((map) => EventRecord.fromMap(Map<String, dynamic>.from(map)))
          .toList();
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<EventRecord?> getEventById(
    int eventRecordId, {
    String? channel,
  }) async {
    try {
      final result = await methodChannel.invokeMethod<Map>('getEventById', {
        'eventRecordId': eventRecordId,
        'channel': ?channel,
      });

      if (result == null) return null;
      return EventRecord.fromMap(Map<String, dynamic>.from(result));
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<String> subscribeToEvents(EventFilter filter) async {
    try {
      final result = await methodChannel.invokeMethod<String>(
        'subscribeToEvents',
        filter.toMap(),
      );

      if (result == null) {
        throw const SubscriptionException('Failed to create subscription');
      }

      return result;
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<void> unsubscribe(String subscriptionId) async {
    try {
      await methodChannel.invokeMethod<void>('unsubscribe', {
        'subscriptionId': subscriptionId,
      });
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<void> clearChannel(String channel, {String? backupPath}) async {
    try {
      await methodChannel.invokeMethod<void>('clearChannel', {
        'channel': channel,
        'backupPath': ?backupPath,
      });
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  @override
  Future<ChannelInfo?> getChannelInfo(String channelName) async {
    try {
      final result = await methodChannel.invokeMethod<Map>('getChannelInfo', {
        'channelName': channelName,
      });

      if (result == null) return null;
      return ChannelInfo.fromMap(Map<String, dynamic>.from(result));
    } on PlatformException catch (e) {
      throw _handlePlatformException(e);
    }
  }

  /// Converts a [PlatformException] to an appropriate [EventLogException].
  EventLogException _handlePlatformException(PlatformException e) {
    final code = e.code;
    final message = e.message ?? 'Unknown error';

    switch (code) {
      case 'ACCESS_DENIED':
        return AccessDeniedException(message, _parseErrorCode(e.details));
      case 'CHANNEL_NOT_FOUND':
        return ChannelNotFoundException(message);
      case 'INVALID_QUERY':
        return InvalidQueryException(message);
      case 'EVENT_NOT_FOUND':
        return EventNotFoundException(_parseEventId(e.details));
      case 'SUBSCRIPTION_ERROR':
        return SubscriptionException(message, _parseErrorCode(e.details));
      default:
        return EventLogException(message, _parseErrorCode(e.details));
    }
  }

  int? _parseErrorCode(dynamic details) {
    if (details is int) return details;
    if (details is Map && details['errorCode'] is int) {
      return details['errorCode'] as int;
    }
    return null;
  }

  int _parseEventId(dynamic details) {
    if (details is int) return details;
    if (details is Map && details['eventRecordId'] is int) {
      return details['eventRecordId'] as int;
    }
    return 0;
  }
}
