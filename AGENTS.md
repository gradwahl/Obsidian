# Repository instructions

- Make every implementation and edit in the local repository.
- Commit and push every completed implementation to GitHub.
- Give each implementation a clear, descriptive commit title. If a pull request is created, give it a clear, descriptive title as well.
- Before pushing, run the checks appropriate to the change and report any checks that could not be completed.
- Do not use Computer Use, desktop UI automation, or browser UI automation for this repository. Use source inspection, command-line tools, automated tests, and saved render artifacts instead. Record live graphical checks as unverified when they require prohibited automation; do not bypass this rule with another UI-control tool.

## Linux and Windows compatibility

- The whole project must work on both Windows and Linux. Treat both as first-class desktop targets across all features, builds, dependency setup, scripts, filesystem paths, configuration, networking, storage, fonts, rendering, and input; this requirement is not limited to the preview or UI.
- Windows compatibility means native Windows support. WSL/WSLg is an additional supported Linux preview route on Windows, not a substitute for native Windows compatibility. Do not claim native Windows support is implemented or verified until its build and launch have been checked.
- For code or build changes, run the relevant builds and tests on both Windows and Linux when available. Use portable implementations or explicit platform-specific implementations for OS-dependent functionality, and preserve PS5 build compatibility. Record existing platform gaps and any new blockers rather than silently narrowing the requirement to the platform that currently works.
- For startup, rendering, windowing, font, input, or UI changes, seek manual visual verification on Windows and Linux, and WSL/WSLg when affected, without using Computer Use or UI automation. Confirm the interface renders, text is readable, and affected interactions work only when supported by actual visual evidence. A successful compilation, startup log, or saved/headless rendering test alone does not establish that a graphical app works; report manual checks that could not be completed.
- Preserve the WSL software-rendering default and explicit renderer override. Do not reintroduce accelerated rendering as the WSL default without verifying a visible, working window on the affected environment.
- When a change affects native Linux display behavior, verify it in a Linux graphical session when available. A Windows WSL preview check does not substitute for this check.
- Keep build dependencies, launch commands, platform limitations, and troubleshooting instructions in README.md accurate for native Windows, Linux, and the Windows WSL/WSLg preview.
- Before pushing, report which builds, tests, and graphical checks passed, and identify any platform or check that could not be verified. Do not describe an untested platform as verified.
