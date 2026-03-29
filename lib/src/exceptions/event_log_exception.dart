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

/// Exception thrown when a channel does not support the requested operation.
class UnsupportedChannelException extends EventLogException {
  /// Creates an [UnsupportedChannelException].
  UnsupportedChannelException(this.channel, {this.operation, int? code})
    : super(
        operation == null || operation.isEmpty
            ? 'Channel does not support this operation: $channel'
            : 'Channel does not support $operation: $channel',
        code,
      );

  /// The channel that rejected the operation.
  final String channel;

  /// The unsupported operation, such as `query` or `subscribe`.
  final String? operation;
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
