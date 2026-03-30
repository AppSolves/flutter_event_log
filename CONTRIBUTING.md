# Contributing to event_log

Thanks for your interest in contributing to `event_log`.

## Development Setup

1. Clone the repository:

```bash
git clone https://github.com/AppSolves/flutter_event_log.git
cd flutter_event_log
```

2. Install dependencies:

```bash
flutter pub get
cd example
flutter pub get
```

3. Run the main checks before opening a pull request:

```bash
flutter analyze
flutter test
cd example
flutter analyze
flutter build windows
flutter test integration_test -d windows
```

## Formatting

Format the repository with:

```powershell
pwsh -File scripts/format_all.ps1
```

Install the pre-commit hook with:

```powershell
pwsh -File scripts/install-git-hooks.ps1
```

## Contribution Guidelines

- Keep pull requests focused and easy to review
- Add or update tests when behavior changes
- Avoid unrelated refactors in the same pull request
- Update documentation when user-facing behavior changes

## Windows Plugin Notes

This package targets the Windows Event Log APIs. Changes to the native plugin
should be validated against the Windows example app and integration tests.

## Reporting Bugs

When filing a bug, please include:

- The package version
- Your Windows version
- Clear reproduction steps
- Expected behavior
- Actual behavior
- Relevant logs or screenshots when helpful

## Security

If you discover a security issue, please follow the guidance in
[SECURITY.md](SECURITY.md).
