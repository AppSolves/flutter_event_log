# Event Log Plugin - Example App

<p align="center">
  <img src="https://via.placeholder.com/600x400/1a1a1a/42A5F5?text=Event+Log+Example+App" alt="Example App">
</p>

A comprehensive demonstration of the **event_log** Flutter plugin capabilities. This example showcases real-time event monitoring, historical queries, advanced filtering, and all plugin features in a beautiful Material Design 3 interface.

## ✨ Features Demonstrated

### 🎯 Core Functionality
- ✅ **Channel Listing** - Browse all Windows Event Log channels
- ✅ **Channel Information** - View detailed channel metadata
- ✅ **Historical Queries** - Query past events with pagination
- ✅ **Event Filtering** - Filter by severity, time, event ID
- ✅ **Live Monitoring** - Subscribe to real-time events
- ✅ **Event Details** - View all event properties in expandable cards

### 🎨 UI/UX Features
- Modern Material Design 3 interface
- Real-time status indicators
- Expandable event cards with detailed information
- Loading states and error handling
- Responsive layout

## 🚀 Getting Started

### Prerequisites

- Flutter SDK ≥ 3.3.0
- Windows 10/11
- Visual Studio 2019 or later (for Windows development)

### Running the Example

1. **Clone the repository**
   ```bash
   git clone https://github.com/AppSolves/flutter_event_log.git
   cd flutter_event_log/example
   ```

2. **Install dependencies**
   ```bash
   flutter pub get
   ```

3. **Run the app**
   ```bash
   flutter run -d windows
   ```

## 📱 Using the App

### 1. Select a Channel
Use the dropdown at the top to select which Windows Event Log channel to monitor (System, Application, Security, etc.).

### 2. Query Events
- **All Events**: Click "Query Events" to fetch the last 50 events
- **Errors Only**: Click "Query Errors" to filter by error and critical events

### 3. Live Monitoring
- Click "Start Live Monitoring" to subscribe to real-time events
- New events appear at the top of the list automatically
- Click "Stop Monitoring" to unsubscribe

### 4. View Event Details
Click on any event card to expand and see all properties:
- Event ID and Record ID
- Timestamp and level
- Provider information
- Process/Thread IDs
- User SID
- Full XML representation

## 🎓 Code Examples

### Querying Events
```dart
final events = await EventLog.query(
  EventFilter(
    channel: _selectedChannel,
    maxEvents: 50,
    reverse: true,
  ),
);
```

### Subscribing to Live Events
```dart
final subscription = await EventLog.subscribe(
  EventFilter(channel: _selectedChannel),
);

subscription.listen(
  (event) {
    setState(() {
      _liveEvents.insert(0, event);
    });
  },
  onError: (error) {
    print('Subscription error: $error');
  },
);
```

### Filtering by Level
```dart
final errorEvents = await EventLog.query(
  EventFilter(
    channel: _selectedChannel,
    levels: [EventLevel.error, EventLevel.critical],
    maxEvents: 50,
  ),
);
```

## 🔧 Customization

The example app is designed to be easily customizable:

- **Modify filters**: Change the `EventFilter` parameters in query methods
- **Adjust UI**: Customize colors and layouts in the widget tree
- **Add features**: Extend with export functionality, search, or analytics

## 📚 Learn More

- [Event Log Plugin Documentation](../README.md)
- [Windows Event Log API](https://docs.microsoft.com/en-us/windows/win32/wes/windows-event-log)
- [Flutter Desktop Documentation](https://docs.flutter.dev/desktop)

## 🐛 Troubleshooting

**No events showing up?**
- Ensure you're running on Windows
- Try selecting different channels
- Check if events exist in Windows Event Viewer

**Access denied errors?**
- Some channels (like Security) require administrator privileges
- Right-click Visual Studio and "Run as administrator"

**Subscription not working?**
- Verify the channel is enabled in Windows Event Viewer
- Check for any error messages in the status bar

## 📄 License

This example is part of the `event_log` plugin and is licensed under the Apache License 2.0.

See [LICENSE](../LICENSE) for more details.