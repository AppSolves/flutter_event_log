/// Represents the severity level of a Windows Event.
///
/// These levels correspond to the standard Windows Event Log levels.
enum EventLevel {
  /// Indicates detailed trace information (Level 5)
  verbose(5, 'Verbose'),

  /// Indicates informational messages (Level 4)
  information(4, 'Information'),

  /// Indicates warning messages (Level 3)
  warning(3, 'Warning'),

  /// Indicates error messages (Level 2)
  error(2, 'Error'),

  /// Indicates critical error messages (Level 1)
  critical(1, 'Critical'),

  /// Indicates log always messages (Level 0)
  logAlways(0, 'LogAlways');

  const EventLevel(this.value, this.name);

  /// The numeric value of the level
  final int value;

  /// The display name of the level
  final String name;

  /// Creates an [EventLevel] from an integer value
  static EventLevel fromValue(int value) {
    return EventLevel.values.firstWhere(
      (level) => level.value == value,
      orElse: () => EventLevel.information,
    );
  }

  @override
  String toString() => name;
}
