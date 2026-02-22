import 'event_level.dart';

/// Represents a Windows Event Log record.
///
/// Contains all the system and event-specific data for a single event.
class EventRecord {
  /// Creates an [EventRecord] from the provided data.
  const EventRecord({
    required this.eventRecordId,
    required this.eventId,
    required this.level,
    required this.timeCreated,
    required this.channel,
    required this.computer,
    required this.providerName,
    this.providerGuid,
    this.task,
    this.opcode,
    this.keywords,
    this.qualifiers,
    this.processId,
    this.threadId,
    this.userId,
    this.activityId,
    this.relatedActivityId,
    this.version,
    this.message,
    this.xml,
    this.eventData,
  });

  /// Creates an [EventRecord] from a map (typically from platform channel).
  factory EventRecord.fromMap(Map<String, dynamic> map) {
    return EventRecord(
      eventRecordId: map['eventRecordId'] as int,
      eventId: map['eventId'] as int,
      level: EventLevel.fromValue(map['level'] as int? ?? 4),
      timeCreated: DateTime.parse(map['timeCreated'] as String),
      channel: map['channel'] as String,
      computer: map['computer'] as String,
      providerName: map['providerName'] as String,
      providerGuid: map['providerGuid'] as String?,
      task: map['task'] as int?,
      opcode: map['opcode'] as int?,
      keywords: map['keywords'] as int?,
      qualifiers: map['qualifiers'] as int?,
      processId: map['processId'] as int?,
      threadId: map['threadId'] as int?,
      userId: map['userId'] as String?,
      activityId: map['activityId'] as String?,
      relatedActivityId: map['relatedActivityId'] as String?,
      version: map['version'] as int?,
      message: map['message'] as String?,
      xml: map['xml'] as String?,
      eventData: map['eventData'] as Map<String, dynamic>?,
    );
  }

  /// Unique identifier for this event record (EventRecordID)
  final int eventRecordId;

  /// Event identifier (EventID)
  final int eventId;

  /// Severity level of the event
  final EventLevel level;

  /// Timestamp when the event was created
  final DateTime timeCreated;

  /// Channel where the event was logged (e.g., "System", "Application")
  final String channel;

  /// Computer name where the event was generated
  final String computer;

  /// Name of the event provider
  final String providerName;

  /// GUID of the event provider (optional)
  final String? providerGuid;

  /// Task category identifier (optional)
  final int? task;

  /// Operation code identifier (optional)
  final int? opcode;

  /// Keywords bitmask (optional)
  final int? keywords;

  /// Qualifiers for the event ID (optional)
  final int? qualifiers;

  /// Process ID that generated the event (optional)
  final int? processId;

  /// Thread ID that generated the event (optional)
  final int? threadId;

  /// User security identifier (optional)
  final String? userId;

  /// Activity ID for event correlation (optional)
  final String? activityId;

  /// Related activity ID for event correlation (optional)
  final String? relatedActivityId;

  /// Event schema version (optional)
  final int? version;

  /// Formatted message text (optional)
  final String? message;

  /// Complete event as XML (optional)
  final String? xml;

  /// Event-specific data as a map (optional)
  final Map<String, dynamic>? eventData;

  /// Converts this event record to a map.
  Map<String, dynamic> toMap() {
    return {
      'eventRecordId': eventRecordId,
      'eventId': eventId,
      'level': level.value,
      'timeCreated': timeCreated.toIso8601String(),
      'channel': channel,
      'computer': computer,
      'providerName': providerName,
      if (providerGuid != null) 'providerGuid': providerGuid,
      if (task != null) 'task': task,
      if (opcode != null) 'opcode': opcode,
      if (keywords != null) 'keywords': keywords,
      if (qualifiers != null) 'qualifiers': qualifiers,
      if (processId != null) 'processId': processId,
      if (threadId != null) 'threadId': threadId,
      if (userId != null) 'userId': userId,
      if (activityId != null) 'activityId': activityId,
      if (relatedActivityId != null) 'relatedActivityId': relatedActivityId,
      if (version != null) 'version': version,
      if (message != null) 'message': message,
      if (xml != null) 'xml': xml,
      if (eventData != null) 'eventData': eventData,
    };
  }

  @override
  String toString() {
    return 'EventRecord(id: $eventRecordId, eventId: $eventId, '
        'level: $level, channel: $channel, time: $timeCreated)';
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is EventRecord && other.eventRecordId == eventRecordId;
  }

  @override
  int get hashCode => eventRecordId.hashCode;
}
