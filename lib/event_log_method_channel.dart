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

  /// Broadcast stream controller for all events.
  StreamController<EventRecord>? _eventStreamController;

  final Map<String, StreamController<EventRecord>> _subscriptionControllers =
      {};

  int _globalStreamListeners = 0;

  /// Subscription to the event channel
  StreamSubscription<dynamic>? _eventChannelSubscription;

  @override
  Stream<EventRecord> getEventStream() {
    _eventStreamController ??= StreamController<EventRecord>.broadcast(
      onListen: () {
        _globalStreamListeners++;
        _ensureListening();
      },
      onCancel: () {
        if (_globalStreamListeners > 0) {
          _globalStreamListeners--;
        }
        _stopListeningIfIdle();
      },
    );
    return _eventStreamController!.stream;
  }

  @override
  Stream<EventRecord> getEventStreamForSubscription(String subscriptionId) {
    return _subscriptionControllers
        .putIfAbsent(
          subscriptionId,
          () => StreamController<EventRecord>.broadcast(
            onCancel: _stopListeningIfIdle,
          ),
        )
        .stream;
  }

  void _ensureListening() {
    _eventChannelSubscription ??= eventChannel.receiveBroadcastStream().listen(
      _handleEvent,
      onError: _handleError,
    );
  }

  void _stopListening() {
    _eventChannelSubscription?.cancel();
    _eventChannelSubscription = null;
  }

  void _stopListeningIfIdle() {
    if (_globalStreamListeners > 0 || _subscriptionControllers.isNotEmpty) {
      return;
    }
    _stopListening();
  }

  void _handleEvent(dynamic event) {
    if (event is! Map) {
      return;
    }

    try {
      final payload = Map<String, dynamic>.from(event);
      final envelope = _EventEnvelope.fromPayload(payload);

      if (envelope.error != null) {
        _emitError(envelope.subscriptionId, envelope.error!);
        return;
      }

      if (envelope.eventRecord == null) {
        return;
      }

      if (envelope.subscriptionId case final String subscriptionId) {
        _subscriptionControllers[subscriptionId]?.add(envelope.eventRecord!);
      }

      _eventStreamController?.add(envelope.eventRecord!);
    } catch (e) {
      debugPrint('Error parsing event: $e');
    }
  }

  void _handleError(dynamic error) {
    debugPrint('Event stream error: $error');
    final exception = EventLogException(error.toString());
    for (final controller in _subscriptionControllers.values) {
      controller.addError(exception);
    }
    _eventStreamController?.addError(exception);
  }

  void _emitError(String? subscriptionId, EventLogException error) {
    if (subscriptionId != null) {
      _subscriptionControllers[subscriptionId]?.addError(error);
    }
    _eventStreamController?.addError(error);
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

      _subscriptionControllers.putIfAbsent(
        result,
        () => StreamController<EventRecord>.broadcast(
          onCancel: _stopListeningIfIdle,
        ),
      );
      _ensureListening();

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
      await _subscriptionControllers.remove(subscriptionId)?.close();
      _stopListeningIfIdle();
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
        return ChannelNotFoundException(_parseChannelName(e.details, message));
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

  String _parseChannelName(dynamic details, String fallback) {
    if (details is Map && details['channel'] is String) {
      return details['channel'] as String;
    }
    return fallback;
  }
}

class _EventEnvelope {
  const _EventEnvelope({this.subscriptionId, this.eventRecord, this.error});

  factory _EventEnvelope.fromPayload(Map<String, dynamic> payload) {
    if (payload.containsKey('event') || payload.containsKey('error')) {
      final subscriptionId = payload['subscriptionId'] as String?;
      final eventPayload = payload['event'];
      final errorPayload = payload['error'];

      return _EventEnvelope(
        subscriptionId: subscriptionId,
        eventRecord: eventPayload is Map
            ? EventRecord.fromMap(Map<String, dynamic>.from(eventPayload))
            : null,
        error: errorPayload is Map
            ? _eventLogExceptionFromDetails(
                Map<String, dynamic>.from(errorPayload),
              )
            : null,
      );
    }

    return _EventEnvelope(eventRecord: EventRecord.fromMap(payload));
  }

  final String? subscriptionId;
  final EventRecord? eventRecord;
  final EventLogException? error;

  static EventLogException _eventLogExceptionFromDetails(
    Map<String, dynamic> payload,
  ) {
    final code = (payload['code'] as String?) ?? 'EXCEPTION';
    final message = (payload['message'] as String?) ?? 'Unknown error';

    switch (code) {
      case 'ACCESS_DENIED':
        return AccessDeniedException(message, _parseErrorCode(payload));
      case 'CHANNEL_NOT_FOUND':
        return ChannelNotFoundException(
          (payload['channel'] as String?) ?? message,
        );
      case 'INVALID_QUERY':
        return InvalidQueryException(message);
      case 'EVENT_NOT_FOUND':
        return EventNotFoundException(_parseEventId(payload));
      case 'SUBSCRIPTION_ERROR':
        return SubscriptionException(message, _parseErrorCode(payload));
      default:
        return EventLogException(message, _parseErrorCode(payload));
    }
  }

  static int? _parseErrorCode(Map<String, dynamic> payload) {
    final errorCode = payload['errorCode'];
    return errorCode is int ? errorCode : null;
  }

  static int _parseEventId(Map<String, dynamic> payload) {
    final eventRecordId = payload['eventRecordId'];
    return eventRecordId is int ? eventRecordId : 0;
  }
}
