# Agent instructions

## Keep the sketch organized

- Keep the Arduino sketch and its directly included headers at the repository root. Put page drawing in `page_*.h`, shared drawing and state in `util.h`, network fetching in `net.h`, and user settings in `config.h`. Put host-only checks in `tests/`, generators in `tools/`, and their source data in `data/`.
- Make focused changes. Reuse existing helpers and configuration values before adding another helper, global, or setting. Keep page registration in `m5papers3_calendar.ino` consistent with the page headers and `buildPageList()`.
- Keep generated `place_names.h` in sync with `data/jma_epicenters.json` and `tools/generate_place_names.py`; regenerate it instead of hand-editing generated entries.
- Keep credentials and local build output out of the repository. `secrets.h` is local and gitignored; never print or commit its values. Put temporary test binaries and build artifacts outside the repository or remove them after use.
- Update `README.md` when changing setup, configuration, page behavior, or data sources. Add or update a focused host check for logic that can run without the device; use the existing tests in `tests/` as examples. Avoid tests that only repeat the implementation.
- Before finishing, inspect `git status` and the diff for unrelated edits, accidental generated files, credentials, and whitespace errors. Report what was checked and any device-only behavior that remains unverified.

## Device memory and storage

Before finishing a firmware change, check whether it fits the M5PaperS3's **remaining** resources, not just its total capacity.

- For program storage, compile for the M5PaperS3 with the project's actual board, partition, and PSRAM settings. Compare the new firmware size with the baseline and the available application partition; include any added filesystem or persistent data in the storage estimate.
- For RAM, account for added globals and the **peak simultaneous** allocations in internal heap and PSRAM. Include temporary buffers, network/TLS activity, display buffers, and the largest contiguous internal allocation needed. Compare these demands with measured free heap, largest free internal block, and free PSRAM at the relevant point in the existing firmware. The news-fetch memory logs in `net.h` are one available measurement point.
- Leave practical headroom for normal runtime variation. If a build or on-device measurement is unavailable, use a conservative estimate, state the missing evidence and uncertainty, and do not claim that the change fits. Report the estimated or measured change and remaining margin in the final response. Reduce memory use or storage use if the evidence shows the change will not fit.
