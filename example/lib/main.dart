import 'dart:async';

import 'package:event_log/event_log.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Event Log Demo',
      theme: ThemeData(primarySwatch: Colors.blue, useMaterial3: true),
      home: const EventLogDemo(),
    );
  }
}

class EventLogDemo extends StatefulWidget {
  const EventLogDemo({super.key});

  @override
  State<EventLogDemo> createState() => _EventLogDemoState();
}

class _EventLogDemoState extends State<EventLogDemo> {
  List<ChannelInfo> _channels = [];
  List<EventRecord> _events = [];
  EventLogSubscription? _subscription;
  String _selectedChannel = 'System';
  bool _isLoading = false;
  String _status = 'Ready';
  final List<EventRecord> _liveEvents = [];

  @override
  void initState() {
    super.initState();
    _loadChannels();
  }

  @override
  void dispose() {
    _subscription?.cancel();
    super.dispose();
  }

  Future<void> _loadChannels() async {
    setState(() {
      _isLoading = true;
      _status = 'Loading channels...';
    });

    try {
      final channels = await EventLog.listChannels();
      setState(() {
        _channels = channels;
        _status = 'Loaded ${channels.length} channels';
      });
    } catch (e) {
      setState(() {
        _status = 'Error loading channels: $e';
      });
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _queryEvents() async {
    setState(() {
      _isLoading = true;
      _status = 'Querying events...';
    });

    try {
      final filter = EventFilter(
        channel: _selectedChannel,
        maxEvents: 50,
        reverse: true,
      );

      final events = await EventLog.query(filter);
      setState(() {
        _events = events;
        _status = 'Found ${events.length} events';
      });
    } catch (e) {
      setState(() {
        _status = 'Error querying events: $e';
      });
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _queryErrorEvents() async {
    setState(() {
      _isLoading = true;
      _status = 'Querying error events...';
    });

    try {
      final filter = EventFilter(
        channel: _selectedChannel,
        levels: [EventLevel.error, EventLevel.critical],
        maxEvents: 50,
        reverse: true,
      );

      final events = await EventLog.query(filter);
      setState(() {
        _events = events;
        _status = 'Found ${events.length} error events';
      });
    } catch (e) {
      setState(() {
        _status = 'Error querying events: $e';
      });
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _startLiveMonitoring() async {
    try {
      _subscription?.cancel();
      _liveEvents.clear();

      final filter = EventFilter(channel: _selectedChannel);

      final subscription = await EventLog.subscribe(filter);
      subscription.listen(
        (event) {
          setState(() {
            _liveEvents.insert(0, event);
            if (_liveEvents.length > 100) {
              _liveEvents.removeLast();
            }
            _status = 'Live monitoring: ${_liveEvents.length} events received';
          });
        },
        onError: (error) {
          setState(() {
            _status = 'Subscription error: $error';
          });
        },
      );

      setState(() {
        _subscription = subscription;
        _status = 'Live monitoring started on $_selectedChannel';
      });
    } catch (e) {
      setState(() {
        _status = 'Error starting subscription: $e';
      });
    }
  }

  Future<void> _stopLiveMonitoring() async {
    await _subscription?.cancel();
    setState(() {
      _subscription = null;
      _status = 'Live monitoring stopped';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Windows Event Log Viewer'),
        elevation: 2,
      ),
      body: Column(
        children: [
          _buildControlPanel(),
          _buildStatusBar(),
          Expanded(child: _buildContent()),
        ],
      ),
    );
  }

  Widget _buildControlPanel() {
    return Card(
      margin: const EdgeInsets.all(8),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                const Text(
                  'Channel:',
                  style: TextStyle(fontWeight: FontWeight.bold),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: DropdownButton<String>(
                    value: _selectedChannel,
                    isExpanded: true,
                    items:
                        {
                          'System',
                          'Application',
                          'Security',
                          ..._channels.map((c) => c.name),
                        }.map((channel) {
                          return DropdownMenuItem(
                            value: channel,
                            child: Text(channel),
                          );
                        }).toList(),
                    onChanged: (value) {
                      if (value != null) {
                        setState(() => _selectedChannel = value);
                      }
                    },
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                ElevatedButton.icon(
                  onPressed: _isLoading ? null : _loadChannels,
                  icon: const Icon(Icons.refresh),
                  label: const Text('Refresh Channels'),
                ),
                ElevatedButton.icon(
                  onPressed: _isLoading ? null : _queryEvents,
                  icon: const Icon(Icons.search),
                  label: const Text('Query Events'),
                ),
                ElevatedButton.icon(
                  onPressed: _isLoading ? null : _queryErrorEvents,
                  icon: const Icon(Icons.error_outline),
                  label: const Text('Errors Only'),
                ),
                ElevatedButton.icon(
                  onPressed: _subscription == null
                      ? _startLiveMonitoring
                      : _stopLiveMonitoring,
                  icon: Icon(
                    _subscription == null ? Icons.play_arrow : Icons.stop,
                  ),
                  label: Text(
                    _subscription == null ? 'Start Live' : 'Stop Live',
                  ),
                  style: ElevatedButton.styleFrom(
                    backgroundColor: _subscription == null
                        ? Colors.green
                        : Colors.red,
                    foregroundColor: Colors.white,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusBar() {
    if (kDebugMode) {
      print('Status updated: $_status');
    }
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      color: Colors.grey[200],
      child: Row(
        children: [
          if (_isLoading)
            const SizedBox(
              width: 16,
              height: 16,
              child: CircularProgressIndicator(strokeWidth: 2),
            ),
          if (_isLoading) const SizedBox(width: 8),
          Expanded(
            child: Text(
              _status,
              style: TextStyle(fontSize: 12, color: Colors.grey[700]),
            ),
          ),
          if (_subscription != null)
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
              decoration: BoxDecoration(
                color: Colors.green,
                borderRadius: BorderRadius.circular(12),
              ),
              child: const Text(
                'LIVE',
                style: TextStyle(
                  color: Colors.white,
                  fontSize: 10,
                  fontWeight: FontWeight.bold,
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildContent() {
    final displayEvents = _subscription != null ? _liveEvents : _events;

    if (displayEvents.isEmpty) {
      return Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.event_note, size: 64, color: Colors.grey[400]),
            const SizedBox(height: 16),
            Text(
              _subscription != null
                  ? 'Waiting for events...'
                  : 'No events to display',
              style: TextStyle(color: Colors.grey[600], fontSize: 16),
            ),
            if (_subscription == null) ...[
              const SizedBox(height: 8),
              Text(
                'Click "Query Events" to load events',
                style: TextStyle(color: Colors.grey[500], fontSize: 14),
              ),
            ],
          ],
        ),
      );
    }

    return ListView.builder(
      itemCount: displayEvents.length,
      itemBuilder: (context, index) {
        final event = displayEvents[index];
        return _buildEventCard(event);
      },
    );
  }

  Widget _buildEventCard(EventRecord event) {
    Color getLevelColor(EventLevel level) {
      switch (level) {
        case EventLevel.critical:
          return Colors.red[900]!;
        case EventLevel.error:
          return Colors.red;
        case EventLevel.warning:
          return Colors.orange;
        case EventLevel.information:
          return Colors.blue;
        default:
          return Colors.grey;
      }
    }

    IconData getLevelIcon(EventLevel level) {
      switch (level) {
        case EventLevel.critical:
        case EventLevel.error:
          return Icons.error;
        case EventLevel.warning:
          return Icons.warning;
        case EventLevel.information:
          return Icons.info;
        default:
          return Icons.circle;
      }
    }

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      child: ExpansionTile(
        leading: Icon(
          getLevelIcon(event.level),
          color: getLevelColor(event.level),
        ),
        title: Text(
          'Event ${event.eventId} - ${event.providerName}',
          style: const TextStyle(fontWeight: FontWeight.bold),
        ),
        subtitle: Text(
          '${event.timeCreated.toLocal()} - ${event.channel}',
          style: const TextStyle(fontSize: 12),
        ),
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                _buildDetailRow('Record ID', event.eventRecordId.toString()),
                _buildDetailRow('Level', event.level.name),
                _buildDetailRow('Computer', event.computer),
                if (event.message != null)
                  _buildDetailRow('Message', event.message!),
                if (event.processId != null)
                  _buildDetailRow('Process ID', event.processId.toString()),
                if (event.threadId != null)
                  _buildDetailRow('Thread ID', event.threadId.toString()),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildDetailRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 100,
            child: Text(
              '$label:',
              style: const TextStyle(fontWeight: FontWeight.bold),
            ),
          ),
          Expanded(child: SelectableText(value)),
        ],
      ),
    );
  }
}
