# Test fixtures

No binary blobs are stored here by design. All regression fixtures are
synthetic and generated at test runtime (`src/test/Suite.cpp`) into the OS
temp directory: minimal PE32/PE32+ images, BMP/TGA samples, truncated files.
This keeps the repo free of copyrighted game data and keeps tests hermetic
(no SDK, no network, no external files required).
