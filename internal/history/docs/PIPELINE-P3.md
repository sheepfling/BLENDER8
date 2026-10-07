# Production pipeline

The current pipeline contract is consolidated in [RECIPE-GUIDE.md](../RECIPE-GUIDE.md), rather than
preserving a contradictory second description here.

Canonical component behavior -> vendor-styled LaTeX -> clean PDF -> validated copy recipe ->
visible-page raster -> paper/handling -> repeated affine/optical/copier passes -> protected-region
limit -> image-only PDF and replay manifest.

Depth-zero jobs are exact clean-PDF passthroughs. Metadata, OCR/accessibility limitations, parameter
units, merge precedence, effect order, geometry and reproducibility are specified in that guide.
Source authorship and technical review rules remain in [AUTHORING.md](../AUTHORING.md).
