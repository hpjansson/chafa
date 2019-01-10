Chafa8x8 Custom Block Font
===

## Advantages of this custom font

1. One can read large fonts from images more easily.

2. Very good at dealing with some natural image textures.

3. Higher definition compared to block glyphs in standard Unicode.

## How to build this font

1. Prepare many images, and put them to a folder, e.g. `~/coco/*.jpg`.

2. Create dataset from those images. `./chafa8x8.py CreateDataset --glob "$HOME/coco/*.jpg"`

3. Run K-Means algorithm to find cluster center vectors. `./chafa8x8.py Clustering`

4. Post-process. Generate C code, SVG images and TTF font. `./chafa8x8.py GenA`

## Acknowledgement

The core algorithm used to generate the glyphs is
[K-Means](https://en.wikipedia.org/wiki/K-means_clustering), one of the
clustering algorithms in the machine learning literature.

You can generate a font by using some image dataset e.g.
[MSCOCO](http://cocodataset.org) dataset (reserach-oriented).

## License

Copyright (C) 2018 Mo Zhou, LGPLv3+
