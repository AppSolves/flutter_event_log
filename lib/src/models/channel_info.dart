/// Represents information about a Windows Event Log channel.
///
/// A channel is a named stream of events (e.g., "System", "Application").
class ChannelInfo {
  /// Creates a [ChannelInfo] with the specified properties.
  const ChannelInfo({
    required this.name,
    required this.enabled,
    this.type,
    this.owningPublisher,
    this.isolation,
    this.logFilePath,
    this.maxSizeInBytes,
    this.eventCount,
  });

  /// Creates a [ChannelInfo] from a map.
  factory ChannelInfo.fromMap(Map<String, dynamic> map) {
    return ChannelInfo(
      name: map['name'] as String,
      enabled: map['enabled'] as bool,
      type: map['type'] as String?,
      owningPublisher: map['owningPublisher'] as String?,
      isolation: map['isolation'] as String?,
      logFilePath: map['logFilePath'] as String?,
      maxSizeInBytes: map['maxSizeInBytes'] as int?,
      eventCount: map['eventCount'] as int?,
    );
  }

  /// The name of the channel (e.g., "System", "Application")
  final String name;

  /// Whether the channel is enabled
  final bool enabled;

  /// Type of channel (e.g., "Admin", "Operational", "Analytic", "Debug")
  final String? type;

  /// Name of the publisher that owns this channel
  final String? owningPublisher;

  /// Isolation level of the channel
  final String? isolation;

  /// Path to the log file for this channel
  final String? logFilePath;

  /// Maximum size of the log file in bytes
  final int? maxSizeInBytes;

  /// Current number of events in the channel
  final int? eventCount;

  /// Converts this channel info to a map.
  Map<String, dynamic> toMap() {
    return {
      'name': name,
      'enabled': enabled,
      if (type != null) 'type': type,
      if (owningPublisher != null) 'owningPublisher': owningPublisher,
      if (isolation != null) 'isolation': isolation,
      if (logFilePath != null) 'logFilePath': logFilePath,
      if (maxSizeInBytes != null) 'maxSizeInBytes': maxSizeInBytes,
      if (eventCount != null) 'eventCount': eventCount,
    };
  }

  @override
  String toString() {
    return 'ChannelInfo(name: $name, enabled: $enabled, type: $type)';
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is ChannelInfo && other.name == name;
  }

  @override
  int get hashCode => name.hashCode;
}
