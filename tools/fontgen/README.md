Chafa8x8 Custom Block Font
===

## Advantages of this custom font

1. One can read large fonts from images more easily.

2. Very good at dealing with some natural image textures.

3. Higher definition compared to block glyphs in standard Unicode.

## How to build this font

1. Prepare many images, and put them to a folder, e.g. `~/coco/*.jpg`.

2. Create dataset from those images. `./chafa8x8.py CreateDataset --glob "$HOME/coco/*.jpg"`. Note, don't miss the quotes because the wildcard should be expanded by python's `glob` instead of the shell. Although the default parameters should be fine, the optional arguments are: (1) `-Mc <int>` specifies the number of vectors to sample from each image file. The default is `16`. Namely, in total 16 random crops (converted to vectors) are sampled from each image; (2) `-N <int>` specifies the vector length. The default is 64 for 8x8 blocks. After this, a fine named `chafa8x8.npz` will be created for the following clustering step.

3. Run K-Means algorithm to find cluster center vectors. `./chafa8x8.py Clustering`. This will create the optimal vectors. The optional parameter `-C <int>` specifies how many vectors you want (i.e., how many optimal 8x8 blocks you want). The actual usable vectors (glyphs) will be less than this number after post processing (such as deduplication). The results will be stored in `chafa8x8.raw.json` by default. Note, this step is slow. For GPU accelerated computing, see the misc notes in the next section.

4. Post-process. Generate C code, SVG images and TTF font. `./chafa8x8.py GenA`. This is actually a composed command. It executes the following four steps: (1) It invokes `./chafa8x8.py Postproc` to deduplicate the optimal glyphs. The results will be written in `chafa8x8.json`; (2) Then it invokes `./chafa8x8.py GenC` to generate C header from json as `chafa8x8.h`; (3) Next, it invokes `./chafa8x8.py GenSVG` to convert the optimal glyphs from json into SVG files under the `chafa8x8_svg` directory; (4) Lastly, it invokes `./chafa8x8.py GenFont` to create the font file `chafa8x8.ttf` from the json result.

## Misc Notes

1. Python dependencies: `pip3 install numpy scipy scikit-learn tqdm`.

2. Note, in order to generate a usable font, the `python3-fontforge` (for Debian bullseye and newer) package has to be installed as well. It will be used in the `./chafa8x8.py GenA` step. It will automatically invoke `chafa8x8.py GenFont` subcommand for creating the font.

3. Note, the clustering step is the most time-consuming procedure. If the created dataset is very large (e.g., more than 1 million vectors inside), it won't be surprising to run for several hours or even longer. Although the result can be better with a large dataset, the resulting fonts should be good to use as long as, say, the dataset size is more than 10 times larger than the number of optimal glyphs we want.

4. You may take a quick glance at the glyphs from the json files, e.g., for the finalized glyphs: `./chafa8x8.py Dump --json chafa8x8.json | less`, and for the preliminary glyphs before deduplication: `./chafa8x8.py Dump --json chafa8x8.raw.json | less`.

5. The clustering step can be accelerated using Nvidia GPU (through CUDA). You need to install faiss first: `pip3 install faiss-gpu`. To use it, just specify the backend in the command line: `./chafa8x8.py Clustering --backend faiss`. 

## Todos

- [ ] update `compare.py` with the latest usage.

## Acknowledgement

The core algorithm used to generate the glyphs is
[K-Means](https://en.wikipedia.org/wiki/K-means_clustering), one of the
clustering algorithms in the machine learning literature.

You can generate a font by using some image dataset e.g.
[MSCOCO](http://cocodataset.org) dataset (research-oriented).

## License

Copyright (C) 2018 Mo Zhou, LGPLv3+
