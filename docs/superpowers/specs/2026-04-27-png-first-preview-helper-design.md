# PNG-first layer assets and preview helper design for openrasterpp

## Problem

Two follow-up changes are requested after the recent move to caller-supplied
`mergedimage.png` and `Thumbnails/thumbnail.png`.

1. `OraDocument` should treat layer image payloads as PNG bytes rather than raw
   RGBA buffers.
2. The library should provide helpers to:
   - convert RGBA image data into PNG bytes, and
   - regenerate `mergedimage.png` / `thumbnail.png` onto `OraDocument`
     when the caller wants the library to render those assets.

The current public API is in an awkward middle state: preview and thumbnail are
caller-supplied PNG, but `layer_images` still stores `ImageBuffer`, so callers
must use two different image representations in the same document model.

## Goal

Make the public image model consistently PNG-first while restoring the helper
flow needed to generate preview and thumbnail assets from layer contents.

The resulting API should:

1. Store `OraDocument::layer_images` as PNG byte sequences.
2. Expose a helper that converts `ImageBuffer` to PNG bytes.
3. Expose a helper that renders `mergedimage.png` and
   `Thumbnails/thumbnail.png` from the document's layer PNG assets.
4. Keep `ora::write()` requiring explicit preview and thumbnail PNG fields.
5. Reuse the existing blend and resize internals rather than inventing a new
   rendering path.

## Non-goals

1. Redesign the provider abstraction.
2. Replace `ImageBuffer` as an internal working type for blending.
3. Add a new high-level builder API such as `add_layer_png()` or
   `add_layer_rgba()`.
4. Validate PNG semantic correctness beyond normal encode/decode failures.
5. Change thumbnail generation policy beyond the previously used
   aspect-ratio-preserving fit within a 256px bound.

## Current state

1. `OraDocument::layer_images` is `std::map<std::string, ImageBuffer>`.
2. `read()` decodes each `data/*.png` entry immediately and stores the decoded
   buffer.
3. `write()` re-encodes each layer image under `data/`.
4. `write()` requires `merged_image_png` and `thumbnail_png` to already be
   present.
5. The repository still has working internal helpers for blending
   (`detail::process_blend`) and resizing (`detail::resize_image`).
6. A previous commit already implemented a PNG-first `layer_images` model plus
   `encode_png()` and `render_preview_and_thumbnail()` helpers, so the desired
   behavior has prior art in this codebase.

## Proposed design

### 1. Public API shape

Change the document model to:

```cpp
std::map<std::string, std::vector<uint8_t>> layer_images;
```

This makes PNG bytes the canonical representation for all image payloads stored
on `OraDocument`.

Add or restore these public helpers:

1. `template<OraProvider Provider> auto encode_png(Provider&, const ImageBuffer&) -> std::expected<std::vector<uint8_t>, Error>;`
2. `auto encode_png(const ImageBuffer&) -> std::expected<std::vector<uint8_t>, Error>;`
3. `template<OraProvider Provider> auto render_preview_and_thumbnail(Provider&, OraDocument&) -> std::expected<void, Error>;`
4. `auto render_preview_and_thumbnail(OraDocument&) -> std::expected<void, Error>;`

`ImageBuffer` remains the library's internal/raw image type for caller code that
needs to construct pixels procedurally, but it is no longer the stored type for
document layer assets.

### 2. Read behavior

`ora::read()` should still decode each layer PNG to ensure it is readable, but
it should store the original PNG bytes in `doc.layer_images`.

That means the read path becomes:

1. Read `data/<name>.png`
2. Decode it to verify the payload is valid PNG
3. Store the original PNG bytes in `doc.layer_images[name]`

This keeps failure behavior explicit while preserving the new PNG-first public
model.

### 3. Write behavior

`ora::write()` should write each `doc.layer_images` entry directly to
`data/<name>.png` without re-encoding it.

The rest of the write behavior remains unchanged:

1. Write `mimetype`
2. Write `stack.xml`
3. Write each layer PNG as provided
4. Require `merged_image_png`
5. Require `thumbnail_png`
6. Write those preview assets unchanged

This removes unnecessary layer re-encoding and keeps the current explicit
preview/thumbnail contract intact.

### 4. Preview/thumbnail helper behavior

`render_preview_and_thumbnail()` should:

1. Validate that `doc.width` and `doc.height` are non-zero.
2. Decode every PNG in `doc.layer_images` into `ImageBuffer`.
3. Blend the layer tree using `detail::process_blend()`.
4. Encode the blended RGBA result as `doc.merged_image_png`.
5. Generate a thumbnail by fitting the rendered image within a 256px maximum
   dimension while preserving aspect ratio. Images already within that bound are
   not upscaled.
6. Encode the thumbnail RGBA result as `doc.thumbnail_png`.

The helper does not change `layer_images`; it overwrites `merged_image_png` and
`thumbnail_png` with newly rendered PNG bytes.

### 5. Error handling

The change should preserve existing error surfaces where possible:

1. PNG decode failures while reading or rendering preview assets propagate as
   `PngDecodeFailed`.
2. Invalid zero dimensions for preview generation return
   `InvalidOraDocument`.
3. Missing layer image entries referenced by nodes continue to fail through
   `detail::process_blend()`.
4. PNG encode failures in `encode_png()` or preview generation propagate as
   `PngEncodeFailed`.

No broad fallback should be introduced. Invalid input should fail explicitly.

### 6. Documentation impact

README and header comments should be updated so the public contract is clear:

1. `OraDocument::layer_images` now stores PNG bytes.
2. Callers that start from RGBA should use `ora::encode_png()`.
3. Callers that need preview and thumbnail assets can call
   `ora::render_preview_and_thumbnail(doc)` before `ora::write()`.
4. `ora::write()` still expects `merged_image_png` and `thumbnail_png` to be
   populated.

## Alternatives considered

### Alternative A: keep `layer_images` as `ImageBuffer` and only add helpers

Rejected because it keeps the document model inconsistent: layer images would be
raw RGBA while preview/thumbnail remain PNG bytes. That conflicts with the issue
request and continues to force callers to manage two formats.

### Alternative B: keep compatibility helpers for direct RGBA layer insertion

Rejected because issue #1 explicitly asks to retire the direct RGBA path.
Keeping compatibility helpers would blur the intended API direction.

### Alternative C: introduce a larger encapsulated API for layer mutation

Rejected because it is beyond the requested scope. The issue asks for format
consistency and helper restoration, not a full object model redesign.

## Verification

Validation should confirm:

1. `encode_png()` converts `ImageBuffer` into decodable PNG bytes.
2. `read()` stores layer payloads as PNG bytes instead of `ImageBuffer`.
3. `write()` copies layer PNG bytes into `data/*.png` without calling PNG
   encode for those entries.
4. `render_preview_and_thumbnail()` fills both preview fields on success.
5. `render_preview_and_thumbnail()` works with the default provider, not just a
   test double.
6. `render_preview_and_thumbnail()` fails clearly on invalid dimensions or bad
   layer PNG input.
7. README examples match the PNG-first API and helper workflow.
