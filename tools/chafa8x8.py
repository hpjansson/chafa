#!/usr/bin/env python3
# This script is written for Chafa in order to generate custom fonts.
# Copyright (C) 2018 Mo Zhou <lumin@debian.org>
# License: LGPLv3+
import os, sys
import argparse
import glob
import json
import time
import random
import numpy as np
from tqdm import tqdm
from PIL import Image
from subprocess import call
from sklearn.cluster import KMeans, MiniBatchKMeans
from scipy.signal import convolve2d as conv2d

# Preserved Unicode Plane for Private use, see wikipedia for detail.
UNICODE_BASE = 0x100000


def bdump8x8(v: np.ndarray, *, clang=False, padding=0) -> str:
    '''
    dump the 8x8 bitmap
    '''
    assert(v.size == 64)
    dump = []
    for (i, v) in enumerate(v):
        if i%8==0 and clang: dump.append(' '*padding + "\"")
        if v:
            dump.append('X')
        else:
            dump.append(' ' if clang else '.')
        if (i+1)%8==0 and clang: dump.append("\"")
        if (i+1)%8==0: dump.append('\n')
    return ''.join(dump)


def mainCreateDataset(argv):
    '''
    Generate dataset used for generating chafa8x8 font from MS-COCO dataset
    http://cocodataset.org/
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--glob', type=str, default='~/coco/*.jpg')
    ag.add_argument('-Mc', type=int, default=16, help='num crops per image')
    ag.add_argument('-N', type=int, default=64, help='vector length')
    ag.add_argument('--save', type=str, default='chafa8x8.npz')
    ag = ag.parse_args(argv)

    if not os.path.exists(ag.save):
        # Glob images
        images = glob.glob(ag.glob, recursive=True)
        Mi = len(images)
        print(f'=> Found {Mi} images. Will sample {ag.Mc} vectors from each image')
        print(f' -> Dataset size M = {Mi*ag.Mc}')
        print(f' -> The resulting dataset takes {Mi*ag.Mc*ag.N/(1024**2)} MiB space.')

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
                region = region.resize((8,8))
                region = np.array(region).ravel()
                dataset[i*ag.Mc+c, :] = region
        # save the dataset as acche
        np.savez(ag.save, dataset=dataset)
    else:
        print(f'=> Dataset already generated: {ag.save}')


def mainClustering(argv):
    '''
    Run K-Means algorithm on the dataset. For large-scale training,
    MiniBatchKMeans will be much faster than KMeans.
    (This will still take some time if dataset is large enough)
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--dataset', type=str, default='chafa8x8.npz')
    ag.add_argument('--save', type=str, default='chafa8x8.raw.json')
    ag.add_argument('-C', type=int, default=5120, help='number of clusters')
    ag = ag.parse_args(argv)

    print(f'=> loading dataset from {ag.dataset}')
    dataset = np.load(ag.dataset)['dataset']

    # Findout the cluster centers with KMeans
    print('=> Clustering ...')
    kmeans = MiniBatchKMeans(n_clusters=ag.C, init='k-means++',
            init_size=37*ag.C, batch_size=8*ag.C,
            compute_labels=False, verbose=True).fit(dataset)
    centers = (kmeans.cluster_centers_ >= 0.5).astype(np.uint8)

    # Save the result to JSON file
    centers = list(sorted([list(map(int, center)) for center in centers],
                    key=lambda x:sum(x)))
    json.dump(centers, open(ag.save, 'w'))
    print(f'=> {ag.save}')


def mainPostproc(argv):
    '''
    Post-processing to make the glyphs "smoother" and do deduplication
    Default convolution kernel: Gaussian 3x3
    '''
    ag = argparse.ArgumentParser()
    ag.add_argument('--json', type=str, default='chafa8x8.raw.json')
    ag.add_argument('--save', type=str, default='chafa8x8.json')
    ag = ag.parse_args(argv)

    # load raw vectors
    centers = json.load(open(ag.json, 'r'))

    # [default kernel] Gaussian kernel (kernel size = 3)
    Kg = np.array([[1/16, 1/8, 1/16], [1/8, 1/4, 1/8], [1/16, 1/8, 1/16]])
    # mean value (glyphs tends to have round corner instead of sharp ones)
    Km = np.ones((3,3))/9
    # Select Kg as the default kernel
    K = Kg
    for (i, center) in enumerate(centers):
        center = np.array(center).reshape((8,8))
        center = conv2d(center, K, 'same').ravel()
        centers[i] = (center >= 0.5)
    centers = np.unique(np.array(centers), axis=0)
    print(' -> number of centers:', centers.shape[0])
    centers = list(sorted([list(map(int, center)) for center in centers],
                    key=lambda x:sum(x)))
    json.dump(centers, open(ag.save, 'w'))
    print(f'=> Result saved to {ag.save}')


def mainDump(argv):
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


def mainGenC(argv):
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


def mainGenSVG(argv):
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
     width="1000"
     viewBox="0 0 500 1000"><defs />
<g>
''')
        template = '<rect style="fill:#000000" width="{w}" height="{h}" x="{x}" y="{y}" />\n'
        center = np.array(center).reshape(8, 8).astype(np.uint8)
        indeces = np.argwhere(center > 0)
        for index in indeces:
            r, c = index
            f.write(template.format(w=62.5, h=127.5, x=0+c*62.5, y=-40+r*127.5))
        f.write(''' </g>\n</svg>''')
        f.close()


def mainGenFont(argv):
    call(['python2.7', 'svg2ttf.py'])


def mainGenA(argv):
    call(['./chafa8x8.py', 'Postproc'])
    call(['./chafa8x8.py', 'GenC'])
    call(['./chafa8x8.py', 'GenSVG'])
    call(['./chafa8x8.py', 'GenFont'])


if __name__ == '__main__':
    eval(f'main{sys.argv[1]}')(sys.argv[2:])
