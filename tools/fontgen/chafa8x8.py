#!/usr/bin/env python3
# This script is written for Chafa in order to generate custom fonts.
# Copyright (C) 2018-2023 Mo Zhou <lumin@debian.org>
# License: LGPLv3+
import os
import sys
import argparse
import glob
try:
    import ujson as json
except ModuleNotFoundError:
    import json
import time
import random
from PIL import Image
from subprocess import call
import numpy as np
from typing import *

# Preserved Unicode Plane for Private use, see wikipedia for detail.
UNICODE_BASE = 0x100000


def bdump8x8(v: np.ndarray, *, clang: bool = False, padding: int = 0) -> str:
    '''
    dump the 8x8 bitmap.
    v: np.ndarray -- the vector (bitmap) to dump
    clang: bool -- dump C code intead of plain text
    padding: int -- number of spaces to indent in clang mode
    '''
    assert(v.size == 64)
    dump = []
    for (i, v) in enumerate(v):
        if i % 8 == 0 and clang:
            dump.append(' '*padding + "\"")
        if v:
            dump.append('X')
        else:
            dump.append(' ' if clang else '.')
        if (i+1) % 8 == 0 and clang:
            dump.append("\"")
        if (i+1) % 8 == 0:
            dump.append('\n')
    return ''.join(dump)


def mainCreateDataset(argv: List[str]):
    '''
    Generate dataset used for generating chafa8x8 font from MS-COCO dataset
    http://cocodataset.org/
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--glob', type=str, default='~/coco/*.jpg',
                    help='image file glob pattern to be expanded by python')
    ag.add_argument('-Mc', type=int, default=16, help='num crops per image')
    ag.add_argument('-N', type=int, default=64, help='vector length')
    ag.add_argument('--save', type=str, default='chafa8x8.npz')
    ag = ag.parse_args(argv)
    from tqdm import tqdm

    if not os.path.exists(ag.save):
        # Glob images
        images = glob.glob(ag.glob, recursive=True)
        Mi = len(images)
        print(
            f'=> Found {Mi} images. Will sample {ag.Mc} vectors from each image')
        print(f' -> Dataset size M = {Mi*ag.Mc}')
        print(
            f' -> The resulting dataset takes {Mi*ag.Mc*ag.N/(1024**2)} MiB space.')

        # Sample Mi*Mc vectors as the dataset from those images
        print(f'=> Sampling vectors from every image ...')
        dataset = np.zeros((Mi*ag.Mc, ag.N), dtype=np.uint8)
        for i in tqdm(range(Mi)):
            image = random.choice(images)
            image = Image.open(image)
            width, height = image.size
            for c in range(ag.Mc):
                w = random.randrange(16, min(width, height//2, 48))
                h = 2*w  # ratio: h/w = 2/1
                woff = random.randrange(0, width - w)
                hoff = random.randrange(0, height - h)
                box = (woff, hoff, woff+w, hoff+h)
                region = image.crop(box).convert('1')
                region = region.resize((8, 8))
                region = np.array(region).ravel()
                dataset[i*ag.Mc+c, :] = region
        # save the dataset as acche
        np.savez(ag.save, dataset=dataset)
    else:
        print(f'=> Dataset already generated: {ag.save}')


def mainClustering(argv: List[str]):
    '''
    Run K-Means algorithm on the dataset. For large-scale training,
    MiniBatchKMeans will be much faster than KMeans.
    (This will still take some time if dataset is large enough)
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--dataset', type=str, default='chafa8x8.npz')
    ag.add_argument('--save', type=str, default='chafa8x8.raw.json')
    ag.add_argument('-C', type=int, default=5120, help='number of clusters')
    ag.add_argument('-B', '--backend', type=str, default='sklearn',
                    choices=('sklearn', 'faiss'), help='k-means backend')
    ag.add_argument('-I', '--iterations', type=int, default=100,
                    help='number of iterations for K-means algorithm (faiss)')
    ag = ag.parse_args(argv)

    print(f'=> loading dataset from {ag.dataset}')
    dataset = np.load(ag.dataset)['dataset']

    # Findout the cluster centers with KMeans
    print('=> Clustering ...')
    if ag.backend == 'sklearn':
        from sklearn.cluster import KMeans, MiniBatchKMeans
        kmeans = MiniBatchKMeans(n_clusters=ag.C, init='k-means++',
                                 init_size=37*ag.C, batch_size=8*ag.C,
                                 compute_labels=False, verbose=True).fit(dataset)
        centers = (kmeans.cluster_centers_ >= 0.5).astype(np.uint8)

        # Save the result to JSON file
        centers = list(sorted([list(map(int, center)) for center in centers],
                              key=lambda x: sum(x)))
        json.dump(centers, open(ag.save, 'w'))
    elif ag.backend == 'faiss':
        import faiss  # pip3 install faiss-gpu
        dataset = dataset.astype(np.float32)  # uint8 to float32
        print('dataset type', dataset.dtype, ', size', dataset.shape)
        clus = faiss.Clustering(dataset.shape[1], ag.C)
        clus.verbose = True
        clus.niter = ag.iterations
        clus.max_points_per_centroid = 10000000
        res = faiss.StandardGpuResources()
        cfg = faiss.GpuIndexFlatConfig()
        cfg.useFloat16 = False
        index = faiss.GpuIndexFlatL2(res, dataset.shape[1], cfg)
        clus.train(dataset, index)
        centroids = faiss.vector_float_to_array(clus.centroids)
        centroids = centroids.reshape(ag.C, dataset.shape[1])
        centroids = (centroids >= 0.5).astype(np.uint8)
        print('centroids', centroids.shape, centroids.dtype)

        # sort and save
        centroids = list(sorted(
            [list(map(int, c)) for c in centroids],
            key=lambda x: sum(x)
        ))
        json.dump(centroids, open(ag.save, 'w'))
    print(f'=> {ag.save}')


def mainPostproc(argv: List[str]):
    '''
    Post-processing to make the glyphs "smoother" and do deduplication
    Default convolution kernel: Gaussian 3x3
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.raw.json')
    ag.add_argument('--save', type=str, default='chafa8x8.json')
    ag = ag.parse_args(argv)
    from scipy.signal import convolve2d as conv2d

    # load raw vectors
    centers = json.load(open(ag.json, 'r'))
    print(' -> (before) number of centers:', len(centers))

    # [default kernel] Gaussian kernel (kernel size = 3)
    Kg = np.array([[1/16, 1/8, 1/16], [1/8, 1/4, 1/8], [1/16, 1/8, 1/16]])
    # mean value (glyphs tends to have round corner instead of sharp ones)
    Km = np.ones((3, 3))/9
    # Select Kg as the default kernel
    K = Kg
    for (i, center) in enumerate(centers):
        center = np.array(center).reshape((8, 8))
        center = conv2d(center, K, 'same').ravel()
        centers[i] = (center >= 0.5)
    centers = np.unique(np.array(centers), axis=0)
    print(' -> (after) number of centers:', centers.shape[0])
    centers = list(sorted([list(map(int, center)) for center in centers],
                          key=lambda x: sum(x)))
    json.dump(centers, open(ag.save, 'w'))
    print(f'=> Result saved to {ag.save}')


def mainDump(argv: List[str]):
    '''
    Dump human-readable bitmaps from json file
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, required=True)
    ag = ag.parse_args(argv)

    centers = json.load(open(ag.json, 'r'))
    for (i, center) in enumerate(centers):
        center = np.array(center)
        print('________', i)
        print(bdump8x8(center))


def mainGenC(argv: List[str]):
    '''
    Generate C code
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.json')
    ag.add_argument('--dst', type=str, default='chafa8x8.h')
    ag = ag.parse_args(argv)

    f = open(ag.dst, 'w')
    f.write(f'/* Auto-generated by {os.path.basename(__file__)}\n')
    f.write(f' * Version: {time.ctime()}\n')
    f.write(f' */\n')
    centers = json.load(open(ag.json, 'r'))
    for (i, center) in enumerate(centers):
        code = UNICODE_BASE + i
        comment = (1 not in center)
        if comment:
            f.write('#if 0\n')
        f.write('{\n')
        f.write(f'    /* Chafa8x8 Font, ID: {i}, Unicode: 0x{code:x} */\n')
        f.write(f'    CHAFA_SYMBOL_TAG_CUSTOM,\n')
        f.write(f'    0x{code:x},\n')
        f.write(bdump8x8(np.array(center), clang=True, padding=4))
        f.write('},\n')
        if comment:
            f.write('#endif\n')
    f.close()


def mainGenSVG(argv: List[str]):
    '''
    Generate SVG files
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.json')
    ag.add_argument('--dir', type=str, default='chafa8x8_svg')
    ag = ag.parse_args(argv)

    if not os.path.exists(ag.dir):
        os.mkdir(ag.dir)

    centers = json.load(open(ag.json, 'r'))
    for (i, center) in enumerate(centers):
        f = open(os.path.join(ag.dir, f'{i}.svg'), 'w')
        f.write('''\
<?xml version="1.0" encoding="UTF-8"?>
<svg version="1.1"
     height="1000"
     width="500"
     viewBox="0 0 500 1000"><defs />
<g>
''')
        template = '<rect style="fill:#000000" width="{w}" height="{h}" x="{x}" y="{y}" />\n'
        center = np.array(center).reshape(8, 8).astype(np.uint8)
        indeces = np.argwhere(center > 0)
        for index in indeces:
            r, c = index
            _w = (50 + 500.0 + 50) / 8.0
            _h = (200 + 1000.0 + 75) / 8.0
            f.write(template.format(w=_w, h=_h, x=-50+c*_w, y=-200+r*_h))
        f.write(''' </g>\n</svg>''')
        f.close()


def mainGenBDF(argv: List[str]):
    '''
    Generate BDF font (it is a plain text format!)
    https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.json')
    ag.add_argument('--bdf', type=str, default='chafa8x8.bdf')
    ag = ag.parse_args(argv)

    # open file
    with open(ag.json, 'rt') as f:
        j = json.load(f)  # List[List[int]]
    bdf = open(ag.bdf, 'wt')

    # write bdf header
    _timestamp_ = time.ctime().replace(' ', '-').replace(':', '-')
    bdf.writelines([
        'STARTFONT 2.1\n',
        f'FONT chafa8x8-{len(j)}-{_timestamp_}\n',
        'SIZE 16 75 75\n',
        'FONTBOUNDINGBOX 16 16 0 -2\n',
        '\n',
        'STARTPROPERTIES 2\n',
        'FONT_ASCENT 14\n',
        'FONT_DESCENT 2\n',
        'ENDPROPERTIES\n',
        '\n',
        f'CHARS {len(j)}\n',
        '\n',
    ])

    # write characters
    _base_ = 0x100000
    for (i, vector) in enumerate(j):
        # start char
        bdf.writelines([
            f'COMMENT Chafa8x8-glyph-id-{i}\n',
            f'STARTCHAR U+{hex(_base_ + i)}\n',
            f'ENCODING {int(_base_ + i)}\n',
            f'SWIDTH 500 0\n',
            f'DWIDTH 8 0\n',
            'BBX 8 16 0 -2\n',
            'BITMAP\n',
        ])
        # glyph representation
        for i in range(0, 64, 8):
            line = 0b0
            for j in range(8):
                line = line + (0b1 << j) * vector[i + j]
            line = '{:02x}'.format(line).upper()
            bdf.writelines([line + '\n'] * 2)
        # end char
        bdf.writelines(['ENDCHAR\n', '\n'])

    # end font
    bdf.write('ENDFONT\n')

    # close file
    bdf.close()


def mainGenFont(argv: List[str]):
    '''
    Generate Font file (ttf) using fontforge
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.json')
    ag.add_argument('--ttf', type=str, default='chafa8x8.ttf')
    ag = ag.parse_args(argv)
    try:
        import fontforge as ff
    except ModuleNotFoundError:
        print('You need to install the python3-fontforge package to generate the font')
        exit(1)
    import json

    N = len(json.load(open('chafa8x8.json', 'r')))

    try:
        font = ff.font()
        font.clear()
        font.copyright = "(C) 2018 Mo Zhou <lumin@debian.org>, LGPLv3+"
        font.fontname = "Chafa8x8"
        font.familyname = "monospace"
        font.fullname = "Chafa8x8 block glyphs by K-Means for character art"
        font.version = "0a"
        font.encoding = 'Custom'
        print('Number of glyphs:', N)
        for i in range(N):
            glyph = font.createChar(0x100000 + i)
            glyph.importOutlines('chafa8x8_svg/%d.svg' % i)
            glyph.left_side_bearing = 0
            glyph.right_side_bearing = 0
            glyph.width = 500
            glyph.vwidth = 1000
        # font.save('chafa8x8.sfd')
        font.generate(ag.ttf)
        font.close()
    except Exception as e:
        print(e)
    print(f'done. the font has been written as {ag.ttf}')


def mainGenA(argv: List[str]):
    print('calling ./chafa8x8.py Postproc')
    call(['./chafa8x8.py', 'Postproc'])
    print('calling ./chafa8x8.py GenC')
    call(['./chafa8x8.py', 'GenC'])
    print('calling ./chafa8x8.py GenSVG')
    call(['./chafa8x8.py', 'GenSVG'])
    print('calling ./chafa8x8.py GenFont')
    call(['./chafa8x8.py', 'GenFont'])
    # experimental
    #print('calling ./chafa8x8.py GenBDF')
    #call(['./chafa8x8.py', 'GenBDF'])
    #call('bdftopcf -o chafa8x8.pcf chafa8x8.bdf'.split())



if __name__ == '__main__':
    eval(f'main{sys.argv[1]}')(sys.argv[2:])
