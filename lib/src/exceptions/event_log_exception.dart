/// Base exception class for event log errors.
class EventLogException implements Exception {
  /// Creates an [EventLogException] with the specified message and optional error code.
  const EventLogException(this.message, [this.code]);

  /// The error message
  final String message;

  /// Optional Windows error code
  final int? code;

  @override
  String toString() {
    if (code != null) {
      return 'EventLogException($code): $message';
    }
    return 'EventLogException: $message';
  }
}

/// Exception thrown when access to an event log channel is denied.
class AccessDeniedException extends EventLogException {
  /// Creates an [AccessDeniedException].
  const AccessDeniedException(super.message, [super.code]);
}

/// Exception thrown when a requested channel is not found.
class ChannelNotFoundException extends EventLogException {
  /// Creates a [ChannelNotFoundException].
  const ChannelNotFoundException(String channel)
    : super('Channel not found: $channel');
}

/// Exception thrown when an invalid query is provided.
class InvalidQueryException extends EventLogException {
  /// Creates an [InvalidQueryException].
  const InvalidQueryException(super.message);
}

/// Exception thrown when an event is not found.
class EventNotFoundException extends EventLogException {
  /// Creates an [EventNotFoundException].
  const EventNotFoundException(int eventRecordId)
    : super('Event record not found: $eventRecordId');
}

/// Exception thrown when a subscription fails.
class SubscriptionException extends EventLogException {
  /// Creates a [SubscriptionException].
  const SubscriptionException(super.message, [super.code]);
}
