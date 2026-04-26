# External preview/thumbnail asset design for openrasterpp

## Problem

`ora::write()` currently renders the full document internally, writes the result
to `mergedimage.png`, then downsamples that rendered image to produce
`Thumbnails/thumbnail.png`.

The requested behavior is to stop generating these files internally and require
the caller to provide both assets from outside the library.

## Goal

Change the write path so that:

1. `mergedimage.png` is supplied externally by the caller.
2. `Thumbnails/thumbnail.png` is supplied externally by the caller.
3. `ora::write()` writes those provided PNG byte sequences as-is.
4. The library no longer renders or resizes images internally for preview or
   thumbnail generation.

## Non-goals

1. Change how layer image files under `data/` are written.
2. Redesign the provider abstraction for this task.
3. Add new image validation beyond checking whether the required assets are
   present.
4. Expand `read()` to fully load preview and thumbnail assets in this change.

## Current state

1. `OraDocument` stores document dimensions, node hierarchy, and layer image data.
2. `ora::write()` serializes `stack.xml` and `data/*.png`, then composes a merged
   image internally with `detail::process_blend()`.
3. The merged image is encoded to `mergedimage.png`.
4. A thumbnail is derived from the merged image with `detail::resize_image()` and
   encoded to `Thumbnails/thumbnail.png`.

## Proposed design

### 1. Public API shape

Extend `OraDocument` with two optional fields:

1. `merged_image_png`
2. `thumbnail_png`

Both fields store PNG-encoded byte sequences supplied by the caller. The write
API remains `ora::write(filename, doc)` / `ora::write(provider, filename, doc)`,
so the call shape stays stable while the asset source moves outside the library.

### 2. Write behavior

`ora::write()` will:

1. Write `mimetype`
2. Write `stack.xml`
3. Write `data/*.png` for layer images
4. Write `mergedimage.png` from `doc.merged_image_png`
5. Write `Thumbnails/thumbnail.png` from `doc.thumbnail_png`

The supplied preview and thumbnail bytes are written unchanged. The library does
not decode, re-encode, render, or resize them.

### 3. Missing asset handling

Both externally supplied assets are required for writing. If either
`merged_image_png` or `thumbnail_png` is absent, `ora::write()` returns
`Error::Code::InvalidOraDocument`.

This intentionally turns a previously implicit internal behavior into an explicit
write-time requirement.

### 4. Internal implementation impact

The write path will stop using:

1. `detail::process_blend()` for preview generation
2. `detail::resize_image()` for thumbnail generation

These helpers can remain in the codebase if they are still used elsewhere, but
they will no longer participate in writing `mergedimage.png` or
`Thumbnails/thumbnail.png`.

### 5. Read-path scope

`read()` stays focused on document structure and layer image loading for this
change. The new `OraDocument` fields exist primarily to support writing.

If loading preview or thumbnail assets becomes useful later, that can be added as
a separate change without blocking this one.

### 6. Documentation and compatibility

The README and public API comments should be updated to explain that callers must
provide both PNG assets before calling `write()`.

Existing call sites that relied on implicit generation may still compile but can
now fail at runtime with `InvalidOraDocument` until they populate the new fields.
This behavior change is intentional and should be documented clearly.

## Verification

Validation should confirm:

1. `ora::write()` succeeds when both PNG assets are present.
2. `ora::write()` fails with `InvalidOraDocument` when either asset is missing.
3. The written archive contains the exact externally supplied bytes at
   `mergedimage.png` and `Thumbnails/thumbnail.png`.
4. The write path does not call PNG encoding for preview/thumbnail generation.
5. The README usage example reflects the new caller-supplied asset requirement.
