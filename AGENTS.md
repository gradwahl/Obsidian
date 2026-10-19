# Repository instructions

- Make every implementation and edit in the local repository.
- Commit and push every completed implementation to GitHub.
- Give each implementation a clear, descriptive commit title. If a pull request is created, give it a clear, descriptive title as well.
- Before pushing, run the checks appropriate to the change and report any checks that could not be completed.

## Linux and Windows compatibility

- Keep the app working on Linux and on Windows through the documented WSL/WSLg preview. Native Windows support must not be claimed unless a native Windows build and launch have been verified.
- For code or build changes, run the Linux build and relevant tests. Check affected platform-specific paths and preserve PS5 build compatibility.
- For startup, rendering, windowing, font, input, or UI changes, also launch the app through WSL/WSLg on Windows and inspect the actual visible window. Confirm the interface renders, text is readable, and affected interactions work. A successful compilation, startup log, or headless rendering test alone does not establish that the Windows preview works.
- Preserve the WSL software-rendering default and explicit renderer override. Do not reintroduce accelerated rendering as the WSL default without verifying a visible, working window on the affected environment.
- When a change affects native Linux display behavior, verify it in a Linux graphical session when available. A Windows WSL preview check does not substitute for this check.
- Keep build dependencies, launch commands, platform limitations, and troubleshooting instructions in README.md accurate for both Linux and Windows/WSL.
- Before pushing, report which builds, tests, and graphical checks passed, and identify any platform or check that could not be verified. Do not describe an untested platform as verified.
