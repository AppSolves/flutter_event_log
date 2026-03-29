import 'event_level.dart';

/// Options for filtering and querying Windows Event Log events.
///
/// Provides a fluent API for building event queries with various criteria.
class EventFilter {
  /// Creates an [EventFilter] with the specified options.
  const EventFilter({
    this.channel,
    this.eventIds,
    this.levels,
    this.providers,
    this.startTime,
    this.endTime,
    this.keywords,
    this.xpathQuery,
    this.maxEvents,
    this.reverse = false,
  });

  /// The channel to query (e.g., "System", "Application", "Security")
  /// If null, queries all channels
  final String? channel;

  /// List of specific event IDs to include
  final List<int>? eventIds;

  /// List of event levels to include
  final List<EventLevel>? levels;

  /// List of provider names to include
  final List<String>? providers;

  /// Start time for the time range filter
  final DateTime? startTime;

  /// End time for the time range filter
  final DateTime? endTime;

  /// Keywords bitmask to filter by
  final int? keywords;

  /// Custom XPath query string (advanced filtering)
  /// If provided, other filters are ignored
  final String? xpathQuery;

  /// Maximum number of events to retrieve
  final int? maxEvents;

  /// Whether to retrieve events in reverse chronological order.
  ///
  /// For Admin and Operational channels, the native query runs newest-first.
  /// For Analytic and Debug channels, Windows only allows forward reads, so
  /// the plugin emulates reverse order in memory instead of throwing.
  final bool reverse;

  /// Creates a copy with the specified fields replaced.
  EventFilter copyWith({
    String? channel,
    List<int>? eventIds,
    List<EventLevel>? levels,
    List<String>? providers,
    DateTime? startTime,
    DateTime? endTime,
    int? keywords,
    String? xpathQuery,
    int? maxEvents,
    bool? reverse,
  }) {
    return EventFilter(
      channel: channel ?? this.channel,
      eventIds: eventIds ?? this.eventIds,
      levels: levels ?? this.levels,
      providers: providers ?? this.providers,
      startTime: startTime ?? this.startTime,
      endTime: endTime ?? this.endTime,
      keywords: keywords ?? this.keywords,
      xpathQuery: xpathQuery ?? this.xpathQuery,
      maxEvents: maxEvents ?? this.maxEvents,
      reverse: reverse ?? this.reverse,
    );
  }

  /// Converts this filter to a map for platform channel communication.
  Map<String, dynamic> toMap() {
    return {
      if (channel != null) 'channel': channel,
      if (eventIds != null && eventIds!.isNotEmpty) 'eventIds': eventIds,
      if (levels != null && levels!.isNotEmpty)
        'levels': levels!.map((l) => l.value).toList(),
      if (providers != null && providers!.isNotEmpty) 'providers': providers,
      if (startTime != null) 'startTime': startTime!.toIso8601String(),
      if (endTime != null) 'endTime': endTime!.toIso8601String(),
      if (keywords != null) 'keywords': keywords,
      if (xpathQuery != null) 'xpathQuery': xpathQuery,
      if (maxEvents != null) 'maxEvents': maxEvents,
      'reverse': reverse,
    };
  }

  /// Builds an XPath query string from the filter criteria.
  ///
  /// Returns the custom XPath query if provided, otherwise generates one
  /// from the filter criteria.
  String buildXPath() {
    if (xpathQuery != null) {
      return xpathQuery!;
    }

    final conditions = <String>[];

    // Event IDs
    if (eventIds != null && eventIds!.isNotEmpty) {
      if (eventIds!.length == 1) {
        conditions.add('EventID=${eventIds!.first}');
      } else {
        final idConditions = eventIds!.map((id) => 'EventID=$id').join(' or ');
        conditions.add('($idConditions)');
      }
    }

    // Levels
    if (levels != null && levels!.isNotEmpty) {
      if (levels!.length == 1) {
        conditions.add('Level=${levels!.first.value}');
      } else {
        final levelConditions = levels!
            .map((l) => 'Level=${l.value}')
            .join(' or ');
        conditions.add('($levelConditions)');
      }
    }

    // Providers
    if (providers != null && providers!.isNotEmpty) {
      if (providers!.length == 1) {
        conditions.add("Provider[@Name='${providers!.first}']");
      } else {
        final providerConditions = providers!
            .map((p) => "Provider[@Name='$p']")
            .join(' or ');
        conditions.add('($providerConditions)');
      }
    }

    // Time range
    if (startTime != null || endTime != null) {
      if (startTime != null && endTime != null) {
        conditions.add(
          'TimeCreated[@SystemTime>=\'${_formatSystemTime(startTime!)}\' and '
          '@SystemTime<=\'${_formatSystemTime(endTime!)}\']',
        );
      } else if (startTime != null) {
        conditions.add(
          'TimeCreated[@SystemTime>=\'${_formatSystemTime(startTime!)}\']',
        );
      } else if (endTime != null) {
        conditions.add(
          'TimeCreated[@SystemTime<=\'${_formatSystemTime(endTime!)}\']',
        );
      }
    }

    // Keywords
    if (keywords != null) {
      conditions.add('band(Keywords,$keywords)');
    }

    // Build the query
    if (conditions.isEmpty) {
      return '*';
    }

    return '*[System[${conditions.join(' and ')}]]';
  }

  /// Formats a DateTime to Windows Event Log SystemTime format.
  String _formatSystemTime(DateTime time) {
    return time.toUtc().toIso8601String();
  }

  @override
  String toString() {
    return 'EventFilter(channel: $channel, eventIds: $eventIds, '
        'levels: $levels, maxEvents: $maxEvents)';
  }
}
